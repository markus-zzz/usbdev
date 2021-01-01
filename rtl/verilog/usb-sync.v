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

// This module continuously adjusts the sampling position to be in the middle
// of a bit period. It does so by keeping track of the four past samples and
// either advances or delays the next sampling position by one 4x clock
module sample_pos_adj(
  input i_clk_4x,
  input i_data,
  output o_sample_en
);

//
// TODO:FIXME: Really need to write unit test for this module !!!!!
//
`ifdef USB_FULL_SPEED
  reg [1:0] cntr;
  reg [4:0] past;
  wire delay;   // Move sampling position to a later time.
  wire advance; // Move sampling position to an earlier time.
  wire cntr_is_zero;
  reg cntr_is_zero_p;

  assign o_sample_en = cntr_is_zero & ~cntr_is_zero_p;
  assign cntr_is_zero = (cntr == 2'b00);

  always @(posedge i_clk_4x)
    cntr_is_zero_p <= cntr_is_zero;

  // A bit transition should ideally occur between past[2] and past[1]. If it
  // occurs elsewhere we are either sampling too early or too late.
  assign advance = (past[3] ^ past[2]) | (past[4] ^ past[3]);
  assign delay   = past[1] ^ past[0];

  always @(posedge i_clk_4x) begin
    past <= {past[3:0], i_data};
  end

  initial begin
    cntr = 0;
  end

  always @(posedge i_clk_4x) begin
    if (cntr == 0 && advance) begin
      cntr <= cntr + 2;
      $display("%t: samp advance", $time);
    end
    else if (cntr == 0 && delay) begin
      cntr <= cntr + 0;
      $display("%t: samp delay", $time);
    end
    else begin
      cntr <= cntr + 1;
    end
  end
`else
  reg [7:0] cntr;
  initial begin
    cntr = 0;
  end

  always @(posedge i_clk_4x) begin
    if (cntr == 9)
      cntr <= 0;
    else
      cntr <= cntr + 1;
  end

  assign o_sample_en = cntr == 0;
`endif
endmodule
