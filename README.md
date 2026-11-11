# miniOS na ESP32-S3

Projekt edukacyjny: własny system operacyjny pisany od zera na ESP32-S3 (Xtensa LX7),
bez ESP-IDF i FreeRTOS. Cel: zrozumieć wnętrze systemu, a nie zbudować praktyczną platformę.

Obraz jądra ładuje bezpośrednio **ROM bootloader** spod `0x0` we flashu, czyli zastępujemy
bootloader drugiego etapu ESP-IDF.

## Plan (każdy krok to osobny katalog z działającym kodem)

| Krok | Katalog | Zakres |
|---|---|---|
| 1 | `01-hello/` | skrypt linkera, punkt wejścia, wyłączenie watchdogów, UART0 przez rejestry |
| 2 | | wyjątki i przerwania: tablica wektorów, własny stos, timer, czytelny `panic` |
| 3 | | przełączanie kontekstu: dwa zadania naprzemiennie (asembler Xtensa) |
| 4 | | scheduler: priorytety, `sleep`, zadanie idle |
| 5 | | synchronizacja: semafory, producent–konsument |
| 6 | | shell po UART: `ps`, `mem` |
| 7+ | | drugi rdzeń (SMP), MMU i kod z flasha (XIP), ładowanie programów |

Świadome ograniczenia: bez Wi-Fi/BT (zamknięte sterowniki wymagają IDF/FreeRTOS),
bez pełnej izolacji procesów (S3 nie ma MMU z tablicami stron, tylko PMS / World Controller).

## Narzędzia

- Toolchain: `xtensa-esp32s3-elf-gcc` (z ESP-IDF albo
  [crosstool-NG Espressif](https://github.com/espressif/crosstool-NG/releases), testowane na esp-14.2.0_20241119)
- Emulator: [QEMU Espressif](https://github.com/espressif/qemu/releases) (`qemu-system-xtensa -machine esp32s3`),
  testowane na esp-develop-9.0.0-20240606
- `pip install esptool` (v5)

## Wgranie na płytkę (Windows)

Gotowe obrazy leżą w `bin/`, więc toolchain nie jest potrzebny. W katalogu repo:

```
flash            :: najnowszy obraz, port COM6, potem monitor
flash 01         :: konkretny krok (bin\01-*.bin)
flash 01 COM7    :: inny port
```

Domyślny port można zmienić na stałe: `setx MINIOS_PORT COM7`. Z monitora wychodzi się przez Ctrl+].

## Budowanie na Windows

Jednorazowo: toolchain do katalogu `tools\` w repo (ok. 370 MB, ignorowany przez Git):

```
cd /d D:\CODING\miniOS\miniOS
mkdir tools
curl -L -o tools\tc.zip https://github.com/espressif/crosstool-NG/releases/download/esp-14.2.0_20241119/xtensa-esp-elf-14.2.0_20241119-x86_64-w64-mingw32.zip
tar -xf tools\tc.zip -C tools
del tools\tc.zip
```

Potem:

```
build 01         :: 01-hello\*.c + link.ld  ->  bin\01-hello.bin
flash 01
```

ELF i mapa linkera lądują w `build\01-hello\`. Starą wersję obrazu przywrócisz przez `git checkout bin`.

## Budowanie i uruchomienie kroku (Linux, make, QEMU)

```sh
cd 01-hello
make            # hello.elf -> hello.bin (esptool elf2image)
make qemu       # uruchom w emulatorze (Ctrl-A X kończy)
make flash PORT=/dev/ttyUSB0   # wgraj na płytkę pod 0x0 (nadpisuje bootloader IDF)
make monitor PORT=/dev/ttyUSB0
make info       # segmenty obrazu i sekcje ELF
```

Na płytce używaj portu **UART0** (GPIO43 TX / GPIO44 RX). Port natywnego USB-Serial/JTAG
pokaże tylko logi ROM-u. Powrót do ESPHome/IDF: wgraj ponownie pełny firmware od `0x0`.
