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

static void uart_puts(const char *s)
{
    while (*s) {
        if (*s == '\n')
            uart_putc('\r');
        uart_putc(*s++);
    }
}

static void uart_puthex(uint32_t v)
{
    uart_puts("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        uart_putc("0123456789abcdef"[(v >> shift) & 0xF]);
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

    uart_puts("\n\n=== miniOS: hello z golego metalu (ESP32-S3) ===\n");
    uart_puts("kernel_entry @ "); uart_puthex((uint32_t)&kernel_entry); uart_puts("\n");
    uart_puts("data_var     = "); uart_puthex(data_var); uart_puts("\n");
    uart_puts("bss_var      = "); uart_puthex(bss_var); uart_puts("\n");

    uint32_t sp;
    __asm__ volatile ("mov %0, a1" : "=r"(sp));   /* a1 = wskaźnik stosu w ABI Xtensa */
    uart_puts("stos (a1)    = "); uart_puthex(sp); uart_puts("\n");

    uint32_t ccount_prev, ccount;
    __asm__ volatile ("rsr.ccount %0" : "=r"(ccount_prev)); /* licznik cykli CPU */
    for (uint32_t tick = 0;; tick++) {
        for (volatile uint32_t i = 0; i < 2000000; i++)
            ;
        __asm__ volatile ("rsr.ccount %0" : "=r"(ccount));
        uart_puts("tick "); uart_puthex(tick);
        uart_puts("  cykli/petla: "); uart_puthex(ccount - ccount_prev); uart_puts("\n");
        ccount_prev = ccount;
    }
}
