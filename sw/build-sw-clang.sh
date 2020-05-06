#!/bin/bash

# exit when any command fails
set -e

clang --target=riscv32 -O3 -Wall -Werror usb-dev-driver.c -c
llvm-mc --arch=riscv32 -assemble start.S --filetype=obj -o start.o
ld.lld -T system.ld start.o usb-dev-driver.o -o usb-dev-driver.elf
llvm-objcopy --only-section=.text --output-target=binary usb-dev-driver.elf usb-dev-driver.bin
hexdump -v -e '4/4 "%08x " "\n"' usb-dev-driver.bin > rom.vh
