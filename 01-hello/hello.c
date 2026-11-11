/*
 * miniOS na ESP32-S3 — krok 1: "hello" z gołego metalu.
 *
 * Ten obraz ładuje bezpośrednio ROM bootloader (spod 0x0 we flashu),
 * bez bootloadera ESP-IDF, bez FreeRTOS, bez żadnej biblioteki.
 * Wszystko robimy zapisami do rejestrów sprzętowych.
 */
#include <stdint.h>

/* Dostęp do rejestru 32-bitowego. 'volatile' = kompilator nie może
 * wyrzucić ani przestawić tych odczytów/zapisów, bo mają efekt uboczny w sprzęcie. */
#define REG(addr) (*(volatile uint32_t *)(addr))

/* ---- Adresy bazowe peryferiów (soc/esp32s3/include/soc/reg_base.h) ---- */
#define UART0_BASE   0x60000000u
#define RTCCNTL_BASE 0x60008000u
#define TIMG0_BASE   0x6001F000u
#define TIMG1_BASE   0x60020000u

/* ---- UART0 ---- */
#define UART_FIFO    (UART0_BASE + 0x00)  /* zapis bajtu = wrzucenie do kolejki TX  */
#define UART_STATUS  (UART0_BASE + 0x1C)  /* bity [25:16] = ile bajtów czeka w TX FIFO */
#define UART_TXFIFO_CNT(st) (((st) >> 16) & 0x3FF)
#define UART_FIFO_SIZE 128

/* ---- Watchdog RTC (RWDT) + Super Watchdog (SWD) ---- */
#define RTC_WDTCONFIG0   (RTCCNTL_BASE + 0x98)
#define RTC_WDTWPROTECT  (RTCCNTL_BASE + 0xB0)
#define RTC_SWD_CONF     (RTCCNTL_BASE + 0xB4)
#define RTC_SWD_WPROTECT (RTCCNTL_BASE + 0xB8)
#define RTC_SWD_AUTO_FEED_EN (1u << 31)

/* ---- Watchdog grupy timerów (MWDT) ---- */
#define TIMG_WDTCONFIG0(base)  ((base) + 0x48)
#define TIMG_WDTWPROTECT(base) ((base) + 0x64)

/* Klucze "odblokowania zapisu" — bez nich rejestry WDT ignorują zapisy. */
#define WDT_WKEY 0x50D83AA1u
#define SWD_WKEY 0x8F1D312Au

static void watchdogs_off(void)
{
    /* RTC WDT: odblokuj, wyzeruj konfigurację (bit 31 = enable), zablokuj z powrotem. */
    REG(RTC_WDTWPROTECT) = WDT_WKEY;
    REG(RTC_WDTCONFIG0)  = 0;
    REG(RTC_WDTWPROTECT) = 0;

    /* Super watchdog nie da się wyłączyć — można go tylko "karmić" automatycznie. */
    REG(RTC_SWD_WPROTECT) = SWD_WKEY;
    REG(RTC_SWD_CONF)    |= RTC_SWD_AUTO_FEED_EN;
    REG(RTC_SWD_WPROTECT) = 0;

    /* MWDT w obu grupach timerów (ROM włącza ten w TIMG0). */
    const uint32_t groups[] = { TIMG0_BASE, TIMG1_BASE };
    for (unsigned i = 0; i < 2; i++) {
        REG(TIMG_WDTWPROTECT(groups[i])) = WDT_WKEY;
        REG(TIMG_WDTCONFIG0(groups[i]))  = 0;
        REG(TIMG_WDTWPROTECT(groups[i])) = 0;
    }
}

static void uart_putc(char c)
{
    /* Czekamy, aż w 128-bajtowej kolejce TX będzie miejsce.
     * Prędkość (115200) i piny UART0 zostawił skonfigurowane ROM. */
    while (UART_TXFIFO_CNT(REG(UART_STATUS)) >= UART_FIFO_SIZE - 1)
        ;
    REG(UART_FIFO) = (uint8_t)c;
}

