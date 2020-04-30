#!/bin/bash

set -e -x
yosys usb-dev.ys
nextpnr-ecp5 \
	--json usb-dev.json \
	--textcfg usb-dev.config \
	--lpf ulx3s.lpf \
	--25k

ecppack --idcode 0x21111043 usb-dev.config usb-dev.bit
