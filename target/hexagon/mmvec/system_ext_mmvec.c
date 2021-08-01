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

#include <stdio.h>
#include <string.h>
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "opcodes.h"
#include "insn.h"
#include "arch_options_calc.h"
#include "system.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros_auto.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#endif
#include "target/hexagon/internal.h"

#define thread_t CPUHexagonState
#define FATAL_REPLAY
#ifdef EXCEPTION_DETECTED
#undef EXCEPTION_DETECTED
#define EXCEPTION_DETECTED      (thread->status & EXEC_STATUS_EXCEPTION)
#endif

#define TYPE_LOAD 'L'
#define TYPE_STORE 'S'
#define TYPE_FETCH 'F'
#define TYPE_ICINVA 'I'

// Get Arch option through thread
#define ARCH_OPT_TH(OPTION) (thread->processor_ptr->arch_proc_options->OPTION)

static inline int vmemu_vtcm_page_cross(thread_t *thread, int in_tcm_new)
{
#if 0
    if (thread->last_pkt->double_access && thread->last_pkt->pkt_has_vmemu_access && thread->last_pkt->pkt_access_count==1)
    {
        if((thread->last_pkt->pkt_has_vtcm_access ^ in_tcm_new))
        {
            warn("VMEMU Crossed VTCM boundry (In VTCM First Access: %d Second Access: %d)", thread->last_pkt->pkt_has_vtcm_access, in_tcm_new);
        return 1;
        }
    }
#endif
    return 0;
}

static inline int check_vmem_and_zmem(thread_t *thread)
{
#if 0
    if (((thread->last_pkt->pkt_ldaccess_vtcm==2) || (thread->last_pkt->pkt_ldaccess_l2==2)) && (!thread->last_pkt->double_access_vec))
    {
        warn("Packet has VMEM & ZMEM to same memory location VTCM=%d L2=%d", thread->last_pkt->pkt_ldaccess_vtcm, thread->last_pkt->pkt_ldaccess_l2);
        return 1;
    }
#endif
    return 0;
}

static inline int in_valid_coproc_memory_space(thread_t *thread, paddr_t paddr)
{
#if 0
    int in_vtcm = in_vtcm_space(thread->processor_ptr, paddr, HIDE_WARNING);
    int in_tcm = in_l2tcm_space(thread->processor_ptr, paddr, HIDE_WARNING);

    if (in_tcm) {
        return 1;
    }


    if (in_vtcm) {
        return 1;
    }


    // Check AHB
    paddr_t ahb_lowaddr = thread->processor_ptr->arch_proc_options->ahb_lowaddr;
    paddr_t ahb_highaddr = thread->processor_ptr->arch_proc_options->ahb_highaddr;
	if (((ahb_lowaddr != 0) && (ahb_highaddr != ahb_lowaddr)) && (ARCH_OPT_TH(AHB_ENABLE))) {
        int in_ahb = ((paddr < ahb_highaddr) && (paddr >= ahb_lowaddr));
        if (in_ahb) {
            warn("Vector LD/ST in AHB space causes error (%016llx <= %016llx < %016llx) ",ahb_lowaddr,paddr,ahb_highaddr);
            return 0;
        }
    }

    // Check AXI2
#if 0
    paddr_t axi2_lowaddr = ARCH_OPT_TH(axi2_lowaddr);
    paddr_t axi2_highaddr = ARCH_OPT_TH(axi2_highaddr);
    if (axi2_lowaddr != 0) {
       int in_axi2 = ((paddr < axi2_highaddr) && (paddr >= axi2_lowaddr));
       if (in_axi2) {
            warn("Vector LD/ST in AXI2 space causes error (%016llx <= %016llx < %016llx)",axi2_lowaddr,paddr,axi2_highaddr);
            return 0;
       }
    }
#endif
#endif
    return 1;
}

