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

#if COMPILE_FIRMWARE

#include "usb-dev-driver.c"

#else

#include "usb-sim-tests.h"

extern "C" void run_test() {
  std::vector<uint8_t> payload{0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00};
  UsbData0Packet data0(payload);

  // OUT followed by DATA0
  UsbSetupPacket setup(0, 0);
  setup.sendUsb();
  data0.sendUsb();
  assert(dynamic_cast<UsbAckPacket *>(printResponse()) &&
         "Expected ACK response.");

  UsbInPacket in(0, 0);

  // Allow for a few NAKs
  UsbPacket *rxp;
  for (int i = 0; i < 3; i++) {
    // IN
    in.sendUsb();
    while ((rxp = printResponse()) && dynamic_cast<UsbNakPacket *>(rxp))
      in.sendUsb();
    if (i & 1) {
      // Then expect the DATA0.
      assert(dynamic_cast<UsbData0Packet *>(rxp) && "Expected DATA0 response.");
    } else {
      // Then expect the DATA1.
      assert(dynamic_cast<UsbData1Packet *>(rxp) && "Expected DATA1 response.");
    }
    // ACK the data
    UsbAckPacket ack;
    ack.sendUsb();
  }

  UsbOutPacket out(0, 0);
  std::vector<uint8_t> empty{};
  UsbData1Packet data1(empty);
  out.sendUsb();
  data1.sendUsb();
  while ((rxp = printResponse()) && dynamic_cast<UsbNakPacket *>(rxp)) {
    out.sendUsb();
    data1.sendUsb();
  }

  uint8_t address = 10;
  {
    // SET_ADDRESS
    std::vector<uint8_t> payload{0x00, 0x05, address, 0x00,
                                 0x00, 0x00, 0x00,    0x00};
    UsbData0Packet data0(payload);
    UsbSetupPacket setup(0, 0);
    setup.sendUsb();
    data0.sendUsb();
    assert(dynamic_cast<UsbAckPacket *>(printResponse()) &&
           "Expected ACK response.");
    UsbInPacket in(0, 0);
    in.sendUsb();
    assert(dynamic_cast<UsbData1Packet *>(printResponse()) &&
           "Expected DATA1 response.");
    UsbAckPacket ack;
    ack.sendUsb();
  }

  {
    // Try setup again
    UsbSetupPacket setup(address, 0);
    setup.sendUsb();
    data0.sendUsb();
    assert(dynamic_cast<UsbAckPacket *>(printResponse()) &&
           "Expected ACK response.");
    UsbInPacket in(address, 0);

    // Allow for a few NAKs
    UsbPacket *rxp;
    for (int i = 0; i < 3; i++) {
      // IN
      in.sendUsb();
      while ((rxp = printResponse()) && dynamic_cast<UsbNakPacket *>(rxp))
        in.sendUsb();
      if (i & 1) {
        // Then expect the DATA0.
        assert(dynamic_cast<UsbData0Packet *>(rxp) &&
               "Expected DATA0 response.");
      } else {
        // Then expect the DATA1.
        assert(dynamic_cast<UsbData1Packet *>(rxp) &&
               "Expected DATA1 response.");
      }
      // ACK the data
      UsbAckPacket ack;
      ack.sendUsb();
    }
  }
}

#endif
