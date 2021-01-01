#!/bin/bash

# exit when any command fails
set -e

clang --target=riscv32 -march=rv32ic -std=c99 -O3 -mno-relax -Wall -Werror usb-dev-driver.c -c
llvm-mc --arch=riscv32 -mcpu=generic-rv32 -mattr=+c -assemble start.S --filetype=obj -o start.o
ld.lld -T system.ld start.o usb-dev-driver.o -o usb-dev-driver.elf
llvm-objcopy --only-section=.text --output-target=binary usb-dev-driver.elf usb-dev-driver.bin
