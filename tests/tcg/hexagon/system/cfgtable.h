/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef CFGTABLE_H
#define CFGTABLE_H

static uint32_t read_cfgtable_field(uint32_t offset)
{
    uint32_t val;
    asm volatile(
        "r0 = cfgbase\n\t"
        "r0 = asl(r0, #5)\n\t"
        "%0 = memw_phys(%1, r0)\n\t"
        : "=r"(val)
        : "r"(offset)
        : "r0");
    return val;
}

#define GET_SUBSYSTEM_BASE() (read_cfgtable_field(0x8) << 16)
#define GET_FASTL2VIC_BASE() (read_cfgtable_field(0x28) << 16)

#endif /* CFGTABLE_H */
