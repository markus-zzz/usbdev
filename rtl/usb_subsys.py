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

# XXX: Check all range() as the stop value is not inclusive.

from nmigen import *
from nmigen.cli import main
from enum import IntEnum, unique

from usb_dev import UsbDevice


def readMemInit(path):
  init = []
  with open(path, 'rb') as f:
    while True:
      bytes = f.read(4)
      if len(bytes) > 0:
        init.append(int.from_bytes(bytes, byteorder='little', signed=False))
      else:
        break
  return init


class PicoRV32(Elaboratable):
  def __init__(self):
    self.o_mem_valid = Signal()
    self.o_mem_instr = Signal()
    self.i_mem_ready = Signal()
    self.o_mem_addr = Signal(32)
    self.o_mem_wdata = Signal(32)
    self.o_mem_wstrb = Signal(4)
    self.i_mem_rdata = Signal(32)

  def elaborate(self, platform):
    m = Module()

    clk = ClockSignal('sync')
    rst = ResetSignal('sync')
    resetn = Signal()
    m.d.comb += resetn.eq(~rst)
    m.submodules += Instance('picorv32',
                             p_COMPRESSED_ISA=Const(1),
                             i_clk=clk,
                             i_resetn=resetn,
                             o_mem_valid=self.o_mem_valid,
                             o_mem_instr=self.o_mem_instr,
                             i_mem_ready=self.i_mem_ready,
                             o_mem_addr=self.o_mem_addr,
                             o_mem_wdata=self.o_mem_wdata,
                             o_mem_wstrb=self.o_mem_wstrb,
                             i_mem_rdata=self.i_mem_rdata)

    return m


class UsbSubSystem(Elaboratable):
  def __init__(self):
    self.i_usb_j_not_k = Signal()
    self.i_usb_se0 = Signal()
    self.o_usb_oe = Signal()
    self.o_usb_j_not_k = Signal()
    self.o_usb_se0 = Signal()
    self.o_usb_attach = Signal()

    self.ports = [
        self.i_usb_j_not_k,
        self.i_usb_se0,
        self.o_usb_oe,
        self.o_usb_j_not_k,
        self.o_usb_se0,
        self.o_usb_attach,
    ]

  def elaborate(self, platform):
    m = Module()

    u_cpu = PicoRV32()

    u_rom = Memory(width=32, depth=1024, init=readMemInit('../sw/usb-dev-driver.bin'))
    u_rom_rd = u_rom.read_port()

    u_ram_rd = []
    u_ram_wr = []
    for idx in range(4):
      u_ram = Memory(width=8, depth=1024, name='u_ram_{}'.format(idx))
      u_ram_rd.append(u_ram.read_port())
      u_ram_wr.append(u_ram.write_port())

    m.submodules += [u_cpu, u_rom_rd]
    for port in u_ram_rd, u_ram_wr:
      m.submodules += port

    u_usbdev = UsbDevice()
    m.submodules += u_usbdev

    m.d.comb += [  # USB - Bus signals.
        u_usbdev.i_usb_j_not_k.eq(self.i_usb_j_not_k),
        u_usbdev.i_usb_se0.eq(self.i_usb_se0),
        self.o_usb_oe.eq(u_usbdev.o_usb_oe),
        self.o_usb_j_not_k.eq(u_usbdev.o_usb_j_not_k),
        self.o_usb_se0.eq(u_usbdev.o_usb_se0),
        self.o_usb_attach.eq(u_usbdev.o_usb_attach),
        # USB - register access.
        u_usbdev.i_reg_addr.eq(u_cpu.o_mem_addr[2:6]),
        u_usbdev.i_reg_wdata.eq(u_cpu.o_mem_wdata)
    ]

    ram_addr = Signal(10)
    ram_wdata = Signal(32)
    ram_wstrb = Signal(4)

    usb_ram_access = Signal()
    m.d.comb += usb_ram_access.eq(u_usbdev.o_mem_ren | u_usbdev.o_mem_wen)

    with m.If(usb_ram_access):
      m.d.comb += [
          ram_addr.eq(u_usbdev.o_mem_addr),
          ram_wdata.eq(Repl(u_usbdev.o_mem_wdata, 4)),
          ram_wstrb.eq(u_usbdev.o_mem_wen << u_usbdev.o_mem_addr[0:2])
      ]
    with m.Else():
      m.d.comb += [
          ram_addr.eq(u_cpu.o_mem_addr[0:10]),
          ram_wdata.eq(u_cpu.o_mem_wdata),
          ram_wstrb.eq(u_cpu.o_mem_wstrb & Repl(u_cpu.o_mem_valid & (u_cpu.o_mem_addr[28:32] == 1), 4))
      ]

    for idx in range(4):
      m.d.comb += [
          u_ram_rd[idx].addr.eq(ram_addr[2:10]), u_ram_wr[idx].addr.eq(ram_addr[2:10]),
          u_ram_wr[idx].data.eq(ram_wdata[8 * idx:8 * (idx + 1)]), u_ram_wr[idx].en.eq(ram_wstrb[idx])
      ]

    usb_mem_addr_p = Signal(2)
    with m.If(u_usbdev.o_mem_ren):
      m.d.sync += usb_mem_addr_p.eq(u_usbdev.o_mem_addr[0:2])

    with m.Switch(usb_mem_addr_p):
      for idx in range(4):
        with m.Case(idx):
          m.d.comb += u_usbdev.i_mem_rdata.eq(u_ram_rd[idx].data)

    cpu_mem_ready = Signal()
    with m.Switch(u_cpu.o_mem_addr[28:32]):
      with m.Case(0):  # ROM
        m.d.sync += cpu_mem_ready.eq(~cpu_mem_ready & u_cpu.o_mem_valid)
        m.d.comb += u_cpu.i_mem_rdata.eq(u_rom_rd.data)
      with m.Case(1):  # RAM
        m.d.sync += cpu_mem_ready.eq(~usb_ram_access & ~cpu_mem_ready & u_cpu.o_mem_valid)
        m.d.comb += u_cpu.i_mem_rdata.eq(Cat(u_ram_rd[0].data, u_ram_rd[1].data, u_ram_rd[2].data, u_ram_rd[3].data))
      with m.Case(2):  # USB (registers)
        m.d.sync += cpu_mem_ready.eq(~cpu_mem_ready & u_cpu.o_mem_valid)
        m.d.comb += [
            u_cpu.i_mem_rdata.eq(u_usbdev.o_reg_rdata),
            u_usbdev.i_reg_wen.eq(u_cpu.o_mem_wstrb.any()),
            u_usbdev.i_reg_ren.eq(1)
        ]
      with m.Default():
        m.d.sync += cpu_mem_ready.eq(0)
        m.d.comb += [u_cpu.i_mem_rdata.eq(0), u_usbdev.i_reg_wen.eq(0), u_usbdev.i_reg_ren.eq(0)]

    m.d.comb += [u_rom_rd.addr.eq(u_cpu.o_mem_addr[2:12]), u_cpu.i_mem_ready.eq(cpu_mem_ready)]

    return m


if __name__ == "__main__":
  uss = UsbSubSystem()
  main(uss, name="usb_subsys", ports=uss.ports)
