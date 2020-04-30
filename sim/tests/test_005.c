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

void put_IN_data(unsigned endpoint, const uint8_t *data, unsigned len) {
  uint32_t endpoint_mask = 1UL << (16 + endpoint);
  *R_USB_DATA_TOGGLE ^= endpoint_mask;
  volatile uint8_t *p = (uint8_t *)(MEM_BASE_RAM);
  for (unsigned i = 0; i < len; i++) {
    p[128 + endpoint * 8 + i] = data[i];
  }

  *R_USB_IN_SIZE_0_7 = len & 0xf; // XXX: mask and shift properly for endpoint
  *R_USB_ENDP_OWNER = endpoint_mask;
  // Wait for host to IN data.
  while (*R_USB_ENDP_OWNER & endpoint_mask)
    ;
}

static const uint8_t response[] = {0x12, 0x01, 0x00, 0x02, 0x02, 0x02,
                                   0x00, 0x08, 0x83, 0x04, 0x40, 0x57,
                                   0x00, 0x02, 0x01, 0x02, 0x03, 0x01};
int main(void) {
  *R_USB_ADDR = 0;
  *R_USB_ENDP_OWNER = 0x00000001;
  *R_USB_CTRL = 1;

  // Wait for incomming data on endpoint 0.
  while (*R_USB_ENDP_OWNER & 0x1)
    ;

  put_IN_data(0, &response[0], 8);
  put_IN_data(0, &response[8], 8);
  put_IN_data(0, &response[16], 2);

  // Prepare to accept host ACK.
  *R_USB_ENDP_OWNER = 0x00000001;
  return 0;
}

#else

#include "usb-sim-tests.h"

void test_005() {
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
}

#endif
