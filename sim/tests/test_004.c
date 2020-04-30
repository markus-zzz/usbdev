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

#include "usb-soc.h"

int main(void) {
  *R_USB_ADDR = 0;
  *R_USB_ENDP_OWNER = 0x00000001;
  *R_USB_CTRL = 1;

  // Wait for incomming data on endpoint 0.
  while (*R_USB_ENDP_OWNER & 0x1)
    ;

  // Increment each value by one.
  volatile uint8_t *p = (uint8_t *)(MEM_BASE_RAM);
  for (int i = 0; i < 8; i++) {
    p[128 + i] = p[0 + i] + 1;
  }

  *R_USB_ENDP_OWNER = 0x00010000;
  return 0;
}

#else

#include "usb-sim-tests.h"

void test_004() {
  std::vector<uint8_t> payload{0x80, 0x06, 0x00, 0x01, 0x00, 0x00, 0x40, 0x00};
  UsbData0Packet data0(payload);

  // OUT followed by DATA0
  UsbSetupPacket setup(0, 0);
  setup.sendUsb();
  data0.sendUsb();
  assert(dynamic_cast<UsbAckPacket *>(printResponse()) &&
         "Expected ACK response.");

  // IN
  UsbInPacket in(0, 0);
  in.sendUsb();

  // Allow for a few NAKs
  UsbPacket *rxp;
  while ((rxp = printResponse()) && dynamic_cast<UsbNakPacket *>(rxp))
    in.sendUsb();
  // Then expect the DATA0.
  assert(dynamic_cast<UsbData0Packet *>(rxp) && "Expected DATA0 response.");
  // ACK the data
  UsbAckPacket ack;
  ack.sendUsb();
}

#endif
