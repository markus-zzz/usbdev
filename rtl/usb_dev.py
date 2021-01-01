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
# yapf --in-place --recursive --style="{indent_width: 2, column_limit: 120}"

from nmigen import *
from nmigen.cli import main
from enum import IntEnum, unique


class UsbBitDecoder(Elaboratable):
  def __init__(self):
    self.i_bit = Signal()
    self.i_bit_en = Signal()
    self.o_bit = Signal()
    self.o_bit_en = Signal()

  def elaborate(self, platform):
    m = Module()
    bit_p = Signal()
    ones_cntr = Signal(range(7))
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
    shift_cntr = Signal(range(8))
    ones_cntr = Signal(range(7))
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
      m.d.sync += ones_cntr.eq(Mux(six_ones | ~shift[0], 0, ones_cntr + 1))

    with m.If(self.i_restart):
      # Prepare new sync-pattern. Idle state is J.
      m.d.sync += [shift.eq(0x80), shift_cntr.eq(0), prev_enc_bit.eq(1)]
    with m.Elif(self.i_enc_en):
      m.d.sync += prev_enc_bit.eq(self.o_enc_bit)
      with m.If(~six_ones):
        m.d.sync += shift_cntr.eq(shift_cntr + 1)
        with m.If(shift_cntr == 7):
          m.d.sync += shift.eq(self.i_byte)
          m.d.comb += self.o_reload.eq(1)  # Should be called o_byte_ack instead of o_reload?
        with m.Else():
          m.d.sync += shift.eq(Cat(shift[1:8], C(0, 1)))
    return m


class UsbBitSync(Elaboratable):
  def __init__(self):
    self.i_data = Signal()
    self.o_sample_en = Signal()

  def elaborate(self, platform):
    m = Module()
    cntr = Signal(range(10))
    m.d.comb += self.o_sample_en.eq(cntr == 0)
    with m.If(cntr == 9):
      m.d.sync += cntr.eq(0)
    with m.Else():
      m.d.sync += cntr.eq(cntr + 1)
    return m


class UsbCrc5(Elaboratable):
  def __init__(self):
    self.i_data = Signal()
    self.i_crc_en = Signal()
    self.i_reset = Signal()
    self.o_crc = Signal(5)

  def elaborate(self, platform):
    m = Module()
    lfsr_c = Signal(5)
    lfsr_q = Signal(5, reset=0x1f)

    m.d.comb += self.o_crc.eq(lfsr_q)

    m.d.comb += [
        lfsr_c[0].eq(lfsr_q[4] ^ self.i_data), lfsr_c[1].eq(lfsr_q[0]),
        lfsr_c[2].eq(lfsr_q[1] ^ lfsr_q[4] ^ self.i_data), lfsr_c[3].eq(lfsr_q[2]), lfsr_c[4].eq(lfsr_q[3])
    ]

    with m.If(self.i_reset):
      m.d.sync += lfsr_q.eq(0x1f)
    with m.Elif(self.i_crc_en):
      m.d.sync += lfsr_q.eq(lfsr_c)

    return m


class UsbCrc16(Elaboratable):
  # crc[15:0]=1+x^2+x^15+x^16
  def __init__(self):
    self.i_data = Signal()
    self.i_crc_en = Signal()
    self.i_reset = Signal()
    self.o_crc = Signal(16)

  def elaborate(self, platform):
    m = Module()
    lfsr_c = Signal(16)
    lfsr_q = Signal(16, reset=0xffff)

    m.d.comb += self.o_crc.eq(lfsr_q)

    m.d.comb += [
        lfsr_c[0].eq(lfsr_q[15] ^ self.i_data), lfsr_c[1].eq(lfsr_q[0]),
        lfsr_c[2].eq(lfsr_q[1] ^ lfsr_q[15] ^ self.i_data), lfsr_c[3].eq(lfsr_q[2]), lfsr_c[4].eq(lfsr_q[3]),
        lfsr_c[5].eq(lfsr_q[4]), lfsr_c[6].eq(lfsr_q[5]), lfsr_c[7].eq(lfsr_q[6]), lfsr_c[8].eq(lfsr_q[7]),
        lfsr_c[9].eq(lfsr_q[8]), lfsr_c[10].eq(lfsr_q[9]), lfsr_c[11].eq(lfsr_q[10]), lfsr_c[12].eq(lfsr_q[11]),
        lfsr_c[13].eq(lfsr_q[12]), lfsr_c[14].eq(lfsr_q[13]), lfsr_c[15].eq(lfsr_q[14] ^ lfsr_q[15] ^ self.i_data)
    ]

    with m.If(self.i_reset):
      m.d.sync += lfsr_q.eq(0xffff)
    with m.Elif(self.i_crc_en):
      m.d.sync += lfsr_q.eq(lfsr_c)

    return m


