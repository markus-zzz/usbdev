# [WIP] USB device implementation in Verilog [WIP]

As the title reads this is a, work-in-progress, Verilog implementation of a USB
device (*low-speed* and *full-speed*) that I have been working on for a while.

## Prerequisites

This section lists the different tools that need to be installed to work on the
project. Only the first item is an absolute prerequisite, the rest depend on
what you want to do.

* Clang, LLD and LLVM tools with support for RISCV - Starting with LLVM release
 9.0.0 [RISCV is an officially supported
 target](https://releases.llvm.org/9.0.0/docs/ReleaseNotes.html#changes-to-the-riscv-target).
 Prebuilt binaries can be fetched from the [LLVM Download
 Page](https://releases.llvm.org/download.html). Installation, at least for our
 purposes, is as easy as extracting the archive and setting the `PATH`
 environment variable.

### Simulation

* [Verilator](https://www.veripool.org/wiki/verilator) - Verilog RTL simulatior.

### Synthesis

* [SymbiFlow](https://symbiflow.github.io/) - Yosys and nextpnr for ECP5.

### Debugging

* [Sigrok](https://sigrok.org/) - sigrok-cli and PulseView.
* [GtkWave](http://gtkwave.sourceforge.net/) - VCD waveform viewer.

## Quick start

To build the simulation environment and run the test-suite
```
cd sim
./build-verilator.sh
./runner.pl
```
Then the USB traffic for a test-case can be decoded by Sigrok's USB packet
decoder. E.g.
```
sigrok-cli -i test_003.sim.sr -P usb_signalling:dp=0:dm=1,usb_packet | awk '/usb_packet-1: [^:]+$/{ print $0 }'
```
should result in (but if not also try decoding with `dp=1:dm=0`)
```
usb_packet-1: OUT ADDR 0 EP 0
usb_packet-1: DATA0 [ 23 64 54 AF CA FE ]
usb_packet-1: ACK
usb_packet-1: IN ADDR 0 EP 1
usb_packet-1: NAK
usb_packet-1: IN ADDR 0 EP 1
usb_packet-1: DATA0 [ 24 65 55 B0 CB FF ]
usb_packet-1: ACK
usb_packet-1: IN ADDR 0 EP 1
usb_packet-1: NAK
```
