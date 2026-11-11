# Krok 1: hello z gołego metalu

## Co się dzieje po resecie

1. ROM czyta nagłówek obrazu spod `0x0` (magic `0xE9`, liczba segmentów, adres wejścia).
2. Kopiuje segmenty do SRAM: `.rodata`/`.data` → DRAM `0x3FC98000`, `.text` → IRAM `0x40378000`.
3. Sprawdza sumę kontrolną i SHA-256, po czym skacze do `kernel_entry`.

## Mapa pamięci

IRAM `0x4037xxxx` i DRAM `0x3FCxxxxx` to **ten sam fizyczny SRAM** (przesunięcie `0x6F0000`):

| Obszar | Adres | Fizycznie (DRAM) |
|---|---|---|
| `.text` | IRAM 0x40378000 – 0x40387FFF | 0x3FC88000 – 0x3FC97FFF |
| `.rodata/.data/.bss` | DRAM 0x3FC98000 – 0x3FCA7FFF | ten sam |
| stos ROM-u (używamy go w kroku 1) | 0x3FCE9710 – 0x3FCEB710 | |
| zarezerwowane przez ROM | od 0x3FCD7E00 w górę | |

## Konsola: UART0 + USB-Serial/JTAG

Każdy znak idzie na dwa wyjścia:
- **UART0** (GPIO43/44, port „COM/UART” płytki, przez mostek USB-UART),
- **USB-Serial/JTAG** (natywny port „USB”, VID 303A:1001). Bajty trafiają do 64-bajtowego
  bufora EP1 (`0x60038000`), a bit `WR_DONE` w `EP1_CONF` (`0x60038004`) wysyła paczkę do PC.
  Jeśli PC nie odbiera, czekamy z limitem czasu i gubimy znaki, zamiast zawiesić system.

Wystarczy więc jeden kabel w porcie „USB”: flashowanie, reset i podgląd idą tą samą drogą.

## Pułapki

- **Napisy w DRAM, nie w IRAM.** Odczyt bajtu z IRAM daje wyjątek `LoadStoreError`.
- **`.bss` zerujemy sami.** ROM kopiuje tylko segmenty z zawartością.
- **Watchdogi:** RTC WDT i MWDT wyłączamy po odblokowaniu kluczem `0x50D83AA1`.
  Super Watchdoga (SWD) nie da się wyłączyć, ustawiamy mu tylko auto-feed (klucz `0x8F1D312A`).
- **Wejście okienkowe:** ROM wywołuje nas jak zwykłą funkcję z ABI okienkowym,
  więc `kernel_entry` może być funkcją w C (zaczyna się od instrukcji `entry`).

## Wynik w QEMU

```
load:0x3fc98000,len:0xa4
load:0x40378000,len:0x1fc
entry 0x403780d8

=== miniOS: hello z golego metalu (ESP32-S3) ===
kernel_entry @ 0x403780d8
data_var     = 0x00c0ffee
bss_var      = 0x00000000
stos (a1)    = 0x3fceb540
tick 0x00000000  cykli/petla: 0x001fd867
...
```

Na prawdziwym S3 wartość „cykli/pętla” powie nam, z jakim taktowaniem zostawił nas ROM.
Liczba cykli na pętlę jest stała, więc różnicę zobaczysz w czasie między tickami.
QEMU nie odwzorowuje taktowania dokładnie.

## Do sprawdzenia samemu

1. `make info`: porównaj segmenty z sekcjami ELF.
2. `xtensa-esp32s3-elf-objdump -d hello.elf | less`: znajdź `entry a1, ...` na początku `kernel_entry`.
3. Zakomentuj `watchdogs_off()` i wgraj na płytkę: po kilku sekundach zobaczysz reset od watchdoga (`rst:0x...`).