static inline int check_gather_store(thread_t* thread, Insn * insn)
{
    /* First check to see if temp vreg has been updated */
    int check  = thread->gather_issued;
#if 0
    /* This field has been removed from the runtime state */
    check &= thread->is_gather_store_insn;
#else
    /* This function should never be called, be defensive about it */
    g_assert_not_reached();
#endif

    /* In case we don't have store, suppress gather */
    if (!check) {
        thread->gather_issued = 0;
        thread->vtcm_pending = 0;   /* Suppress any gather writes to memory */
    }
    return check;
}


#if 0
void gather_activeop(thread_t *thread, paddr_t paddr, int size) {
    paddr_t paddr_byte = paddr;
    paddr_t final_paddr = (paddr_t) paddr + size;

    for(; paddr_byte < final_paddr; paddr_byte++) {
      	enlist_byte(thread, paddr_byte, POISON, 2); //rw=2, write-poison
    }
  	enlist_byte(thread, paddr, SYNCED, 0);
}
#endif

void mem_load_vector_oddva(thread_t* thread, Insn * insn, vaddr_t vaddr, vaddr_t lookup_vaddr, int slot, int size, size1u_t* data, int etm_size, int use_full_va)
{
    enum ext_mem_access_types access_type;

	int i;

	if (!use_full_va) {
		lookup_vaddr = vaddr;
	}


    FATAL_REPLAY;
    if (!size) return;

    access_type=access_type_vload;
    mem_init_access_unaligned(thread, slot, lookup_vaddr, vaddr, size, access_type, TYPE_LOAD);
    if (EXCEPTION_DETECTED) return;




    if (EXCEPTION_DETECTED) return;

    for (i=0;i<size;i++) {
        //data[i] = sim_mem_read1(thread->system_ptr, thread->threadId, paddr);
        hexagon_load_byte(thread, &data[i], vaddr);
        vaddr = vaddr + 1;
    }


    for (i=0; i<size; i++) {
        thread->mem_access[slot].cdata[i] = data[i];
    }
    fVDOCHKPAGECROSS(vaddr, vaddr+size);
}

#if 0
void mem_fetch_vector(thread_t* thread, Insn * insn, vaddr_t vaddr, int slot, int size)
{
	enum ext_mem_access_types access_type = access_type_vfetch;


    FATAL_REPLAY;
    if (!size) return;

    mem_init_access(thread, slot, vaddr, size, access_type, TYPE_LOAD);
    if (EXCEPTION_DETECTED) return;


}
#endif

void mem_store_vector_oddva(thread_t* thread, Insn * insn, vaddr_t vaddr, vaddr_t lookup_vaddr, int slot, int size, size1u_t* data, size1u_t* mask, unsigned invert, int use_full_va)
{
    enum ext_mem_access_types access_type;
    int i;
    //mmvecx_t *mmvecx = thread; //THREAD2STRUCT;
    thread_t *mmvecx = thread; //THREAD2STRUCT;

	if (!use_full_va) {
		lookup_vaddr = vaddr;
	}

    if (!size) return;
    FATAL_REPLAY;





    access_type=access_type_vstore;
    mem_init_access_unaligned(thread, slot, lookup_vaddr, vaddr, size, access_type, TYPE_STORE);
    if (EXCEPTION_DETECTED) return;

    int in_tcm = in_vtcm_space(thread->processor_ptr,vaddr, HIDE_WARNING);
    int is_gather_store = check_gather_store(thread, insn); /* Right Now only gather stores temp */
    //printf("mem_store_vector_oddva thread->last_pkt->double_access=%d in_tcm=%d paddr=%llx\n", thread->last_pkt->double_access, in_tcm, paddr);

    if (is_gather_store) {
        memcpy(data, &mmvecx->tmp_VRegs[0].ub[0], size);
        mmvecx->VRegs_updated_tmp = 0;
        mmvecx->gather_issued = 0;
    }

    if (EXCEPTION_DETECTED) return;

    // If it's a gather store update store data from temporary register
    // And clear flag



    mmvecx->vstore_pending[slot] = 1;
    mmvecx->vstore[slot].va   = vaddr;
    mmvecx->vstore[slot].size = size;
    memcpy(&mmvecx->vstore[slot].data.ub[0], data, size);
    if (!mask) {
        memset(&mmvecx->vstore[slot].mask.ub[0], invert ? 0 : -1, size);
    } else if (invert) {
        for (i=0; i<size; i++) {
            mmvecx->vstore[slot].mask.ub[i] = !mask[i];
        }
    } else {
        memcpy(&mmvecx->vstore[slot].mask.ub[0], mask, size);
    }


    // On a gather store, overwrite the store mask to emulate dropped gathers
    if (is_gather_store) {
        memcpy(&mmvecx->vstore[slot].mask.ub[0], &mmvecx->vtcm_log.mask.ub[0], size);

        // Store all zeros
        if (!in_tcm)
        {
             memset(&mmvecx->vstore[slot].mask.ub[0], 1, size);
             memset(&mmvecx->vstore[slot].data.ub[0], 0, size);
        }
    }
    for (i=0; i<size; i++) {
        thread->mem_access[slot].cdata[i] = data[i];
    }

    fVDOCHKPAGECROSS(vaddr, vaddr+size);

    return;
}

