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


class UsbBitEncoder(Elaboratable):
    def __init__(self):
        self.i_byte = Signal(8)
        self.o_reload = Signal()
        self.i_restart = Signal()
        self.i_enc_en = Signal()
        self.o_enc_bit = Signal()

    def elaborate(self, platform):
        m = Module()

        shift = Signal(8)
        shift_cntr = Signal(range(0, 7))
        ones_cntr = Signal(range(0, 6))
        prev_enc_bit = Signal()
        six_ones = Signal()
        stuffed_bit = Signal()

        m.d.comb += [
            self.o_reload.eq(0),
            # NRZI encoding - 1'b1 no change in level, 1'b0 change in level.
            self.o_enc_bit.eq(Mux(stuffed_bit, prev_enc_bit, ~prev_enc_bit)),
            six_ones.eq(ones_cntr == 6),
            stuffed_bit.eq(Mux(six_ones, 0, shift[0]))
        ]

        with m.If(self.i_enc_en):
            m.d.sync += ones_cntr.eq(
                Mux(six_ones | ~shift[0], 0, ones_cntr + 1))

        with m.If(self.i_restart):
            # Prepare new sync-pattern. Idle state is J.
            m.d.sync += [shift.eq(0x80), shift_cntr.eq(0), prev_enc_bit.eq(1)]
        with m.Elif(self.i_enc_en):
            m.d.sync += prev_enc_bit.eq(self.o_enc_bit)
            with m.If(~six_ones):
                m.d.sync += shift_cntr.eq(shift_cntr + 1)
                with m.If(shift_cntr == 7):
                    m.d.sync += shift.eq(self.i_byte)
                    m.d.comb += self.o_reload.eq(
                        1)  # Should be called o_byte_ack instead of o_reload?
                with m.Else():
                    m.d.sync += shift.eq(Cat(shift[1:8], C(0, 1)))
        return m


if __name__ == "__main__":
    dec = UsbBitDecoder()
    main(dec,
         name="usb_bit_decode_2",
         ports=[dec.i_bit, dec.i_bit_en, dec.o_bit, dec.o_bit_en])
    enc = UsbBitEncoder()
    main(enc,
         name="usb_bit_encode_2",
         ports=[
             enc.i_byte, enc.o_reload, enc.i_restart, enc.i_enc_en,
             enc.o_enc_bit
         ])
