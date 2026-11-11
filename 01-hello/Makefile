# Wymaga: xtensa-esp32s3-elf-gcc w PATH (z ESP-IDF lub crosstool-NG Espressif) oraz esptool
CC      = xtensa-esp32s3-elf-gcc
OBJDUMP = xtensa-esp32s3-elf-objdump
CFLAGS  = -Os -g -Wall -Wextra -ffreestanding -nostdlib -mlongcalls
LDFLAGS = -nostdlib -T link.ld -Wl,-Map=hello.map -Wl,--gc-sections

PORT ?= /dev/ttyUSB0

all: hello.bin

hello.elf: hello.c link.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ hello.c

hello.bin: hello.elf
	esptool --chip esp32s3 elf2image --flash-mode dio --flash-freq 80m --flash-size 4MB -o $@ $<

# 4 MB obraz flasha dla QEMU (nasz obraz pod 0x0, reszta 0xFF)
flash.img: hello.bin
	esptool --chip esp32s3 merge-bin --pad-to-size 4MB -o $@ 0x0 $<

qemu: flash.img
	qemu-system-xtensa -nographic -machine esp32s3 -drive file=flash.img,if=mtd,format=raw

flash: hello.bin
	esptool --chip esp32s3 -p $(PORT) -b 460800 write-flash 0x0 hello.bin

monitor:
	python -m serial.tools.miniterm $(PORT) 115200

info: hello.bin
	esptool image-info hello.bin
	$(OBJDUMP) -h hello.elf

clean:
	rm -f hello.elf hello.bin hello.map flash.img
.PHONY: all qemu flash monitor info clean
