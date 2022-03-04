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


//#include "thread.h"
//#include "arch.h"
#ifndef CONFIG_USER_ONLY
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#endif
#include <assert.h>
#include "macros.h"
#include "mmvec/mmvec.h"
#include "system.h"
//#include "external_api.h"
//#include "iic.h"
//#include "uarch/uarch.h"

#define thread_t CPUHexagonState


#ifdef ZEBU_CHECKSUM_TRACE
#include "zebu_external_api.h"
#include "zebu_exec.h"
#endif


#include "string.h"
//#include "memwrap.h"

//#include "pmu.h"
//#include "isdb.h"

//#include "walk/walk.h"

//#include "clade_if.h"
//#include "clade2_if.h"

#define TLBGUESSIDX(VA) ( ((VA>>12)^(VA>>22)) & (MAX_TLB_GUESS_ENTRIES-1))

extern const size1u_t insn_allowed_uslot[][4];
extern const size1u_t insn_sitype[];
extern const size1u_t sitype_allowed_uslot[][4];
extern const char* sitype_name[];

void iic_flush_cache(processor_t * proc)

{
}

paddr_t
mem_init_access(thread_t * thread, int slot, size4u_t vaddr, int width,
				enum mem_access_types mtype, int type_for_xlate)
{
	mem_access_info_t *maptr = &thread->mem_access[slot];


#ifdef FIXME
	maptr->is_memop = 0;
	maptr->log_as_tag = 0;
	maptr->no_deriveumaptr = 0;
	maptr->is_dealloc = 0;
	maptr->dropped_z = 0;

	exception_info einfo;
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

	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
    maptr->paddr = vaddr;

	return (maptr->paddr);
}

paddr_t
mem_init_access_unaligned(thread_t *thread, int slot, size4u_t vaddr, size4u_t realvaddr, int size,
	enum mem_access_types mtype, int type_for_xlate)
{
	paddr_t ret;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	ret = mem_init_access(thread,slot,vaddr,1,mtype,type_for_xlate);
	maptr->vaddr = realvaddr;
	maptr->paddr -= (vaddr-realvaddr);
	maptr->width = size;
	return ret;
}

int
sys_xlate_dma(thread_t *thread, size8u_t va, int access_type, int maptr_type, int slot, size4u_t align_mask, xlate_info_t *xinfo, exception_info *einfo, int extended_va, int vtcm_invalid, int dlbc, int forget)
{
  int ret = 1;

  memset(einfo,0,sizeof(*einfo));
  memset(xinfo,0,sizeof(*xinfo));
  xinfo->pte_u = 1;
  xinfo->pte_w = 1;
  xinfo->pte_r = 1;
  xinfo->pte_x = 1;
  xinfo->pa = (uint64_t)va;

  return ret;
}


#define FATAL_REPLAY
void
mem_dmalink_store(thread_t * thread, size4u_t vaddr, int width, size8u_t data, int slot)
{
	FATAL_REPLAY;

	mem_access_info_t *maptr = &thread->mem_access[slot];


	maptr->is_memop = 0;
	maptr->log_as_tag = 0;
	maptr->no_deriveumaptr = 0;
	maptr->is_dealloc = 0;
	//maptr->dropped_z = 0;

	//exception_info einfo = {0};

	/* The basic stuff */
	maptr->bad_vaddr = maptr->vaddr = vaddr;
	maptr->width = width;
	maptr->type = access_type_store;
	maptr->tnum = thread->threadId;
    maptr->cancelled = 0;
    maptr->valid = 1;

	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
#if 0
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->xe = GET_SSR_FIELD(SSR_XE);
    maptr->xa = GET_SSR_FIELD(SSR_XA);

	/* For trace in the uarch */
	maptr->pc_va = thread->Regs[REG_PC];


	// Different here, we're not going to take an exception on dmlink, but the dmwait
	// if this packet has an exception, don't log the store
	if(sys_xlate_dma(thread,vaddr,TYPE_STORE,access_type_store, slot, width-1, &maptr->xlate_info, &einfo)==0) {
		SYSVERWARN("not doing dmlink store due to potential exception");
		if (!thread->processor_ptr->options->testgen_mode) {
			MEMTRACE_ST(thread, thread->Regs[REG_PC], vaddr, 0, width, DWRITE, data);
		}
		return;
	}

	maptr->paddr = maptr->xlate_info.pa;
#else
  maptr->paddr = vaddr;
#endif

	thread->mem_access[slot].stdata = data;
	thread->store_pending[slot] = 1;

	LOG_MEM_STORE(vaddr,maptr->paddr, width, data, slot);

}

void register_coproc_ldst_exception(thread_t * thread, int slot, size4u_t badva)
{
#ifdef FIXME
	warn("Coprocessor LDST Exception, tnum=%d npc=%x\n", thread->threadId, thread->Regs[REG_PC]);
	if (slot == 0) {
		register_error_exception(thread, PRECISE_CAUSE_COPROC_LDST,
			 badva,
			 thread->Regs[REG_BADVA1],
			 0 /* select 0 */,
			 1 /* set bv0 */,
			 0 /* clear bv1 */, 1<<slot);
	} else {
		register_error_exception(thread,PRECISE_CAUSE_COPROC_LDST,
			thread->Regs[REG_BADVA0],
			badva,
			1 /* select 1 */,
			0 /* clear bv0 */,
			1 /* set bv1 */, 1<<slot);
	}
#else
    printf("ERROR: register_coproc_ldst_exception not implemented\n");
    g_assert_not_reached();
#endif
}

