#!/bin/bash

# exit when any command fails
set -e

clang --target=riscv32 -O3 -Wall -Werror system.c start.c -c
ld.lld -T system.ld start.o system.o -o system.elf
llvm-objcopy --output-target=binary system.elf system.bin
echo '@00000000' > system.vh
hexdump -v -e '4/4 "%08x " "\n"' system.bin >> rom.vh