static int check_scatter_gather_page_cross(thread_t* thread, vaddr_t base, int length, int page_size)
{

    return 0;
}

void mem_gather_store(CPUHexagonState *env, target_ulong vaddr,
                      int slot, uint8_t *data)
{
    size_t size = sizeof(MMVector);

    /*
     * If it's a gather store update store data from temporary register
     * and clear flag
     */
    memcpy(data, &env->tmp_VRegs[0].ub[0], size);
    env->VRegs_updated_tmp = 0;
    env->gather_issued = false;

    env->vstore_pending[slot] = 1;
    env->vstore[slot].va   = vaddr;
    env->vstore[slot].size = size;
    memcpy(&env->vstore[slot].data.ub[0], data, size);

    /* On a gather store, overwrite the store mask to emulate dropped gathers */
    memcpy(&env->vstore[slot].mask.ub[0], &env->vtcm_log.mask.ub[0], size);
}

void mem_vector_scatter_init(thread_t* thread, Insn * insn, vaddr_t base_vaddr, int length, int element_size)
{
    enum ext_mem_access_types access_type=access_type_vscatter_store;
    // Translation for Store Address on Slot 1 - maybe any slot?
    int slot = insn->slot;
    mem_init_access(thread, slot, base_vaddr, 1, access_type, TYPE_STORE);
    mem_access_info_t * maptr = &thread->mem_access[slot];
    if (EXCEPTION_DETECTED) return;
    //mmvecx_t *mmvecx = thread; //THREAD2STRUCT;
    thread_t *mmvecx = thread; //THREAD2STRUCT;


    thread->mem_access[slot].paddr = thread->mem_access[slot].paddr & ~(element_size-1);   // Align to element Size

	int in_tcm = in_vtcm_space(thread->processor_ptr,base_paddr,SHOW_WARNING);
    if (maptr->xlate_info.memtype.device && !in_tcm) register_coproc_ldst_exception(thread,slot,base_vaddr);

    int scatter_gather_exception =  (length < 0);
    scatter_gather_exception |= !in_tcm;
    scatter_gather_exception |= !in_vtcm_space(thread->processor_ptr,base_paddr+length, SHOW_WARNING);
    scatter_gather_exception |= check_scatter_gather_page_cross(thread,base_vaddr,length, thread->mem_access[slot].xlate_info.size);
    if (scatter_gather_exception)
        register_coproc_ldst_exception(thread,slot,base_vaddr);

    if (EXCEPTION_DETECTED) return;

	maptr->range = length;

    int i = 0;
    for(i = 0; i < fVECSIZE(); i++) {
        mmvecx->vtcm_log.offsets.ub[i] = 0; // Mark invalid
        mmvecx->vtcm_log.data.ub[i] = 0;
        mmvecx->vtcm_log.mask.ub[i] = 0;
        //mmvecx->vtcm_log.pa[i] = 0;
    }
    mmvecx->vtcm_log.va_base = base_vaddr;
//    mmvecx->vtcm_log.pa_base = base_paddr;

    mmvecx->vtcm_pending = 1;
    mmvecx->vtcm_log.oob_access = 0;
    mmvecx->vtcm_log.op = 0;
    mmvecx->vtcm_log.op_size = 0;
    return;
}


