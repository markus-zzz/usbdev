#!/bin/bash

# exit when any command fails
set -e

clang --target=riscv32 -O3 -Wall -Werror system.c -c
llvm-mc --arch=riscv32 -assemble start.S --filetype=obj -o start.o
ld.lld -T system.ld start.o system.o -o system.elf
llvm-objcopy --only-section=.text --output-target=binary system.elf system.bin
echo '@00000000' > system.vh
hexdump -v -e '4/4 "%08x " "\n"' system.bin >> rom.vh
