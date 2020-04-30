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

  while (1) {
    *R_USB_ENDP_OWNER = 0x00010000;
    while (*R_USB_ENDP_OWNER & 0x10000)
      ;
    for (int i = 0; i < 8; i++) {
      p[128 + i]++;
    }
  }
  return 0;
}
