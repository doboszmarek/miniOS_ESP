# Gotowe obrazy

Skompilowane obrazy kolejnych kroków, do wgrania bez stawiania toolchaina.
Każdy wgrywa się pod `0x0` (zastępuje bootloader ESP-IDF):

```
python -m esptool --chip esp32s3 -p COM6 -b 460800 write-flash 0x0 bin\01-hello.bin
python -m serial.tools.miniterm COM6 115200
```

| Plik | Krok |
|---|---|
| `01-hello.bin` | 1: hello z gołego metalu, konsola na UART0 + USB-Serial/JTAG |
