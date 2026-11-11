/*
 * Skrypt linkera dla obrazu ładowanego przez ROM ESP32-S3.
 *
 * Ten sam fizyczny SRAM widać pod dwoma adresami:
 *   IRAM (szyna instrukcji) 0x4037xxxx  <->  DRAM (szyna danych) 0x3FCxxxxx,
 *   przesunięcie = 0x6F0000.
 * Kod kładziemy w IRAM, dane w DRAM — w RÓŻNYCH fizycznych miejscach,
 * żeby się nie nadpisały. Unikamy górnej części DRAM (od 0x3FCD7E00),
 * której używa sam ROM (bufory, stos, .bss ROM-u).
 */
ENTRY(kernel_entry)

MEMORY
{
    iram (rx)  : ORIGIN = 0x40378000, LENGTH = 64K  /* fizycznie = DRAM 0x3FC88000..0x3FC97FFF */
    dram (rw)  : ORIGIN = 0x3FC98000, LENGTH = 64K  /* fizycznie za kodem, bez nakładania      */
}

SECTIONS
{
    .text : ALIGN(4)
    {
        *(.literal .text .literal.* .text.*)
    } > iram

    /* Stałe (np. napisy) MUSZĄ być w DRAM: odczyt pojedynczego bajtu
     * z IRAM daje na Xtensa wyjątek LoadStoreError. */
    .rodata : ALIGN(4)
    {
        *(.rodata .rodata.*)
    } > dram

    .data : ALIGN(4)
    {
        *(.data .data.*)
    } > dram

    .bss (NOLOAD) : ALIGN(4)
    {
        _bss_start = .;
        *(.bss .bss.* COMMON)
        . = ALIGN(4);
        _bss_end = .;
    } > dram
}