class UsbDevice(Elaboratable):
  def __init__(self):
    self.i_usb_j_not_k = Signal()
    self.i_usb_se0 = Signal()

    self.o_usb_oe = Signal()
    self.o_usb_j_not_k = Signal()
    self.o_usb_se0 = Signal()
    self.o_usb_attach = Signal()

    self.o_mem_wen = Signal()
    self.o_mem_ren = Signal()
    self.o_mem_addr = Signal(10)
    self.o_mem_wdata = Signal(8)
    self.i_mem_rdata = Signal(8)

    self.i_reg_wen = Signal()
    self.i_reg_ren = Signal()
    self.i_reg_addr = Signal(4)
    self.i_reg_wdata = Signal(32)
    self.o_reg_rdata = Signal(32)

    self.ports = [
        self.i_usb_j_not_k, self.i_usb_se0, self.o_usb_oe, self.o_usb_j_not_k, self.o_usb_se0, self.o_usb_attach,
        self.o_mem_wen, self.o_mem_ren, self.o_mem_addr, self.o_mem_wdata, self.i_mem_rdata, self.i_reg_wen,
        self.i_reg_ren, self.i_reg_addr, self.i_reg_wdata, self.o_reg_rdata
    ]

  def elaborate(self, platform):
    @unique
    class PID(IntEnum):
      OUT = 0b0001
      IN = 0b1001
      SOF = 0b0101
      SETUP = 0b1101
      DATA0 = 0b0011
      DATA1 = 0b1011
      ACK = 0b0010
      NAK = 0b1010
      STALL = 0b1110

    def foo(a):
      return (((~int(a)) << 4) | int(a)) & 0xff

    m = Module()

    j_not_k_shift = Signal(8)
    se0_shift = Signal(4)
    dec_shift = Signal(32)
    dec_bit_cntr = Signal(range(32))
    rx_pid = Signal(4)
    tx_pid = Signal(4)
    token_addr = Signal(7)
    token_endp = Signal(4)
    token_valid = Signal()
    crc5_en = Signal()
    crc5_rst = Signal()
    crc16_en = Signal()
    crc16_rst = Signal()
    crc16_pass = Signal()
    dec_eop = Signal()
    data_bit_cntr = Signal(10)
    tx_byte = Signal(8)
    tx_data_packet_len = Signal(7)
    tx_data_packet_idx = Signal(7)
    tx_data_mem = Signal(8)
    tx_crc16 = Signal(16)

    # Control registers exposed to the CPU.

    @unique
    class R_USB(IntEnum):
      ADDR = 0
      ENDP_OWNER = 1
      CTRL = 2
      IN_SIZE_0_7 = 3
      IN_SIZE_8_15 = 4
      DATA_TOGGLE = 5
      OUT_SIZE_0_7 = 6
      OUT_SIZE_8_15 = 7

    r_usb_ctrl = Signal()  # USB misc ctrl register.
    r_usb_addr = Signal(7)  # USB device address.
    # USB (not CPU) currently owns endpoint. Bits
    # 31:16 are IN endpoints and bits 15:0 are OUT endpoints.
    r_usb_endp_owner = Signal(32)
    r_usb_in_size = Signal(64)
    r_usb_out_size = Signal(64)
    r_usb_data_toggle = Signal(32)

    usb_endp_owner_clr_mask = Signal(32)
    usb_endp_owner_set_mask = Signal(32)

    # Sub-modules.
    u_spa = UsbBitSync()
    m.d.comb += [u_spa.i_data.eq(self.i_usb_j_not_k)]

    u_bit_dec = UsbBitDecoder()
    m.d.comb += [u_bit_dec.i_bit.eq(self.i_usb_j_not_k), u_bit_dec.i_bit_en.eq(u_spa.o_sample_en)]

    u_bit_enc = UsbBitEncoder()
    m.d.comb += [
        u_bit_enc.i_byte.eq(tx_byte),
        # u_bit_enc.i_restart.eq(fsm.ongoing('TX_SYNC')),
        u_bit_enc.i_enc_en.eq(u_spa.o_sample_en)
    ]

    u_crc5 = UsbCrc5()
    m.d.comb += [
        u_crc5.i_data.eq(dec_shift[31]),
        u_crc5.i_crc_en.eq(u_bit_dec.o_bit_en & crc5_en),
        u_crc5.i_reset.eq(crc5_rst)
    ]

    u_crc16 = UsbCrc16()
    m.d.comb += [
        u_crc16.i_data.eq(dec_shift[31]),
        u_crc16.i_crc_en.eq(u_bit_dec.o_bit_en & crc16_en),
        u_crc16.i_reset.eq(crc16_rst)
    ]

    u_crc16_tx = UsbCrc16()
    m.submodules += [u_spa, u_bit_dec, u_bit_enc, u_crc5, u_crc16, u_crc16_tx]

    with m.If(u_spa.o_sample_en):
      m.d.sync += [
          j_not_k_shift.eq(Cat(self.i_usb_j_not_k, j_not_k_shift[0:7])),
          se0_shift.eq(Cat(self.i_usb_se0, se0_shift[0:3]))
      ]

    m.d.comb += [dec_eop.eq((se0_shift[1:3] == 0b11) & (j_not_k_shift[0] == 0b1))]

    # All USB data is transmitted least significant bit (lsb) first.
    with m.If(u_bit_dec.o_bit_en):
      m.d.sync += [dec_shift.eq(Cat(dec_shift[1:32], u_bit_dec.o_bit)), dec_bit_cntr.eq(dec_bit_cntr + 1)]

    # Default combinatorial values overridden by FSM.
    m.d.comb += [crc5_rst.eq(0), crc16_rst.eq(0)]

    m.d.sync += [usb_endp_owner_clr_mask.eq(0)]

    with m.FSM(reset='WAIT_SYNC') as fsm:

      m.d.comb += [
          u_bit_enc.i_restart.eq(fsm.ongoing('TX_SYNC')),
          tx_byte.eq(Cat(tx_pid, ~tx_pid)),
          self.o_usb_se0.eq(0),
          self.o_usb_oe.eq(0),
          self.o_usb_j_not_k.eq(u_bit_enc.o_enc_bit)
      ]

      with m.State('WAIT_SYNC'):
        m.d.comb += [crc5_rst.eq(1), crc16_rst.eq(1)]
        # It is better to detect SYNC on the raw bit stream since the bus
        # IDLE state messes with the bit de-stuffing.
        with m.If(u_bit_dec.o_bit_en & (j_not_k_shift[0:8] == 0b0101_0100)):
          m.d.sync += dec_bit_cntr.eq(1)
          m.next = 'WAIT_PID'

      with m.State('WAIT_PID'):
        with m.If(u_bit_dec.o_bit_en & (dec_bit_cntr == 8)):
          m.d.sync += [rx_pid.eq(dec_shift[24:28])]
          with m.Switch(dec_shift[24:32]):
            # Token Packets
            with m.Case(foo(PID.OUT), foo(PID.IN), foo(PID.SETUP)):
              m.d.sync += [dec_bit_cntr.eq(1), crc5_en.eq(1)]
              m.next = 'WAIT_ADDR_ENDP'
            # Data Packets
            with m.Case(foo(PID.DATA0), foo(PID.DATA1)):
              m.d.sync += [data_bit_cntr.eq(1), crc16_en.eq(1), crc16_pass.eq(0)]
              m.next = 'WAIT_DATA'
            # Handshake Packets
            with m.Case(foo(PID.ACK), foo(PID.NAK)):
              m.next = 'WAIT_HAND_EOP'
            # Start of Frame Packets
            with m.Case(foo(PID.SOF)):
              m.d.sync += [dec_bit_cntr.eq(1), crc5_en.eq(1)]
              m.next = 'WAIT_FRAME_NBR'
            # Invalid packets
            with m.Default():
              m.next = 'WAIT_SYNC'

      with m.State('WAIT_FRAME_NBR'):
        with m.If(u_bit_dec.o_bit_en & (dec_bit_cntr == 11)):
          m.next = 'WAIT_TOK_EOP'

      with m.State('WAIT_CRC5'):
        with m.If(u_bit_dec.o_bit_en & (dec_bit_cntr == 5)):
          m.d.sync += [crc5_en.eq(0)]
          m.next = 'WAIT_TOK_EOP'

      with m.State('WAIT_TOK_EOP'):
        with m.If(u_bit_dec.o_bit_en & dec_eop):
          m.next = 'WAIT_SYNC'
          m.d.sync += [token_valid.eq(0)]
          # Compare against expected residual given in spec.
          with m.If(u_crc5.o_crc == 0b01100):
            # CRC5 - pass.
            with m.If(rx_pid == PID.SOF):
              #XXX: Store away the frame nbr.
              pass
            # PID_SETUP, PID_IN, PID_OUT
            with m.Elif(r_usb_addr == token_addr):
              m.d.sync += [token_valid.eq(1)]
              with m.If(rx_pid == PID.IN):
                m.next = 'WAIT_TX_0'
                with m.If(r_usb_endp_owner.bit_select(Cat(token_endp, Const(1)), 1)):
                  m.d.sync += [
                      tx_pid.eq(Mux(r_usb_data_toggle.bit_select(Cat(token_endp, Const(1)), 1), PID.DATA1, PID.DATA0)),
                      tx_data_packet_idx.eq(0),
                      tx_data_packet_len.eq(r_usb_in_size.word_select(token_endp, 4))
                  ]
                with m.Else():
                  m.d.sync += [tx_pid.eq(PID.NAK)]

      with m.State('WAIT_ADDR_ENDP'):
        with m.If(u_bit_dec.o_bit_en & (dec_bit_cntr == 7 + 4)):
          m.d.sync += [token_addr.eq(dec_shift[21:28]), token_endp.eq(dec_shift[28:32]), dec_bit_cntr.eq(1)]
          m.next = 'WAIT_CRC5'

      with m.State('WAIT_DATA'):
        with m.If(u_bit_dec.o_bit_en):
          m.d.sync += [data_bit_cntr.eq(data_bit_cntr + 1)]
          with m.If(data_bit_cntr[0:3] == 1):
            # Compare CRC against expected residual given in spec. We do not
            # know if the packet is done yet (more bytes may follow).
            m.d.sync += [crc16_pass.eq(u_crc16.o_crc == 0b1000_0000_0000_1101)]
          with m.If(dec_eop):
            m.d.sync += [crc16_en.eq(0)]
            with m.If(token_valid & crc16_pass):
              m.next = 'WAIT_TX_0'
              with m.If(r_usb_endp_owner.bit_select(Cat(token_endp, Const(0)), 1)):
                m.d.sync += [tx_pid.eq(PID.ACK)]
                m.d.sync += [usb_endp_owner_clr_mask.bit_select(Cat(token_endp, Const(0)), 1).eq(1)]
                m.d.sync += [r_usb_out_size.word_select(token_endp, 4).eq(data_bit_cntr[3:7] - 2)]
              with m.Else():
                m.d.sync += [tx_pid.eq(PID.NAK)]
            with m.Else():
              m.next = 'WAIT_SYNC'

      with m.State('WAIT_HAND_EOP'):
        with m.If(u_bit_dec.o_bit_en & dec_eop):
          with m.If(rx_pid == PID.ACK):
            m.d.sync += [usb_endp_owner_clr_mask.bit_select(Cat(token_endp, Const(1)), 1).eq(1)]
          m.next = 'WAIT_SYNC'

      with m.State('WAIT_TX_0'):
        with m.If(u_spa.o_sample_en):
          m.next = 'WAIT_TX_1'

      with m.State('WAIT_TX_1'):
        with m.If(u_spa.o_sample_en):
          m.next = 'TX_SYNC'

      with m.State('TX_SYNC'):
        with m.If(u_spa.o_sample_en):
          m.next = 'TX_PID'

      with m.State('TX_PID'):
        m.d.comb += [self.o_usb_oe.eq(1)]
        with m.If(u_spa.o_sample_en & u_bit_enc.o_reload):
          with m.Switch(tx_pid):
            with m.Case(PID.DATA0, PID.DATA1):
              with m.If(tx_data_packet_len == 0):
                m.next = 'TX_CRC_0'
              with m.Else():
                m.d.sync += [tx_data_packet_idx.eq(tx_data_packet_idx + 1)]
                m.next = 'TX_DATA'
            with m.Case(PID.ACK, PID.NAK):
              m.next = 'TX_WAIT_ACK_NAK'

      with m.State('TX_WAIT_ACK_NAK'):
        m.d.comb += [self.o_usb_oe.eq(1)]
        with m.If(u_spa.o_sample_en & u_bit_enc.o_reload):
          m.next = 'TX_EOP_0'

      with m.State('TX_DATA'):
        m.d.comb += [self.o_usb_oe.eq(1), tx_byte.eq(tx_data_mem)]
        with m.If(u_spa.o_sample_en & u_bit_enc.o_reload):
          m.d.sync += [tx_data_packet_idx.eq(tx_data_packet_idx + 1)]
          with m.If(tx_data_packet_idx == tx_data_packet_len):
            m.next = 'TX_CRC_0'

      with m.State('TX_CRC_0'):
        m.d.comb += [self.o_usb_oe.eq(1), tx_byte.eq(tx_crc16[0:8])]
        with m.If(u_spa.o_sample_en & u_bit_enc.o_reload):
          m.next = 'TX_CRC_1'

      with m.State('TX_CRC_1'):
        m.d.comb += [self.o_usb_oe.eq(1), tx_byte.eq(tx_crc16[8:16])]
        with m.If(u_spa.o_sample_en & u_bit_enc.o_reload):
          m.next = 'TX_CRC_2'

      with m.State('TX_CRC_2'):
        m.d.comb += [self.o_usb_oe.eq(1)]
        with m.If(u_spa.o_sample_en & u_bit_enc.o_reload):
          m.next = 'TX_EOP_0'

      with m.State('TX_EOP_0'):
        m.d.comb += [self.o_usb_oe.eq(1)]
        m.d.comb += [self.o_usb_se0.eq(1)]
        # Drive SE0.
        with m.If(u_spa.o_sample_en):
          m.next = 'TX_EOP_1'

      with m.State('TX_EOP_1'):
        m.d.comb += [self.o_usb_oe.eq(1)]
        m.d.comb += [self.o_usb_se0.eq(1)]
        # Drive SE0.
        with m.If(u_spa.o_sample_en):
          m.next = 'TX_EOP_2'

      with m.State('TX_EOP_2'):
        m.d.comb += [self.o_usb_oe.eq(1)]
        m.d.comb += [self.o_usb_j_not_k.eq(1)]
        # Drive J.
        with m.If(u_spa.o_sample_en):
          m.next = 'WAIT_SYNC'

    byte_cntr_minus_one = Signal(4)
    m.d.comb += [byte_cntr_minus_one.eq(data_bit_cntr[3:7] - 1)]
    m.d.comb += [
        self.o_mem_addr.eq(
            Mux(fsm.ongoing('WAIT_DATA'), Cat(byte_cntr_minus_one[0:3], token_endp, 0),
                Cat(tx_data_packet_idx[0:3], token_endp, 1))),
        self.o_mem_wdata.eq(dec_shift[24:32]),
        self.o_mem_ren.eq(u_spa.o_sample_en & u_bit_enc.o_reload & (fsm.ongoing('TX_PID') | fsm.ongoing('TX_DATA'))),
        self.o_mem_wen.eq(u_bit_dec.o_bit_en & fsm.ongoing('WAIT_DATA') & token_valid
                          & r_usb_endp_owner.bit_select(Cat(token_endp, Const(0)), 1) & (data_bit_cntr[0:3] == 0)
                          & (byte_cntr_minus_one < 8))
    ]

    mem_ren_p = Signal()
    m.d.sync += [mem_ren_p.eq(self.o_mem_ren), tx_data_mem.eq(Mux(mem_ren_p, self.i_mem_rdata, tx_data_mem))]

    tx_crc_shift = Signal(8)
    tx_crc_shift_cntr = Signal(4, reset=8)

    with m.If(u_bit_enc.o_reload & fsm.ongoing('TX_DATA')):
      m.d.sync += [tx_crc_shift.eq(tx_byte), tx_crc_shift_cntr.eq(0)]
    with m.Elif(tx_crc_shift_cntr < 8):
      m.d.sync += [tx_crc_shift.eq(Cat(tx_crc_shift[1:8], 0)), tx_crc_shift_cntr.eq(tx_crc_shift_cntr + 1)]

    m.d.comb += [
        u_crc16_tx.i_data.eq(tx_crc_shift[0]),
        u_crc16_tx.i_crc_en.eq(tx_crc_shift_cntr < 8),
        u_crc16_tx.i_reset.eq(fsm.ongoing('TX_SYNC')),
        tx_crc16.eq(~u_crc16_tx.o_crc[::-1])
    ]

    # USB block control registers.

    # r_usb_endp_owner requires special handling since it is written by both CPU and USB.
    m.d.comb += [usb_endp_owner_set_mask.eq(0)]
    # Clear (USB) has priority over set (CPU), but they should never overlap
    # unless SW makes a mistake.
    m.d.sync += [r_usb_endp_owner.eq((r_usb_endp_owner | usb_endp_owner_set_mask) & ~usb_endp_owner_clr_mask)]

    with m.Switch(self.i_reg_addr):
      with m.Case(R_USB.ADDR):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_addr)]
        with m.If(self.i_reg_wen):
          m.d.sync += [r_usb_addr.eq(self.i_reg_wdata[0:7])]
      with m.Case(R_USB.ENDP_OWNER):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_endp_owner)]
        # Writing requires special handling.
        with m.If(self.i_reg_wen):
          m.d.comb += [usb_endp_owner_set_mask.eq(self.i_reg_wdata)]
      with m.Case(R_USB.CTRL):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_ctrl)]
        with m.If(self.i_reg_wen):
          m.d.sync += [r_usb_ctrl.eq(self.i_reg_wdata[0:1])]
      with m.Case(R_USB.IN_SIZE_0_7):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_in_size[0:32])]
        with m.If(self.i_reg_wen):
          m.d.sync += [r_usb_in_size[0:32].eq(self.i_reg_wdata)]
      with m.Case(R_USB.IN_SIZE_8_15):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_in_size[32:64])]
        with m.If(self.i_reg_wen):
          m.d.sync += [r_usb_in_size[32:64].eq(self.i_reg_wdata)]
      with m.Case(R_USB.DATA_TOGGLE):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_data_toggle)]
        with m.If(self.i_reg_wen):
          m.d.sync += [r_usb_data_toggle.eq(self.i_reg_wdata)]
      with m.Case(R_USB.OUT_SIZE_0_7):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_out_size[0:32])]
        # Read only.
      with m.Case(R_USB.OUT_SIZE_8_15):
        m.d.comb += [self.o_reg_rdata.eq(r_usb_out_size[32:64])]
        # Read only.
      with m.Default():
        m.d.comb += [self.o_reg_rdata.eq(0)]

    m.d.comb += self.o_usb_attach.eq(r_usb_ctrl[0])

    return m


if __name__ == "__main__":
  dec = UsbBitDecoder()
  main(dec, name="usb_bit_decode_2", ports=[dec.i_bit, dec.i_bit_en, dec.o_bit, dec.o_bit_en])

  enc = UsbBitEncoder()
  main(enc, name="usb_bit_encode_2", ports=[enc.i_byte, enc.o_reload, enc.i_restart, enc.i_enc_en, enc.o_enc_bit])

  sync = UsbBitSync()
  main(sync, name="sample_pos_adj_2", ports=[sync.i_data, sync.o_sample_en])

  crc5 = UsbCrc5()
  main(crc5, name="crc5_2", ports=[crc5.i_data, crc5.i_crc_en, crc5.o_crc])

  crc16 = UsbCrc16()
  main(crc16, name="crc16_2", ports=[crc16.i_data, crc16.i_crc_en, crc16.o_crc])

  usbdev = UsbDevice()
  main(usbdev, name="usb_dev_2", ports=usbdev.ports)
