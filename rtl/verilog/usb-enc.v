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

module usb_bit_encode(
  input i_clk_48mhz,
  input [7:0] i_byte,
  output o_reload,
  input i_restart,
  input  i_enc_en,
  output o_enc_bit
);

  reg [7:0] shift;
  reg [2:0] shift_cntr;
  reg [2:0] ones_cntr;
  reg prev_enc_bit;

  wire six_ones;
  assign six_ones = (ones_cntr == 3'h6);

  wire stuffed_bit;
  assign stuffed_bit = six_ones ? 1'b0 : shift[0];

  always @(posedge i_clk_48mhz) begin
    if (i_enc_en) begin
      if (six_ones | ~shift[0])
        ones_cntr <= 0;
      else
        ones_cntr <= ones_cntr + 1;
    end
  end

  always @(posedge i_clk_48mhz) begin
    if (i_restart) begin
      shift <= 8'b1000_0000;
      shift_cntr <= 0;
    end
    else if (i_enc_en & ~six_ones) begin
      shift <= (shift_cntr == 7) ? i_byte : {1'b0, shift[7:1]};
      shift_cntr <= shift_cntr + 1;
    end
  end

  always @(posedge i_clk_48mhz) begin
    if (i_restart) begin
      prev_enc_bit <= 1'b1; // Idle J state.
    end
    else if (i_enc_en) begin
      prev_enc_bit <= o_enc_bit;
    end
  end

  // NRZI encoding - 1'b1 no change in level, 1'b0 change in level.
  assign o_enc_bit = stuffed_bit ? prev_enc_bit : ~prev_enc_bit;
  assign o_reload = i_enc_en & ~six_ones && (shift_cntr == 3'h7);
endmodule
