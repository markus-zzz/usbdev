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

```
cd sim
./build-verilator.sh #Will need some editing depending on your Verilator installation
./runner.pl
```
