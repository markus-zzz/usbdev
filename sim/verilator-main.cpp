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

#include "Vsoc_top.h"
#include "Vsoc_top_soc_top.h"
#include "Vsoc_top_spram__D8.h"
#include "usb-pack-gen.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include <dlfcn.h>
#include <iomanip>
#include <stdio.h>

static Vsoc_top *u_usb_dev = NULL;
static VerilatedVcdC *trace = NULL;
static unsigned tick = 0;

static FILE *csvFP = NULL;

double sc_time_stamp() { return tick; }

void ClockUsbSymbolInternal(UsbSymbol sym) {
  switch (sym) {
  case UsbSymbol::J:
    u_usb_dev->i_usb_j_not_k = 1;
    u_usb_dev->i_usb_se0 = 0;
    break;
  case UsbSymbol::K:
    u_usb_dev->i_usb_j_not_k = 0;
    u_usb_dev->i_usb_se0 = 0;
    break;
  case UsbSymbol::SE0:
    u_usb_dev->i_usb_j_not_k = 0;
    u_usb_dev->i_usb_se0 = 1;
    break;
  case UsbSymbol::SE1:
    break;
  }

  for (int i = 0; i < 10; i++) {
    u_usb_dev->i_clk = 1;
    u_usb_dev->eval();
    if (trace)
      trace->dump(tick++);
    u_usb_dev->i_clk = 0;
    u_usb_dev->eval();
    if (trace)
      trace->dump(tick++);
  }
}

void CsvDumpSymbol(UsbSymbol sym) {
  for (int i = 0; i < 4; i++) {
    if (csvFP) {
      switch (sym) {
      case UsbSymbol::J:
        fprintf(csvFP, "1,0\n");
        break;
      case UsbSymbol::K:
        fprintf(csvFP, "0,1\n");
        break;
      case UsbSymbol::SE0:
        fprintf(csvFP, "0,0\n");
        break;
      case UsbSymbol::SE1:
        break;
      }
    }
  }
  fflush(csvFP);
}

void ClockUsbSymbol(UsbSymbol sym) {

  ClockUsbSymbolInternal(sym);
  CsvDumpSymbol(sym);
}

UsbPacket *tryRecvUsbPacket(unsigned maxWaitBitTimes = 8) {
  UsbSymbolVector recvSyms;

  for (int i = 0; i < maxWaitBitTimes || u_usb_dev->o_usb_oe; i++) {
    ClockUsbSymbolInternal(UsbSymbol::J);
    if (u_usb_dev->o_usb_oe) {
      UsbSymbol sym;
      if (u_usb_dev->o_usb_se0)
        sym = UsbSymbol::SE0;
      else
        sym = u_usb_dev->o_usb_j_not_k ? UsbSymbol::J : UsbSymbol::K;
      recvSyms.push_back(sym);
      CsvDumpSymbol(sym);
    } else
      CsvDumpSymbol(UsbSymbol::J);
  }

  if (recvSyms.size() == 0 || !recvSyms.startsWithSync() ||
      !recvSyms.endsWithEop()) {
    // Invalid packet received.
    return nullptr;
  }

  recvSyms.stripEop();
  UsbBitVector recvBits(recvSyms);

  UsbPacket *p;
  if ((p = UsbAckPacket::tryDecode(recvBits)))
    return p;
  else if ((p = UsbNakPacket::tryDecode(recvBits)))
    return p;
  else if ((p = UsbData0Packet::tryDecode(recvBits)))
    return p;
  else if ((p = UsbData1Packet::tryDecode(recvBits)))
    return p;

  return nullptr;
}

UsbPacket *printResponse() {
  UsbPacket *recvPacket;
  if (recvPacket = tryRecvUsbPacket())
    recvPacket->print(std::cout);
  else
    std::cout << "[No response]\n";
  return recvPacket;
}

void dumpRAM(unsigned size) {
  uint8_t *p[4] = {u_usb_dev->soc_top->genblk1__BRA__0__KET____DOT__u_ram->mem,
                   u_usb_dev->soc_top->genblk1__BRA__1__KET____DOT__u_ram->mem,
                   u_usb_dev->soc_top->genblk1__BRA__2__KET____DOT__u_ram->mem,
                   u_usb_dev->soc_top->genblk1__BRA__3__KET____DOT__u_ram->mem};

  for (unsigned i = 0; i < size; i++) {
    if (i % 16 == 0)
      std::cout << std::endl
                << std::setw(4) << std::setfill('0') << std::hex << i << ":";

    std::cout << " " << std::setw(2) << std::setfill('0') << std::hex
              << (int)p[i & 3][i >> 2];
  }
  std::cout << std::endl;
}

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <test-case.so>\n", argv[0]);
    return -1;
  }

  void *dl_handle = dlopen(argv[1], RTLD_LAZY);
  if (!dl_handle) {
    fprintf(stderr, "dlopen: %s\n", dlerror());
    return -1;
  }

  void (*test_function)(void) = (void (*)())dlsym(dl_handle, "run_test");
  if (!test_function) {
    fprintf(stderr, "dlsym: %s\n", dlerror());
    return -1;
  }

  csvFP = fopen("dump.csv", "w");

  // Initialize Verilators variables
  Verilated::commandArgs(argc, argv);

  Verilated::traceEverOn(true);

  u_usb_dev = new Vsoc_top;

  if (true) {
    trace = new VerilatedVcdC;
    u_usb_dev->trace(trace, 99);
    trace->open("dump.vcd");
  }

  u_usb_dev->i_clk = 0;
  u_usb_dev->i_rst = 0;
  u_usb_dev->i_usb_j_not_k = 0;
  u_usb_dev->i_usb_se0 = 0;

  // Pulse reset.
  u_usb_dev->i_rst = 1;
  ClockUsbSymbol(UsbSymbol::J);
  u_usb_dev->i_rst = 0;

  // Wait for USB device to attach.
  while (!u_usb_dev->o_usb_attach)
    ClockUsbSymbol(UsbSymbol::J);

  // Then apply some additional clocks.
  ClockUsbSymbol(UsbSymbol::J);
  ClockUsbSymbol(UsbSymbol::J);
  ClockUsbSymbol(UsbSymbol::J);
  ClockUsbSymbol(UsbSymbol::J);

  // Execute the selected test.
  test_function();

  trace->flush();

  dumpRAM(256);

  return 0;
}
