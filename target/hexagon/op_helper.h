/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_OP_HELPER_H
#define HEXAGON_OP_HELPER_H

#include "internal.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"

#ifdef CONFIG_USER_ONLY
#define CPU_MMU_INDEX(ENV) MMU_USER_IDX
#else
#define CPU_MMU_INDEX(ENV) cpu_mmu_index((ENV), false)
#endif

static inline size1u_t mem_read1(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_ldub_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), GETPC());
}
static inline size2u_t mem_read2(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_lduw_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), GETPC());
}
static inline size4u_t mem_read4(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_ldl_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), GETPC());
}
static inline size8u_t mem_read8(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_ldq_mmuidx_ra(env, paddr, CPU_MMU_INDEX(env), GETPC());
}

/* Misc functions */
void cancel_slot(CPUHexagonState *env, uint32_t slot);
void write_new_pc(CPUHexagonState *env, bool pkt_has_multi_cof,
                  target_ulong addr, target_ulong PC);

void log_reg_write(CPUHexagonState *env, int rnum,
                   target_ulong val, uint32_t slot);
void log_store64(CPUHexagonState *env, target_ulong addr,
                 int64_t val, int width, int slot);
void log_store32(CPUHexagonState *env, target_ulong addr,
                 target_ulong val, int width, int slot);
#endif
