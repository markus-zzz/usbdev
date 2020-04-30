# USB device implementation in Verilog

As the title reads this is a Verilog implementation of a USB device
(*low-speed* and *full-speed*) that I have been working on for a while.

## Prerequisites

This section lists the different tools that need to be installed to work on the
project. Only the first item is an absolute prerequsite - the rest depend on
what you want to do.

* Clang, LLD and LLVM tools with support for RISCV - Starting with LLVM release
 9.0.0 [RISCV is an officially supported
 target](https://releases.llvm.org/9.0.0/docs/ReleaseNotes.html#changes-to-the-riscv-target).
 Prebuilt binaries can be fetched from the [LLVM Download
 Page](https://releases.llvm.org/download.html). Installation, at least for our
 purposes, is as easy as extracting the archive and setting the `PATH`
 environment variable.

### Simulation

* Verilator - Verilog RTL simulatior.

### Synthesis

* SymbiFlow - Yosys and nextpnr for ECP5.

### Debugging

* Sigrok - sigrok-cli and PulseView.
* GtkWave - VCD waveform viewer.

## Quick start

 Register           | Address
--------------------|-----------
R_USB_ADDR          | 0x20000000
R_USB_ENDP_OWNER    | 0x20000004
R_USB_CTRL          | 0x20000008
R_USB_IN_SIZE_0_7   | 0x2000000C
R_USB_IN_SIZE_8_15  | 0x20000010
R_USB_DATA_TOGGLE   | 0x20000014
R_USB_OUT_SIZE_0_7  | 0x20000018
R_USB_OUT_SIZE_8_15 | 0x2000001C



### R_USB_ADDR (R/W)
 Bits | Name | Description
------|------|------------
 6-0  | addr | Address of this USB device.

### R_USB_ENDP_OWNER (R/W)
 Bits | Name | Description
------|------|------------
   0  | in0  | 1 - IN endpoint 0 is owned by USB and not CPU. Writing 1 hands over endpoint from CPU to USB. Writing 0 has no effect.
   1  | in1  | 1 - IN endpoint 0 is owned by USB and not CPU. Writing 1 hands over endpoint from CPU to USB. Writing 0 has no effect.

sigrok-cli --input-file sofs.sr -O csv | awk 'BEGIN{FS=","}{print $2$3}' - > sofs.vh
```
```
sigrok-cli -i setup.sr -P usb_signalling:dp=1:dm=0,usb_packet,usb_request
sigrok-cli -i setup-trimmed.sr -P usb_signalling:dp=1:dm=0 | ./usb_signalling2vh.py > setup.vh
sigrok-cli -i mytx.csv -I csv:samplerate=48000000 -P usb_signalling:dp=1:dm=0
```
