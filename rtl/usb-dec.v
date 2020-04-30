/*
 * Copyright (C) 2019-2020 Markus Lavin (https://www.zzzconsulting.se/)
 *
 * All rights reserved.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

`default_nettype none

module usb_bit_decode(
  input i_clk_48mhz,
  input i_bit,
  input i_bit_en,
  output o_bit,
  output o_bit_en
);
  // NRZI decode (is XNOR of last two incomming bits).
  wire nrzi_dec;
  assign nrzi_dec = bit_p ~^ i_bit;
  reg bit_p;
  always @(posedge i_clk_48mhz) begin
    if (i_bit_en) bit_p <= i_bit;
  end

  // Bit de-stuffing (after every six one bits a zero bit is insered so we need
  // to discard that bit).
  reg [2:0] ones_cntr;

  initial begin
    ones_cntr = 0;
  end

  wire six_ones;
  assign six_ones = (ones_cntr == 3'h6);

  always @(posedge i_clk_48mhz) begin
    if (i_bit_en) begin
      if (six_ones | ~nrzi_dec)
        ones_cntr <= 0;
      else
        ones_cntr <= ones_cntr + 1;
    end
  end

  // After 6 one bits the next bit will be an inserted zero so discard that.
  assign o_bit_en = i_bit_en & ~six_ones;
  assign o_bit = nrzi_dec;
endmodule
