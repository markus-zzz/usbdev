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

module soc_top(
  input i_rst,
  input i_clk,
  input i_usb_j_not_k,
  input i_usb_se0,
  output o_usb_oe,
  output o_usb_j_not_k,
  output o_usb_se0,
  output o_usb_attach,
  output test_out
);

  wire cpu_mem_valid;
  wire cpu_mem_instr;
  reg  cpu_mem_ready;
  wire [31:0] cpu_mem_addr;
  wire [31:0] cpu_mem_wdata;
  wire [ 3:0] cpu_mem_wstrb;
  reg [31:0] cpu_mem_rdata;
  wire [31:0] ram_rdata, rom_rdata;
  wire [31:0] usb_reg_rdata;

  wire [9:0] ram_addr;
  wire [31:0] ram_wdata;
  wire [3:0] ram_wstrb;
 
  wire usb_ram_access;
  assign usb_ram_access = usb_mem_ren | usb_mem_wen;

  assign ram_addr  = usb_ram_access ? usb_mem_addr : cpu_mem_addr[9:0];
  assign ram_wdata = usb_ram_access ? {4{usb_mem_wdata}} : cpu_mem_wdata;
  assign ram_wstrb = usb_ram_access ? {3'b000, usb_mem_wen} << ram_addr[1:0] : cpu_mem_wstrb;

  always @* begin
    case (cpu_mem_addr[31:28])
      4'h0: cpu_mem_rdata = rom_rdata;
      4'h1: cpu_mem_rdata = ram_rdata;
      default: cpu_mem_rdata = usb_reg_rdata;
    endcase
  end

  wire clk, rst;
  assign clk = i_clk;
  assign rst = i_rst;

  always @(posedge clk) begin
    if (rst) cpu_mem_ready <= 0;
    else begin
      case (cpu_mem_addr[31:28])
        4'h0: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        4'h1: cpu_mem_ready <= ~usb_ram_access & ~cpu_mem_ready & cpu_mem_valid;
        4'h2: cpu_mem_ready <= ~cpu_mem_ready & cpu_mem_valid;
        default: cpu_mem_ready <= cpu_mem_ready;
      endcase
    end
  end

  // ROM - CPU code.
  sprom #(
    .aw(12),
    .dw(32),
    .MEM_INIT_FILE("rom.vh")
  ) u_rom(
    .clk(clk),
    .rst(rst),
    .ce(cpu_mem_valid && cpu_mem_addr[31:28] == 4'h0),
    .oe(1'b1),
    .addr(cpu_mem_addr[13:2]),
    .do(rom_rdata)
  );

  // RAM - shared between CPU and USB. USB has priority.
  genvar gi;
  generate
    for (gi=0; gi<4; gi=gi+1) begin
      spram #(
        .aw(10),
        .dw(8)
      ) u_ram(
        .clk(clk),
        .rst(rst),
        .ce(usb_ram_access || (cpu_mem_valid && cpu_mem_addr[31:28] == 4'h1)),
        .oe(1'b1),
        .addr(ram_addr[9:2]),
        .do(ram_rdata[(gi+1)*8-1:gi*8]),
        .di(ram_wdata[(gi+1)*8-1:gi*8]),
        .we(ram_wstrb[gi])
      );
    end
  endgenerate

  picorv32 u_cpu(
    .clk(clk),
    .resetn(~rst),
    .mem_valid(cpu_mem_valid),
    .mem_instr(cpu_mem_instr),
    .mem_ready(cpu_mem_ready),
    .mem_addr(cpu_mem_addr),
    .mem_wdata(cpu_mem_wdata),
    .mem_wstrb(cpu_mem_wstrb),
    .mem_rdata(cpu_mem_rdata)
  );

  wire usb_mem_wen;
  wire usb_mem_ren;
  wire [9:0] usb_mem_addr;
  wire [7:0] usb_mem_wdata;
  reg [7:0] usb_mem_rdata;

  usb_dev u_usb_dev(
    .i_rst(rst),
    .i_clk_48mhz(clk),
    .i_usb_j_not_k(i_usb_j_not_k),
    .i_usb_se0(i_usb_se0),
    .o_usb_oe(o_usb_oe),
    .o_usb_j_not_k(o_usb_j_not_k),
    .o_usb_se0(o_usb_se0),
    .o_usb_attach(o_usb_attach),

    .o_mem_wen(usb_mem_wen),
    .o_mem_ren(usb_mem_ren),
    .o_mem_addr(usb_mem_addr),
    .o_mem_wdata(usb_mem_wdata),
    .i_mem_rdata(usb_mem_rdata),

    .i_reg_wen(cpu_mem_wstrb && cpu_mem_addr[31:28] == 4'h2),
    .i_reg_addr(cpu_mem_addr[5:2]),
    .o_reg_rdata(usb_reg_rdata),
    .i_reg_wdata(cpu_mem_wdata)
  );

  reg [9:0] usb_mem_addr_p;
  always @(posedge clk) begin
    if (usb_mem_ren)
      usb_mem_addr_p <= usb_mem_addr;
  end

  always @(*) begin
    case (usb_mem_addr_p[1:0])
    2'h0: usb_mem_rdata = ram_rdata[7:0];
    2'h1: usb_mem_rdata = ram_rdata[15:8];
    2'h2: usb_mem_rdata = ram_rdata[23:16];
    2'h3: usb_mem_rdata = ram_rdata[31:24];
    endcase
  end

  //DEBUG
  /*
  always @(posedge clk) begin
    if (usb_mem_ren)
      $display("USB MEM read addr %h", usb_mem_addr);
    if (usb_mem_wen)
      $display("USB MEM write addr %h", usb_mem_addr);
  end
  */

endmodule
