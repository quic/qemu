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

#ifndef HEX_COPROC_H
#define HEX_COPROC_H

typedef struct {
    int32_t opcode;
    void *vtcm_haddr;
    hwaddr vtcm_base;
    uint32_t reg_usr;
    int32_t slot;
    int32_t page_size;
    int32_t arg1;
    int32_t arg2;
} CoprocArgs;

void coproc(CoprocArgs args);

#endif
