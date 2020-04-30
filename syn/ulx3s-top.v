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

module ulx3s_top(
  input clk_25mhz,
  input [6:0] btn,
  output [7:0] led,
  inout [27:0] gp,
  inout [27:0] gn,
  inout usb_fpga_bd_dp,
  inout usb_fpga_bd_dn,
  output usb_fpga_pu_dp,
  output usb_fpga_pu_dn
);
  wire clk_15mhz;
  assign usb_fpga_pu_dp = 1'b0;
  assign led[4] = btn[6];
  assign led[3] = btn[6];
  assign led[2] = btn[6];
  assign led[1] = 1'b1;

  reg [1:0] pipe;

  assign gp[21] = pipe[0];
  assign gn[21] = pipe[1];

  wire usb_oe, usb_out_j_not_k, usb_out_se0;

  assign usb_fpga_bd_dp = usb_oe ? (usb_out_se0 ? 1'b0 : ~usb_out_j_not_k) : 1'bz; // low-speed
  assign usb_fpga_bd_dn = usb_oe ? (usb_out_se0 ? 1'b0 :  usb_out_j_not_k) : 1'bz;

  always @(posedge clk_25mhz)
    pipe <= {usb_fpga_bd_dp, usb_fpga_bd_dn};

  soc_top u_soc(
    .i_rst(btn[6]),
    .i_clk(clk_15mhz),
    .i_usb_j_not_k(usb_fpga_bd_dn), // low-speed
    .i_usb_se0(usb_fpga_bd_dp ~| usb_fpga_bd_dn),
    .o_usb_oe(usb_oe),
    .o_usb_j_not_k(usb_out_j_not_k),
    .o_usb_se0(usb_out_se0),
    .o_usb_attach(usb_fpga_pu_dn),
    .test_out(gp[22])
  );

  pll pll0(
    .clkin(clk_25mhz),
    .clkout0(clk_15mhz)
  );
endmodule
