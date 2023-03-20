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


#include <assert.h>
#include <string.h>
#include "hmx/hmx_hex_arch_types.h"
#include "hmx/hmx_coproc.h"
#include "hmx/hmx_system.h"


#define g_assert_not_reached() assert(0)
#define HEX_DEBUG_LOG(...)
#define LOG_MEM_STORE(...)

extern const size1u_t insn_allowed_uslot[][4];
extern const size1u_t insn_sitype[];
extern const size1u_t sitype_allowed_uslot[][4];
extern const char* sitype_name[];

static inline int clz32(uint32_t val)
{
    return val ? __builtin_clz(val) : 32;
}

paddr_t
hmx_mem_init_access(thread_t * thread, int slot, size4u_t vaddr, int width,
				enum mem_access_types mtype, int type_for_xlate, int page_size)
{
	mem_access_info_t *maptr = &thread->mem_access[slot];


#ifdef FIXME
	maptr->is_memop = 0;
	maptr->log_as_tag = 0;
	maptr->no_deriveumaptr = 0;
	maptr->is_dealloc = 0;
	maptr->dropped_z = 0;

        hex_exception_info einfo;
#endif

	/* The basic stuff */
#ifdef FIXME
	maptr->bad_vaddr = maptr->vaddr = vaddr;
#else
	maptr->vaddr = vaddr;
#endif
	maptr->width = width;
	maptr->type = mtype;
#ifdef FIXME
	maptr->tnum = thread->threadId;
#endif
    maptr->cancelled = 0;
    maptr->valid = 1;

    maptr->size = 31 - clz32(page_size);

	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
    maptr->paddr = vaddr;
    xlate_info_t *xinfo = &(maptr->xlate_info);
    memset(xinfo,0,sizeof(*xinfo));
    xinfo->size = maptr->size;

	return (maptr->paddr);
}

paddr_t
hmx_mem_init_access_unaligned(thread_t *thread, int slot, size4u_t vaddr, size4u_t realvaddr, int size,
	enum mem_access_types mtype, int type_for_xlate, int page_size)
{
	paddr_t ret;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	ret = hmx_mem_init_access(thread,slot,vaddr,1,mtype,type_for_xlate, page_size);
	maptr->vaddr = realvaddr;
	maptr->paddr -= (vaddr-realvaddr);
	maptr->width = size;
	return ret;
}

static inline int clz64(uint64_t val)
{
    return val ? __builtin_clzll(val) : 64;
}

static inline int clo64(uint64_t val)
{
    return clz64(~val);
}

size4u_t count_leading_ones_1(size1u_t src)
{
    int ret;
    for (ret = 0; src & 0x80; src <<= 1) {
        ret++;
    }
    return ret;
}

size4u_t count_leading_ones_8(size8u_t src)
{
    return clo64(src);
}

