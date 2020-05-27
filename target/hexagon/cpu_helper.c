/*
 *  Copyright (c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <math.h>
#include "qemu/osdep.h"
#include "cpu.h"
#include "hw/hexagon/hexagon.h"
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "qemu/log.h"
#include "tcg/tcg-op.h"
#include "internal.h"
#include "macros.h"
#include "arch.h"
#include "fma_emu.h"
#include "conv_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros.h"
#include "hex_mmu.h"
#include "op_helper.h"

#ifndef CONFIG_USER_ONLY
static size1u_t hexagon_swi_mem_read1(CPUHexagonState *env, vaddr_t paddr)

{
    size1u_t data = 0;
    unsigned mmu_idx = cpu_mmu_index(env, false);
    data = cpu_ldub_mmuidx_ra(env, paddr, mmu_idx, GETPC());
    return data;
}

static size2u_t hexagon_swi_mem_read2(CPUHexagonState *env, vaddr_t paddr)

{
    size2u_t data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 2; i++) {
        tdata = hexagon_swi_mem_read1(env, paddr + i);
        data = ((size2u_t) tdata << (8 * i)) | data;
    }

    return data;
}

static target_ulong hexagon_swi_mem_read4(CPUHexagonState *env, vaddr_t paddr)

{
    target_ulong data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 4; i++) {
        tdata = hexagon_swi_mem_read1(env, paddr + i);
        data = ((target_ulong) tdata << (8 * i)) | data;
    }

    return data;
}


static size8u_t hexagon_swi_mem_read8(CPUHexagonState *env, vaddr_t paddr)

{
    size8u_t data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 8; i++) {
        tdata = hexagon_swi_mem_read1(env, paddr + i);
        data = ((size8u_t) tdata << (8 * i)) | data;
    }

    return data;
}

static void hexagon_swi_mem_write1(CPUHexagonState *env, vaddr_t paddr,
    size1u_t value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stb_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void hexagon_swi_mem_write2(CPUHexagonState *env, vaddr_t paddr,
    size2u_t value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stw_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void hexagon_swi_mem_write4(CPUHexagonState *env, vaddr_t paddr,
    target_ulong value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stl_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void hexagon_swi_mem_write8(CPUHexagonState *env, vaddr_t paddr,
    size8u_t value)
{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stq_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

void hexagon_tools_memory_read(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr)

{
    vaddr_t paddr = vaddr;

    switch (size) {
    case 1:
        (*(size1u_t *) retptr) = hexagon_swi_mem_read1(env, paddr);
        break;
    case 2:
        (*(size2u_t *) retptr) = hexagon_swi_mem_read2(env, paddr);
        break;
    case 4:
        (*(target_ulong *) retptr) = hexagon_swi_mem_read4(env, paddr);
        break;
    case 8:
        (*(size8u_t *) retptr) = hexagon_swi_mem_read8(env, paddr);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
    }
}

void hexagon_tools_memory_write(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data)

{
    paddr_t paddr = vaddr;

    switch (size) {
    case 1:
        hexagon_swi_mem_write1(env, paddr, (size1u_t) data);
        log_store32(env, paddr, (size4u_t)data, 1, 0);
        break;
    case 2:
        hexagon_swi_mem_write2(env, paddr, (size2u_t) data);
        log_store32(env, paddr, (size4u_t)data, 2, 0);
        break;
    case 4:
        hexagon_swi_mem_write4(env, paddr, (target_ulong) data);
        log_store32(env, paddr, (size4u_t)data, 4, 0);
        break;
    case 8:
        hexagon_swi_mem_write8(env, paddr, (size8u_t) data);
        log_store32(env, paddr, data, 8, 0);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
    }
}

int hexagon_tools_memory_read_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr)

{
    vaddr_t paddr = vaddr;
    int ret = 0;

    switch (size) {
    case 4:
        (*(target_ulong *) retptr) = hexagon_swi_mem_read4(env, paddr);
        break;
    case 8:
        (*(size8u_t *) retptr) = hexagon_swi_mem_read8(env, paddr);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
        ret = 1;
        break;
     }

    return ret;
}

int hexagon_tools_memory_write_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data)

{
    paddr_t paddr = vaddr;
    int ret = 0;

    switch (size) {
    case 4:
        hexagon_swi_mem_write4(env, paddr, (target_ulong) data);
        log_store32(env, vaddr, (size4u_t)data, 4, 0);
        break;
    case 8:
        hexagon_swi_mem_write8(env, paddr, (size8u_t) data);
        log_store64(env, vaddr, data, 8, 0);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
        ret = 1;
        break;
    }

    return ret;
}

#endif
