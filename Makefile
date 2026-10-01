# Needs an ARM toolchain (devkitARM or gcc-arm-none-eabi) and gbafix (devkitPro).
PREFIX ?= arm-none-eabi-
CFLAGS = -mcpu=arm7tdmi -marm -mthumb-interwork -O2 -Wall -ffreestanding -fno-builtin -nostdlib
all: build/empire-ants.gba
build/empire-ants.gba: src/main.c src/art.h src/crt0.s gba.ld
	mkdir -p build
	$(PREFIX)gcc $(CFLAGS) -T gba.ld -Wl,--gc-sections src/crt0.s src/main.c -lgcc -o build/empire-ants.elf
	$(PREFIX)objcopy -O binary build/empire-ants.elf $@
	gbafix $@ -t"EMPIRE-ANTS" -cEANT
clean:
	rm -rf build
