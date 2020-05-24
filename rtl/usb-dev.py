#
# Copyright (C) 2019-2020 Markus Lavin (https://www.zzzconsulting.se/)
#
# All rights reserved.
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

from nmigen import *
from nmigen.cli import main


class UsbBitDecoder(Elaboratable):
    def __init__(self):
        self.i_bit = Signal()
        self.i_bit_en = Signal()
        self.o_bit = Signal()
        self.o_bit_en = Signal()

    def elaborate(self, platform):
        m = Module()
        bit_p = Signal()
        ones_cntr = Signal(range(0, 6))
        # NRZI decode (is XNOR of last two incomming bits).
        m.d.comb += [self.o_bit.eq(~(bit_p ^ self.i_bit)), self.o_bit_en.eq(0)]
        with m.If(self.i_bit_en):
            m.d.sync += bit_p.eq(self.i_bit)
            with m.If((ones_cntr == 6) | ~self.o_bit):
                m.d.sync += ones_cntr.eq(0)
            with m.Else():
                m.d.sync += ones_cntr.eq(ones_cntr + 1)
            with m.If(ones_cntr != 6):
                m.d.comb += self.o_bit_en.eq(1)
        return m


if __name__ == "__main__":
    dec = UsbBitDecoder()
    main(dec,
         name="usb_bit_decode_2",
         ports=[dec.i_bit, dec.i_bit_en, dec.o_bit, dec.o_bit_en])
