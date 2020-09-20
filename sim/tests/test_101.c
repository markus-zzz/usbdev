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

#include "../sw/usb-desc.h"
#include "usb-sim-tests.h"

extern "C" void run_test() {
  {
    UsbOutPacket out(0, 1);
    out.sendUsb();
    std::vector<uint8_t> data{0, 1, 2, 3, 4, 5, 6, 7};
    UsbData1Packet data0(data);
    data0.sendUsb();
    printResponse();
  }
  {
    UsbOutPacket out(0, 1);
    out.sendUsb();
    std::vector<uint8_t> data{0, 0, 0, 0, 0, 0, 0, 0};
    UsbData1Packet data0(data);
    data0.sendUsb();
    printResponse();
  }
}

#endif
