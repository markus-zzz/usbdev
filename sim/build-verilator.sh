#!/bin/bash

set -e

rm -rf obj_dir

python3 ../rtl/usb_subsys.py generate > usb_subsys.v

verilator -trace -cc usb_subsys.vlt usb_subsys.v ../rtl/verilog/picorv32.v +1364-2005ext+v --top-module usb_subsys -Wno-fatal -comp-limit-members 1024
VERILATOR_ROOT=/usr/share/verilator/
cd obj_dir; make -f Vusb_subsys.mk; cd ..
g++ -std=c++14 verilator-main.cpp usb-pack-gen.cpp obj_dir/Vusb_subsys__ALL.a -I obj_dir/ -I $VERILATOR_ROOT/include/ -I $VERILATOR_ROOT/include/vltstd $VERILATOR_ROOT/include/verilated.cpp $VERILATOR_ROOT/include/verilated_vcd_c.cpp -I. -o usb-sim -O0 -g3 -ldl -Wl,--export-dynamic
