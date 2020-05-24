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

/* verilator lint_off WIDTH */

`default_nettype none

module usb_dev(
  input i_rst,
  input i_clk_48mhz,
  input i_usb_j_not_k,
  input i_usb_se0,

  output o_usb_oe,
  output o_usb_j_not_k,
  output o_usb_se0,
  output o_usb_attach,

  output o_mem_wen,
  output o_mem_ren,
  output [9:0] o_mem_addr,
  output [7:0] o_mem_wdata,
  input  [7:0] i_mem_rdata,

  input i_reg_wen,
  input i_reg_ren,
  input  [3:0] i_reg_addr,
  input  [31:0] i_reg_wdata,
  output reg [31:0] o_reg_rdata,

  output test_out
);

  //
  // Control registers exposed to the CPU.
  //
  parameter
    R_USB_ADDR          = 4'h0,
    R_USB_ENDP_OWNER    = 4'h1,
    R_USB_CTRL          = 4'h2,
    R_USB_IN_SIZE_0_7   = 4'h3,
    R_USB_IN_SIZE_8_15  = 4'h4,
    R_USB_DATA_TOGGLE   = 4'h5,
    R_USB_OUT_SIZE_0_7  = 4'h6,
    R_USB_OUT_SIZE_8_15 = 4'h7;

  reg [0:0] r_usb_ctrl; // USB misc ctrl register.
  reg [6:0] r_usb_addr; // USB device address.
  reg [31:0] r_usb_endp_owner; // USB (not CPU) currently owns endpoint. Bits
                               // 31:16 are IN endpoints and bits 15:0 are OUT endpoints.
  reg [63:0] r_usb_in_size;
  reg [63:0] r_usb_out_size;
  reg [31:0] r_usb_data_toggle;

  reg [31:0] usb_endp_owner_clr_mask;

  reg [6:0] token_addr;
  reg [3:0] token_endp;
  reg token_valid;

  wire sample_pos_en;
  wire dec_bit;
  wire dec_bit_en;
  reg [7:0] j_not_k_shift;
  reg [3:0] se0_shift;
  reg [31:0] dec_shift;

  wire [4:0] crc5_res;
  wire [15:0] crc16_res;

  wire dec_eop;
  wire reload;
  reg [7:0] tx_byte;
  wire out_j_not_k;

  reg [7:0] tx_data_mem;
  reg [6:0] tx_data_packet_len, tx_data_packet_idx;
  wire [15:0] tx_crc16, tx_crc16_tmp;

  sample_pos_adj u_spa(
    .i_clk_4x(i_clk_48mhz),
    .i_data(i_usb_j_not_k),
    .o_sample_en(sample_pos_en)
  );

  usb_bit_decode_2 u_bit_dec(
    .clk(i_clk_48mhz),
    .rst(i_rst),
    .i_bit(i_usb_j_not_k),
    .i_bit_en(sample_pos_en),
    .o_bit(dec_bit),
    .o_bit_en(dec_bit_en)
  );

  usb_bit_encode_2 u_bit_enc(
    .clk(i_clk_48mhz),
    .rst(i_rst),
    .i_byte(tx_byte),
    .i_enc_en(sample_pos_en),
    .i_restart(state == S_TX_SYNC),
    .o_reload(reload),
    .o_enc_bit(out_j_not_k)
  );

  assign o_usb_se0 = (state == S_TX_EOP_0 || state == S_TX_EOP_1);
  assign o_usb_oe  = (state > S_TX_SYNC);
  assign o_usb_j_not_k = (state == S_TX_EOP_2) ? 1'b1 : out_j_not_k;

  always @(posedge i_clk_48mhz) begin
    if (sample_pos_en) begin
      j_not_k_shift <= {j_not_k_shift[6:0], i_usb_j_not_k};
      se0_shift <= {se0_shift[2:0], i_usb_se0};
    end
  end

  // All USB data is transmitted least significant bit (lsb) first.
  always @(posedge i_clk_48mhz) begin
    if (dec_bit_en)
      dec_shift <= {dec_bit, dec_shift[31:1]};
  end


  reg [9:0] data_bit_cntr; // Max packet sizes is 64*8=512 so 0-1023 is reasonable range for data bit counter.
  reg [7:0] dec_bit_cntr;

  parameter
    S_WAIT_SYNC      = 1,
    S_WAIT_PID       = 2,
    S_WAIT_FRAME_NBR = 3,
    S_WAIT_CRC5      = 4,
    S_WAIT_TOK_EOP   = 5,
    S_WAIT_ADDR_ENDP = 6,
    S_WAIT_DATA      = 7,
    S_WAIT_HAND_EOP  = 8,

    S_WAIT_TX_0       = 10,
    S_WAIT_TX_1       = 11,

    S_TX_SYNC         = 16,
    S_TX_PID          = 17,
    S_TX_WAIT_ACK_NAK = 18,
    S_TX_DATA         = 19,
    S_TX_CRC_0        = 20,
    S_TX_CRC_1        = 21,
    S_TX_CRC_2        = 22,
    S_TX_EOP_0        = 23,
    S_TX_EOP_1        = 24,
    S_TX_EOP_2        = 25;

  parameter
    PID_OUT   = 4'b0001,
    PID_IN    = 4'b1001,
    PID_SOF   = 4'b0101,
    PID_SETUP = 4'b1101,
    PID_DATA0 = 4'b0011,
    PID_DATA1 = 4'b1011,
    PID_ACK   = 4'b0010,
    PID_NAK   = 4'b1010,
    PID_STALL = 4'b1110;

  reg [5:0] state;
  reg crc5_en;
  reg crc16_en;
  reg crc16_pass;
  reg [3:0] rx_pid, tx_pid;

  assign test_out = o_usb_oe;

  initial state = S_WAIT_SYNC;

  task print_pid;
  input [7:0] pid;
  begin
    case (pid)
      {~PID_SETUP, PID_SETUP}: $display("%t: PID: SETUP", $time);
      {~PID_IN, PID_IN}:       $display("%t: PID: IN", $time);
      {~PID_OUT, PID_OUT}:     $display("%t: PID: OUT", $time);
      {~PID_DATA0, PID_DATA0}: $display("%t: PID: DATA0", $time);
      {~PID_DATA1, PID_DATA1}: $display("%t: PID: DATA1", $time);
      {~PID_ACK, PID_ACK}:     $display("%t: PID: ACK", $time);
      {~PID_NAK, PID_NAK}:     $display("%t: PID: NAK", $time);
      {~PID_STALL, PID_STALL}: $display("%t: PID: STALL", $time);
      {~PID_SOF, PID_SOF}:     $display("%t: PID: SOF", $time);
      default:                 $display("%t: PID: <INVALID> %b", $time, pid);
    endcase
  end
  endtask

  // Detect the EOP sequence {SE0,SE0,J}
  assign dec_eop = se0_shift[2:1] == 2'b11 && j_not_k_shift[0] == 1'b1;
  wire usb_clk_en = o_usb_oe ? sample_pos_en : dec_bit_en;

  always @(posedge i_clk_48mhz) begin
    usb_endp_owner_clr_mask <= 0;
    if (i_rst) begin
      state <= S_WAIT_SYNC;
      dec_bit_cntr <= 0;
      crc5_en <= 0;
      crc16_en <= 0;
    end
    else if (usb_clk_en) begin
      dec_bit_cntr <= dec_bit_cntr + 1;
      case (state)
        S_WAIT_SYNC: begin
          // It is better to detect SYNC on the raw bit stream since the bus
          // IDLE state messes with the bit de-stuffing.
          if (j_not_k_shift[7:0] == 8'b0101_0100) begin
            state <= S_WAIT_PID;
            dec_bit_cntr <= 1;
            $display("%t: SYNC", $time);
          end
        end
        S_WAIT_PID: begin
          if (dec_bit_cntr == 8) begin
            print_pid(dec_shift[31:24]);
            rx_pid <= dec_shift[27:24];
            case (dec_shift[31:24])
              //
              // Token Packets
              //
              {~PID_IN, PID_IN}, {~PID_OUT, PID_OUT}, {~PID_SETUP, PID_SETUP}: begin
                state <= S_WAIT_ADDR_ENDP;
                dec_bit_cntr <= 1;
                crc5_en <= 1;
              end
              //
              // Data Packets
              //
              {~PID_DATA0, PID_DATA0}, {~PID_DATA1, PID_DATA1}: begin
                state <= S_WAIT_DATA;
                data_bit_cntr <= 1;
                crc16_en <= 1;
                crc16_pass <= 0;
              end

              //
              // Handshake Packets
              //
              {~PID_ACK, PID_ACK}, {~PID_NAK, PID_NAK}: begin
                state <= S_WAIT_HAND_EOP;
              end

              //
              // Start of Frame Packets
              //
              {~PID_SOF, PID_SOF}: begin
                state <= S_WAIT_FRAME_NBR;
                dec_bit_cntr <= 1;
                crc5_en <= 1;
              end
              default: begin
                // Got INVALID PID.
                state <= S_WAIT_SYNC;
              end
            endcase
          end
        end
        S_WAIT_ADDR_ENDP: begin
          if (dec_bit_cntr == 7 + 4) begin
            $display("%t: Addr=%d, Endpoint=%d", $time, dec_shift[27:21], dec_shift[31:28]);
            token_addr <= dec_shift[27:21];
            token_endp <= dec_shift[31:28];
            state <= S_WAIT_CRC5;
            dec_bit_cntr <= 1;
          end
        end
        S_WAIT_FRAME_NBR: begin
          if (dec_bit_cntr == 11) begin
            $display("%t: Frame#=%d", $time, dec_shift[31:21]);
            state <= S_WAIT_CRC5;
            dec_bit_cntr <= 1;
          end
        end
        S_WAIT_CRC5: begin
          if (dec_bit_cntr == 5) begin
            state <= S_WAIT_TOK_EOP;
            crc5_en <= 0;
          end
        end
        S_WAIT_TOK_EOP: begin
          if (dec_eop) begin
            $display("%t: EOP", $time);
            state <= S_WAIT_SYNC;
            token_valid <= 0;
            // Compare against expected residual given in spec.
            if (crc5_res == 5'b01100) begin
              $display("%t: CRC5 - pass", $time);
              if (rx_pid == PID_SOF) begin
                //XXX: Store away the frame nbr.
              end
              else if (r_usb_addr == token_addr) begin // PID_SETUP, PID_IN, PID_OUT
                token_valid <= 1;
                if (rx_pid == PID_IN) begin
                  state <= S_WAIT_TX_0;
                  if (r_usb_endp_owner[{1'b1, token_endp}]) begin
                    tx_pid <= r_usb_data_toggle[{1'b1, token_endp}] ? PID_DATA1 : PID_DATA0;
                    tx_data_packet_idx <= 0;
                    tx_data_packet_len <= r_usb_in_size[token_endp*4+:4];
                  end
                  else
                    tx_pid <= PID_NAK;
                end
              end
            end
            else
              $display("%t: CRC5 - fail (residual %b)", $time, crc5_res);
          end
        end
        S_WAIT_HAND_EOP: begin
          if (dec_eop) begin
            $display("%t: EOP", $time);
            state <= S_WAIT_SYNC;
            if (rx_pid == PID_ACK)
              usb_endp_owner_clr_mask[{1'b1, token_endp}] <= 1'b1;
          end
        end
        S_WAIT_DATA: begin
          data_bit_cntr <= data_bit_cntr + 1;
          if (data_bit_cntr[2:0] == 0) begin
            $display("%t: DATA = %h", $time, dec_shift[31:24]);
          end
          if (data_bit_cntr[2:0] == 1) begin
            // Compare CRC against expected residual given in spec. We do not
            // know if the packet is done yet (more bytes may follow).
            crc16_pass <= (crc16_res == 16'b1000_0000_0000_1101) ? 1 : 0;
          end
          if (dec_eop) begin
            $display("%t: EOP data_bit_cntr=%d, crc16_pass=%h", $time, data_bit_cntr, crc16_pass);
            crc16_en <= 0;
            if (token_valid & crc16_pass) begin
              state <= S_WAIT_TX_0;
              if (r_usb_endp_owner[{1'b0, token_endp}]) begin
                tx_pid <= PID_ACK;
                usb_endp_owner_clr_mask[{1'b0, token_endp}] <= 1'b1;
                r_usb_out_size[token_endp*4+:4] <= data_bit_cntr[6:3] - 2;
              end
              else
                tx_pid <= PID_NAK;
            end
            else begin
              state <= S_WAIT_SYNC;
            end
          end
        end

        //
        // Tx - states
        //
        S_WAIT_TX_0: begin
          state <= S_WAIT_TX_1;
        end
        S_WAIT_TX_1: begin
          state <= S_TX_SYNC;
        end

        S_TX_SYNC: begin
          state <= S_TX_PID;
        end
        S_TX_PID: begin
          if (reload) begin
            case (tx_pid)
              PID_DATA0, PID_DATA1: begin
                if (tx_data_packet_len == 0) begin
                  state <= S_TX_CRC_0;
                end
                else begin
                  if (reload)
                    tx_data_packet_idx <= tx_data_packet_idx + 1;
                  state <= S_TX_DATA;
                end
              end
              PID_ACK, PID_NAK: state <= S_TX_WAIT_ACK_NAK;
              default: /* not possible */;
            endcase
          end
        end
        S_TX_WAIT_ACK_NAK: begin
          if (reload)
            state <= S_TX_EOP_0;
        end
        S_TX_DATA: begin
          if (reload)
            tx_data_packet_idx <= tx_data_packet_idx + 1;
          if (reload && tx_data_packet_idx == tx_data_packet_len) begin
            state <= S_TX_CRC_0;
          end
        end
        S_TX_CRC_0: begin
          if (reload) begin
            state <= S_TX_CRC_1;
          end
        end
        S_TX_CRC_1: begin
          if (reload)
            state <= S_TX_CRC_2;
        end
        S_TX_CRC_2: begin
          if (reload)
            state <= S_TX_EOP_0;
        end
        S_TX_EOP_0: begin
          // Drive SE0.
          state <= S_TX_EOP_1;
        end
        S_TX_EOP_1: begin
          // Drive SE0.
          state <= S_TX_EOP_2;
        end
        S_TX_EOP_2: begin
          // Drive J.
          state <= S_WAIT_SYNC;
        end
      endcase
    end
  end

  always @(*) begin
    case (state)
      /*S_TX_PID,*/ default: begin
        tx_byte = {~tx_pid, tx_pid};
      end
      S_TX_DATA: begin
        tx_byte = tx_data_mem;
      end
      S_TX_CRC_0: begin
        tx_byte = tx_crc16[7:0];
      end
      S_TX_CRC_1: begin
        tx_byte = tx_crc16[15:8];
      end
    endcase
  end

  // CRC for Token and SOF packets.
  crc5 u_crc5(
    .data_in(dec_shift[31]),
    .crc_en(dec_bit_en & crc5_en),
    .crc_out(crc5_res),
    .rst(state == S_WAIT_SYNC),
    .clk(i_clk_48mhz)
  );

  // CRC for Data packets.
  crc16 u_crc16(
    .data_in(dec_shift[31]),
    .crc_en(dec_bit_en & crc16_en),
    .crc_out(crc16_res),
    .rst(state == S_WAIT_SYNC),
    .clk(i_clk_48mhz)
  );

  reg [7:0] tx_crc_shift;
  reg [3:0] tx_crc_shift_cntr;

  always @(posedge i_clk_48mhz) begin
    if (i_rst) begin
      tx_crc_shift_cntr <= 8;
    end
    else if (reload && state == S_TX_DATA) begin
      tx_crc_shift <= tx_byte;
      tx_crc_shift_cntr <= 0;
    end
    else if (tx_crc_shift_cntr < 8) begin
      tx_crc_shift <= {1'b0, tx_crc_shift[7:1]};
      tx_crc_shift_cntr <= tx_crc_shift_cntr + 1;
    end
  end

  // CRC for Tx Data packets (should be merged with other crc16).
  crc16 u_crc16_tx(
    .data_in(tx_crc_shift[0]),
    .crc_en(tx_crc_shift_cntr < 8),
    .crc_out(tx_crc16_tmp),
    .rst(state == S_TX_SYNC),
    .clk(i_clk_48mhz)
  );

  // According to spec crc16 should be sent MSB first and negated for Tx.
  assign tx_crc16 = ~{tx_crc16_tmp[0],tx_crc16_tmp[1],tx_crc16_tmp[2],tx_crc16_tmp[3],
                      tx_crc16_tmp[4],tx_crc16_tmp[5],tx_crc16_tmp[6],tx_crc16_tmp[7],
                      tx_crc16_tmp[8],tx_crc16_tmp[9],tx_crc16_tmp[10],tx_crc16_tmp[11],
                      tx_crc16_tmp[12],tx_crc16_tmp[13],tx_crc16_tmp[14],tx_crc16_tmp[15]};

  wire [3:0] byte_cntr_minus_one;
  assign byte_cntr_minus_one = data_bit_cntr[6:3] - 4'h1;
  assign o_mem_addr = state == S_WAIT_DATA ? {1'b0, token_endp, byte_cntr_minus_one[2:0]} : {1'b1, token_endp, tx_data_packet_idx[2:0]};
  assign o_mem_wdata = dec_shift[31:24];
  assign o_mem_wen = (usb_clk_en && state == S_WAIT_DATA && token_valid && r_usb_endp_owner[{1'b0, token_endp}] && data_bit_cntr[2:0] == 0 && byte_cntr_minus_one < 8);
  assign o_mem_ren = (usb_clk_en && (state == S_TX_DATA || state == S_TX_PID) && reload);

  reg mem_ren_p;

  always @(posedge i_clk_48mhz) begin
    mem_ren_p <= o_mem_ren;
  end

  always @(posedge i_clk_48mhz) begin
    if (mem_ren_p)
      tx_data_mem <= i_mem_rdata;
  end

  //
  // USB block control registers.
  //
  integer i;
  always @* begin
    case (i_reg_addr)
      R_USB_ADDR         : o_reg_rdata = r_usb_addr;
      R_USB_ENDP_OWNER   : o_reg_rdata = r_usb_endp_owner;
      R_USB_CTRL         : o_reg_rdata = r_usb_ctrl;
      R_USB_IN_SIZE_0_7  : o_reg_rdata = r_usb_in_size[31:0];
      R_USB_IN_SIZE_8_15 : o_reg_rdata = r_usb_in_size[63:32];
      R_USB_DATA_TOGGLE  : o_reg_rdata = r_usb_data_toggle[31:0];
      R_USB_OUT_SIZE_0_7 : o_reg_rdata = r_usb_out_size[31:0];
      R_USB_OUT_SIZE_8_15: o_reg_rdata = r_usb_out_size[63:32];
      default: o_reg_rdata = r_usb_addr;
    endcase
  end
  always @(posedge i_clk_48mhz) begin
    if (i_rst) begin
      r_usb_addr <= 0;
      r_usb_ctrl <= 0;
      r_usb_in_size <= 0;
      r_usb_data_toggle <= 0;
    end
    else if (i_reg_wen) begin
      case (i_reg_addr)
        R_USB_ADDR        : r_usb_addr <= i_reg_wdata[6:0];
        R_USB_ENDP_OWNER  : /* requires special handling */;
        R_USB_CTRL        : r_usb_ctrl <= i_reg_wdata[0:0];
        R_USB_IN_SIZE_0_7 : r_usb_in_size[31:0] <= i_reg_wdata[31:0];
        R_USB_IN_SIZE_8_15: r_usb_in_size[63:32] <= i_reg_wdata[31:0];
        R_USB_DATA_TOGGLE : r_usb_data_toggle[31:0] <= i_reg_wdata[31:0];
        default           : /* do nothing */;
      endcase
    end
  end
  //r_usb_endp_owner requires special handling since it is written by both CPU and USB.
  wire [31:0] usb_endp_owner_set_mask;
  assign usb_endp_owner_set_mask = (i_reg_wen && i_reg_addr == R_USB_ENDP_OWNER) ? i_reg_wdata : 32'h0;
  always @(posedge i_clk_48mhz) begin
    if (i_rst) begin
      r_usb_endp_owner <= 32'h0000_0000;
    end
    else begin
      //Clear (USB) has priority over set (CPU), but they should never overlap
      //unless SW makes a mistake.
      r_usb_endp_owner <= (r_usb_endp_owner | usb_endp_owner_set_mask) & ~usb_endp_owner_clr_mask;
    end
  end

  assign o_usb_attach = r_usb_ctrl[0];
endmodule