void mem_vector_gather_init(thread_t* thread, Insn * insn, vaddr_t base_vaddr,  int length, int element_size)
{

    int slot = insn->slot;
    enum ext_mem_access_types access_type = access_type_vgather_load;
    mem_init_access(thread, slot, base_vaddr, 1,  access_type, TYPE_LOAD);
    mem_access_info_t * maptr = &thread->mem_access[slot];
    mmvecx_t *mmvecx = thread ;//THREAD2STRUCT;

    if (EXCEPTION_DETECTED) return;



    thread->mem_access[slot].paddr = thread->mem_access[slot].paddr & ~(element_size-1);   // Align to element Size

    // Need to Test 4 conditions for exception
    // M register is positive
    // Base and Base+Length-1 are in TCM
    // Base + Length doesn't cross a page
	int in_tcm = in_vtcm_space(thread->processor_ptr,base_paddr, SHOW_WARNING);
    if (maptr->xlate_info.memtype.device && !in_tcm) register_coproc_ldst_exception(thread,slot,base_vaddr);

    int scatter_gather_exception =  (length < 0);
    scatter_gather_exception |= !in_tcm;
    scatter_gather_exception |= !in_vtcm_space(thread->processor_ptr,base_paddr+length, SHOW_WARNING);
    scatter_gather_exception |= check_scatter_gather_page_cross(thread,base_vaddr,length, thread->mem_access[slot].xlate_info.size);
    if (scatter_gather_exception)
        register_coproc_ldst_exception(thread,slot,base_vaddr);

    if (EXCEPTION_DETECTED) return;

	maptr->range = length;

    int i = 0;
    for(i = 0; i < 2*fVECSIZE(); i++) {
        mmvecx->vtcm_log.offsets.ub[i] = 0x0;
    }
    for(i = 0; i < fVECSIZE(); i++) {
        mmvecx->vtcm_log.data.ub[i] = 0;
        mmvecx->vtcm_log.mask.ub[i] = 0;
        //mmvecx->vtcm_log.pa[i] = 0;
        mmvecx->tmp_VRegs[0].ub[i] = 0;
    }
    mmvecx->vtcm_log.oob_access = 0;
    mmvecx->vtcm_log.op = 0;
    mmvecx->vtcm_log.op_size = 0;

    mmvecx->vtcm_log.va_base = base_vaddr;
//    mmvecx->vtcm_log.pa_base = base_paddr;

    // Temp Reg gets updated
    // This allows Store .new to grab the correct result
    mmvecx->VRegs_updated_tmp = 0xFFFFFFFF;
    mmvecx->gather_issued = 1;

    return;
}



void mem_vector_scatter_finish(thread_t* thread, Insn * insn, int op)
{

    int slot = insn->slot;
    mmvecx_t *mmvecx = thread;//THREAD2STRUCT;
    thread->store_pending[slot] = 0;
    mmvecx->vstore_pending[slot] = 0;
    mmvecx->vtcm_log.size = fVECSIZE();


    memcpy(thread->mem_access[slot].cdata, &mmvecx->vtcm_log.offsets.ub[0], 256);





    return;
}

void mem_vector_gather_finish(thread_t* thread, Insn * insn)
{
    // Gather Loads
    int slot = insn->slot;
    mmvecx_t *mmvecx = thread;//THREAD2STRUCT;


	memcpy(thread->mem_access[slot].cdata, &mmvecx->vtcm_log.offsets.ub[0], 256);


}
