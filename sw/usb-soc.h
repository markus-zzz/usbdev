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

#pragma once

#include <stdint.h>

#define MEM_BASE_ROM 0x00000000UL
#define MEM_BASE_RAM 0x10000000UL
#define MEM_BASE_REG 0x20000000UL

#define R_USB_ADDR               ((volatile uint32_t*)(MEM_BASE_REG + 0x0))
#define R_USB_ENDP_OWNER         ((volatile uint32_t*)(MEM_BASE_REG + 0x4))
#define R_USB_CTRL               ((volatile uint32_t*)(MEM_BASE_REG + 0x8))
#define R_USB_IN_SIZE_0_7        ((volatile uint32_t*)(MEM_BASE_REG + 0xc))
#define R_USB_IN_SIZE_8_15       ((volatile uint32_t*)(MEM_BASE_REG + 0x10))
#define R_USB_DATA_TOGGLE        ((volatile uint32_t*)(MEM_BASE_REG + 0x14))
#define R_USB_OUT_SIZE_0_7       ((volatile uint32_t*)(MEM_BASE_REG + 0x18))
#define R_USB_OUT_SIZE_8_15      ((volatile uint32_t*)(MEM_BASE_REG + 0x1c))