/* ---- USB-Serial/JTAG (natywny port "USB", na Windows np. COM6) ----
 * Wbudowany w S3 kontroler USB widziany przez PC jako port szeregowy CDC-ACM.
 * Enumeracją USB zajmuje się sprzęt, my tylko wrzucamy bajty do 64-bajtowego
 * bufora punktu końcowego EP1 i mówimy "wyślij" (WR_DONE).
 * Gdy po stronie PC nikt nie czyta portu, bufor się nie opróżnia — dlatego
 * czekamy z limitem czasu i wtedy po prostu gubimy znaki, zamiast zawiesić system. */
#define USJ_BASE     0x60038000u
#define USJ_EP1      (USJ_BASE + 0x0)      /* zapis bajtu do bufora IN (do PC) */
#define USJ_EP1_CONF (USJ_BASE + 0x4)
#define USJ_WR_DONE       (1u << 0)        /* zapis 1 = wyślij to, co jest w buforze */
#define USJ_IN_DATA_FREE  (1u << 1)        /* 1 = w buforze jest miejsce */

static int usj_dead;  // w .bss   /* 1 = host nie odbiera, nie tracimy czasu na czekanie */

static void usj_putc(char c)
{
    if (!(REG(USJ_EP1_CONF) & USJ_IN_DATA_FREE)) {
        if (usj_dead)
            return;                         /* dalej nikt nie słucha */
        REG(USJ_EP1_CONF) = USJ_WR_DONE;    /* bufor pełny: wypchnij go */
        uint32_t timeout = 200000;
        while (!(REG(USJ_EP1_CONF) & USJ_IN_DATA_FREE))
            if (--timeout == 0) {
                usj_dead = 1;
                return;
            }
    }
    usj_dead = 0;
    REG(USJ_EP1) = (uint8_t)c;
    if (c == '\n')
        REG(USJ_EP1_CONF) = USJ_WR_DONE;    /* koniec linii: wyślij od razu */
}

/* Konsola = ten sam znak na oba wyjścia. */
static void console_putc(char c)
{
    uart_putc(c);
    usj_putc(c);
}

static void console_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            console_putc('\r');
        console_putc(*s++);
    }
}

static void console_puthex(uint32_t v)
{
    console_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        console_putc("0123456789abcdef"[(v >> shift) & 0xF]);
}

/* Symbole z linker scriptu: granice sekcji. */
extern uint32_t _bss_start, _bss_end;

/* Licznik do sprawdzenia, że .data i .bss naprawdę działają. */
volatile uint32_t data_var = 0xC0FFEE;   // trafi do .data (ROM skopiuje wartość)
volatile uint32_t bss_var;               // trafi do .bss (my zerujemy)

/*
 * Punkt wejścia. ROM skacze tutaj (adres wpisany w nagłówek obrazu) instrukcją
 * wywołania okienkowego, więc zwykła funkcja C (która zaczyna się od 'entry')
 * jest poprawnym punktem wejścia. Na razie działamy na stosie ROM-u
 * (0x3FCE9710–0x3FCEB710); własny stos ustawimy w kroku 2.
 */
void __attribute__((noreturn)) kernel_entry(void)
{
    /* ROM kopiuje tylko segmenty z danymi — .bss musimy wyzerować sami. */
    for (uint32_t *p = &_bss_start; p < &_bss_end; p++)
        *p = 0;

    watchdogs_off();

    console_puts("\n\n=== miniOS: hello z golego metalu (ESP32-S3) ===\n");
    console_puts("kernel_entry @ "); console_puthex((uint32_t)&kernel_entry); console_puts("\n");
    console_puts("data_var     = "); console_puthex(data_var); console_puts("\n");
    console_puts("bss_var      = "); console_puthex(bss_var); console_puts("\n");

    uint32_t sp;
    __asm__ volatile ("mov %0, a1" : "=r"(sp));   /* a1 = wskaźnik stosu w ABI Xtensa */
    console_puts("stos (a1)    = "); console_puthex(sp); console_puts("\n");

    uint32_t ccount_prev, ccount;
    __asm__ volatile ("rsr.ccount %0" : "=r"(ccount_prev)); /* licznik cykli CPU */
    for (uint32_t tick = 0;; tick++) {
        for (volatile uint32_t i = 0; i < 2000000; i++)
            ;
        __asm__ volatile ("rsr.ccount %0" : "=r"(ccount));
        console_puts("tick "); console_puthex(tick);
        console_puts("  cykli/petla: "); console_puthex(ccount - ccount_prev); console_puts("\n");
        ccount_prev = ccount;
    }
}
