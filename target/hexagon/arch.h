/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_ARCH_H
#define HEXAGON_ARCH_H

#include "qemu/osdep.h"
#include "qemu/int128.h"

extern const uint8_t rLPS_table_64x4[64][4];
extern const uint8_t AC_next_state_MPS_64[64];
extern const uint8_t AC_next_state_LPS_64[64];

extern uint32_t fbrevaddr(uint32_t pointer);
extern uint32_t count_leading_ones_2(uint16_t src);
extern uint64_t interleave(uint32_t odd, uint32_t even);
extern uint64_t deinterleave(uint64_t src);
extern uint32_t carry_from_add64(uint64_t a, uint64_t b, uint32_t c);
extern int32_t conv_round(int32_t a, int n);
extern void arch_fpop_start(CPUHexagonState *env);
extern void arch_fpop_end(CPUHexagonState *env);
extern void arch_raise_fpflag(unsigned int flags);
extern int arch_sf_recip_common(int32_t *Rs, int32_t *Rt, int32_t *Rd,
                                int *adjust);
extern int arch_sf_invsqrt_common(int32_t *Rs, int32_t *Rd, int *adjust);
extern int arch_recip_lookup(int index);
extern int arch_invsqrt_lookup(int index);

#endif
