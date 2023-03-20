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

#include "qemu/osdep.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"
#include "cpu.h"
#include "opcodes.h"
#include "coproc.h"
#include "macros.h"
#include "hmx/hmx.h"
#include "hmx/macros_auto.h"
#include "hmx/ext_hmx.h"
#include "hmx/hmx_int_ops.h"

static inline size1u_t mem_read1(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_ldub_mmuidx_ra(env, paddr, MMU_USER_IDX, GETPC());
}
static inline size2u_t mem_read2(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_lduw_mmuidx_ra(env, paddr, MMU_USER_IDX, GETPC());
}
static inline size4u_t mem_read4(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_ldl_mmuidx_ra(env, paddr, MMU_USER_IDX, GETPC());
}
static inline size8u_t mem_read8(CPUHexagonState *env, paddr_t paddr)
{
    return cpu_ldq_mmuidx_ra(env, paddr, MMU_USER_IDX, GETPC());
}

#define thread env
#define THREAD2STRUCT ((hmx_state_t *)env->processor_ptr->shared_extptr)

void coproc(CPUHexagonState *env, CoprocArgs args)
{
    switch (args.opcode) {
#include "coproc_cases_generated.c.inc"
    default:
        /* Ignore unknown opcode */
        break;
    }
}

void coproc_commit(CPUHexagonState *env)
{
    hmx_ext_commit_regs(env, 0);
    hmx_ext_commit_mem(env, 0, 0);
}

