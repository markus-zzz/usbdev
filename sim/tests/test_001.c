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

/* Test that tokens to disabled (owned by CPU) endpoints generate NAK response.
 */

#if COMPILE_FIRMWARE

#include "usb-soc.h"

int main(void) {
  *R_USB_ADDR = 0;
  *R_USB_ENDP_OWNER = 0x0;
  *R_USB_CTRL = 1;
  return 0;
}

#else

#include "usb-sim-tests.h"

extern "C" void run_test() {
  // SETUP followed by DATA0
  UsbSetupPacket setup(0, 0);
  std::vector<uint8_t> payload{0x23, 0x64, 0x54, 0xaf, 0xca, 0xfe};
  UsbData0Packet data0(payload);
  setup.sendUsb();
  data0.sendUsb();
  assert(dynamic_cast<UsbNakPacket *>(printResponse()) &&
         "Expected NAK response.");

  // IN
  UsbInPacket in(0, 0);
  in.sendUsb();
  assert(dynamic_cast<UsbNakPacket *>(printResponse()) &&
         "Expected NAK response.");

  // OUT followed by DATA0
  UsbOutPacket out(0, 0);
  out.sendUsb();
  data0.sendUsb();
  assert(dynamic_cast<UsbNakPacket *>(printResponse()) &&
         "Expected NAK response.");
}

#endif
