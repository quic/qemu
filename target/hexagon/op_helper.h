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

#ifndef HEXAGON_OP_HELPER_H
#define HEXAGON_OP_HELPER_H

#include "internal.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"

#if 1
static inline size1u_t mem_read1(CPUHexagonState *env, paddr_t paddr)
{
    size1u_t retval;
#ifdef CONFIG_USER_ONLY
    uintptr_t ra = GETPC();
    retval = cpu_ldub_data_ra(env, paddr, ra);
#else
    hexagon_tools_memory_read(env, paddr, 1, &retval);
#endif
    return retval;
}
static inline size2u_t mem_read2(CPUHexagonState *env, paddr_t paddr)
{
    size2u_t retval;
#ifdef CONFIG_USER_ONLY
    uintptr_t ra = GETPC();
    retval = cpu_lduw_data_ra(env, paddr, ra);
#else
    hexagon_tools_memory_read(env, paddr, 2, &retval);
#endif
    return retval;
}
static inline size4u_t mem_read4(CPUHexagonState *env, paddr_t paddr)
{
    size4u_t retval;
#ifdef CONFIG_USER_ONLY
    uintptr_t ra = GETPC();
    retval = cpu_ldl_data_ra(env, paddr, ra);
#else
    hexagon_tools_memory_read(env, paddr, 4, &retval);
#endif
    return retval;
}
static inline size8u_t mem_read8(CPUHexagonState *env, paddr_t paddr)
{
    size8u_t retval;
#ifdef CONFIG_USER_ONLY
    uintptr_t ra = GETPC();
    retval = cpu_ldq_data_ra(env, paddr, ra);
#else
    hexagon_tools_memory_read(env, paddr, 8, &retval);
#endif
    return retval;
}
#endif

#endif
