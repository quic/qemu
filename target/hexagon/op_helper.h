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

static inline void log_store32(CPUHexagonState *env, target_ulong addr,
                               target_ulong val, int width, int slot)
{
    HEX_DEBUG_LOG("log_store%d(0x" TARGET_FMT_lx
                  ", %" PRId32 " [0x08%" PRIx32 "])\n",
                  width, addr, val, val);
    env->mem_log_stores[slot].va = addr;
    env->mem_log_stores[slot].width = width;
    env->mem_log_stores[slot].data32 = val;
}

static inline void log_store64(CPUHexagonState *env, target_ulong addr,
                               int64_t val, int width, int slot)
{
    HEX_DEBUG_LOG("log_store%d(0x" TARGET_FMT_lx
                  ", %" PRId64 " [0x016%" PRIx64 "])\n",
                   width, addr, val, val);
    env->mem_log_stores[slot].va = addr;
    env->mem_log_stores[slot].width = width;
    env->mem_log_stores[slot].data64 = val;
}

static inline uint8_t mem_load1(CPUHexagonState *env,
                                target_ulong vaddr)
{
    uint8_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u8(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 1, &retval);
#endif
    return retval;
}
static inline uint16_t mem_load2(CPUHexagonState *env,
                                 target_ulong vaddr)
{
    uint16_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u16(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 2, &retval);
#endif
    return retval;
}
static inline uint32_t mem_load4(CPUHexagonState *env,
                                 target_ulong vaddr)
{
    uint32_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u32(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 4, &retval);
#endif
    return retval;
}
static inline uint64_t mem_load8(CPUHexagonState *env,
                                 target_ulong vaddr)
{
    uint64_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u64(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 8, &retval);
#endif
    return retval;
}
static inline size1u_t mem_read1(CPUHexagonState *env, paddr_t paddr)
{
    size1u_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u8(retval, paddr);
#else
    hexagon_tools_memory_read(env, paddr, 1, &retval);
#endif
    return retval;
}
static inline size2u_t mem_read2(CPUHexagonState *env, paddr_t paddr)
{
    size2u_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u16(retval, paddr);
#else
    hexagon_tools_memory_read(env, paddr, 2, &retval);
#endif
    return retval;
}
static inline size4u_t mem_read4(CPUHexagonState *env, paddr_t paddr)
{
    size4u_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u32(retval, paddr);
#else
    hexagon_tools_memory_read(env, paddr, 4, &retval);
#endif
    return retval;
}
static inline size8u_t mem_read8(CPUHexagonState *env, paddr_t paddr)
{
    size8u_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u64(retval, paddr);
#else
    hexagon_tools_memory_read(env, paddr, 8, &retval);
#endif
    return retval;
}
#endif
