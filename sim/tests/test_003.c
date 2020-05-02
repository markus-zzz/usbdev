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

/* Test that tokens to enabled (owned by USB) endpoints generate valid response.
 */

#if COMPILE_FIRMWARE

#include "usb-soc.h"

int main(void) {
  *R_USB_ADDR = 0;
  *R_USB_ENDP_OWNER = 0x00000001;
  *R_USB_CTRL = 1;
  /* Wait for data to arrive on IN endpoint 0 */
  while (*R_USB_ENDP_OWNER & 0x1)
    ;
  /* Copy (+1) data to OUT endpoint 1 and enable it */
  volatile uint8_t *p = (uint8_t *)(MEM_BASE_RAM);
  for (int i = 0; i < 8; i++)
    p[128 + 8 + i] = p[0 + i] + 1;
  /* Wait a while */
  for (int i = 0; i < 128; i++)
    (void)*R_USB_ADDR;
  *R_USB_IN_SIZE_0_7 = 0x00000060;
  *R_USB_DATA_TOGGLE = 0 << (1 + 16);
  /* Enable OUT endpoint 1 */
  *R_USB_ENDP_OWNER = 0x00020000;
}

#else

#include "usb-sim-tests.h"

extern "C" void run_test() {
  std::vector<uint8_t> payload{0x23, 0x64, 0x54, 0xaf, 0xca, 0xfe};
  UsbData0Packet data0(payload);

  // OUT followed by DATA0
  UsbOutPacket out(0, 0);
  out.sendUsb();
  data0.sendUsb();
  assert(dynamic_cast<UsbAckPacket *>(printResponse()) &&
         "Expected ACK response.");

  // IN
  UsbInPacket in(0, 1);
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
  // Verify that it contains the expected value
  for (int i = 0; i < data0.m_payloadBytes.size(); i++)
    assert(dynamic_cast<UsbData0Packet *>(rxp)->m_payloadBytes[i] ==
               data0.m_payloadBytes[i] + 1 &&
           "IN should be OUT +1 for each element");
  // Send an additional IN to the same endpoint but this time expect a NAK
  in.sendUsb();
  assert(dynamic_cast<UsbNakPacket *>(printResponse()) &&
         "Expected NAK response.");
}

#endif
