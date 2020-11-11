/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

//#include <arch/thread.h>
//#include <arch/memwrap.h>
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#include "system.h"
//#include <arch/external_api.h>
#include "opcodes.h"
#include "insn.h"
//#include "pmu.h"
#include "hmx/system_ext_hmx.h"
#include "hmx/mpy_hmx.h"
#include "arch_options_calc.h"

#define thread_t         CPUHexagonState
#define THREAD2STRUCT ((hmx_state_t*)env->processor_ptr->shared_extptr)

// Get Arch option through thread
#define ARCH_OPT_TH(OPTION) (thread->processor_ptr->arch_proc_options->OPTION)
#define MEMTRACE_LD(...)
#define memwrap_memtrace_ld(...)
#define memwrap_memtrace_st(...)
#define warn(...)
#define register_coproc_ldst_exception(...) {}


#define check_mxmem_page_cross(thread, base, length, page_size)   0

#ifdef FIXME
#define MX_EXCEPTION_CHECK(VA,LEN,LEN2,TYPE1, TYPE2)\
	mem_init_access_unaligned(thread, slot, VA, VA, LEN, TYPE1, TYPE2);\
    if (EXCEPTION_DETECTED) return;\
	paddr = maptr->paddr;\
	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;\
    mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);\
    mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr+LEN, SHOW_WARNING);\
    mxmem_exception |= check_mxmem_page_cross(thread,VA,LEN2, thread->mem_access[slot].xlate_info.size);\
    if (mxmem_exception) {\
        register_coproc_ldst_exception(thread,slot,VA);\
	}\
	if (EXCEPTION_DETECTED) return;
#else

#define check_mxmem_page_cross(thread, base, length, page_size)   0

#define MX_EXCEPTION_CHECK(VA,LEN,LEN2,TYPE1, TYPE2)\
	  mem_init_access_unaligned(thread, slot, VA, VA, LEN, TYPE1, TYPE2);\
    {\
        if (EXCEPTION_DETECTED) return;\
	      paddr = maptr->paddr;\
	      int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;\
        mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);\
        mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr+LEN, SHOW_WARNING);\
        mxmem_exception |= check_mxmem_page_cross(thread,VA,LEN2, thread->mem_access[slot].xlate_info.size);\
        if (mxmem_exception) {\
            register_coproc_ldst_exception(thread,slot,VA);\
	      }\
	      if (EXCEPTION_DETECTED) return;\
    }


#endif



void hmx_bias_init(thread_t* thread, int slot, vaddr_t vaddr, int access_type, int size)
{
  thread_t *env = thread;
	paddr_t paddr;
	mem_access_info_t *maptr = &thread->mem_access[slot = 0];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;

	int type = (access_type == access_type_hmx_load_bias) ? TYPE_LOAD : TYPE_STORE;

	if(size / thread->processor_ptr->arch_proc_options->hmx_output_depth == 8)
		hmx_maptr->bias_32bit = 1;
	else if (size / thread->processor_ptr->arch_proc_options->hmx_output_depth == 4)
		hmx_maptr->bias_32bit = 0;
	else {
		warn(":unknown bias data type, neither 16bit nor 32bit ");
  }

	mem_init_access_unaligned(thread, slot, vaddr, vaddr, size, access_type, type);

    if (EXCEPTION_DETECTED) return;
	maptr->paddr = paddr = maptr->paddr & ~(size-1);

	maptr->width = size;

	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;
	mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);
	mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr + size-1, SHOW_WARNING);
    if (mxmem_exception) {
        register_coproc_ldst_exception(thread,slot,vaddr);
	}
	if (EXCEPTION_DETECTED) return;

#ifdef VERIFICATION
    int slot_tmp = thread->ver_cur_slot;
    thread->ver_cur_slot = slot;
#endif
	if(!thread->bq_on) {
		if (access_type == access_type_hmx_load_bias) {
			memwrap_memtrace_ld(thread, thread->Regs[REG_PC], vaddr, paddr, 1);
		} else {
			memwrap_memtrace_st(thread, thread->Regs[REG_PC], vaddr, paddr, 1);
		}
	}
#ifdef VERIFICATION
    thread->ver_cur_slot = slot_tmp;
#endif

}

//static const char * hmx_act_type_str[] = {"BLOCK", "DEEP", "ABOVE", "SINGLE", "DILATE", "BATCH"};
//static const char * hmx_wei_type_str[] = {"NORMAL", "DEEP", "AFTER", "SINGLE", "DILATE", "DROP"};

void hmx_activation_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int format, int block_type)
{
  thread_t *env = thread;
    paddr_t paddr;
    mem_access_info_t *maptr = &thread->mem_access[slot=1]; // MGL: slot must be set to 1
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;

	vaddr_t addr_mask = fMX_GETADDRMASK(thread->processor_ptr);
	int dY = range & addr_mask;

	MX_EXCEPTION_CHECK(start, dY, dY, access_type_hmx_load_act, TYPE_LOAD);

	maptr->vaddr = start;

	memset(hmx_maptr, 0, sizeof(hmx_mem_access_info_t));


	maptr->range = range;
	hmx_maptr->format = format;
	hmx_maptr->block_type = block_type;

	maptr->width = 2048;

	size4u_t mask = ((1 << fMX_GETCHANNELSIZE(thread->processor_ptr))-1) << format;
	hmx_maptr->ch_start = ((start & mask) >> format) & ~0x3;
	hmx_maptr->ch_stop = (((range & mask) >> format) | 0x3)+1;

	hmx_maptr->dY = dY;

	fMX_GETMASK(THREAD2STRUCT->tile_x_mask, ~range, format, 0);
	fMX_GETMASK(THREAD2STRUCT->tile_y_mask,  range, format, 0);


	fMX_MASKED_INC(THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_x_mask);
	fMX_MASKED_INC(THREAD2STRUCT->tile_y_inc, THREAD2STRUCT->tile_y_mask);

	hmx_maptr->blocks = 1;
	hmx_maptr->fx = start & THREAD2STRUCT->tile_x_mask;
	hmx_maptr->fy = start & THREAD2STRUCT->tile_y_mask;
	hmx_maptr->acc_select = THREAD2STRUCT->current_acc_fxp;


	if (block_type == HMX_ACT_DEEP) {
		if (hmx_maptr->dY < 0) {
			hmx_maptr->blocks = 0;
		} else {
			int blocks = (range & addr_mask) >> get_hmx_block_bit(thread->processor_ptr);
			hmx_maptr->blocks = blocks+1;
		}
		hmx_maptr->y_tap = 1;
	} else if ((block_type == HMX_ACT_BLOCK) || (block_type == HMX_ACT_DILATE) || (block_type == HMX_ACT_BATCH)) {
		hmx_maptr->y_stop = hmx_maptr->fy;
		hmx_maptr->y_dilate = (block_type == HMX_ACT_DILATE);
		hmx_maptr->batch = (block_type == HMX_ACT_BATCH);
		if (THREAD2STRUCT->tile_y_mask!=0) {
			fMX_COMPUTER_FILTER_SIZE(hmx_maptr->y_tap, hmx_maptr->fy, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc);
		} else {
			hmx_maptr->y_tap = 1;
		}
		if (hmx_maptr->batch) {
			hmx_maptr->y_tap = 1;
		}

	} else if (block_type == HMX_ACT_ABOVE) {
		hmx_maptr->y_start = hmx_maptr->fy;
		hmx_maptr->y_stop  = THREAD2STRUCT->tile_y_mask;
		if (THREAD2STRUCT->tile_y_mask!=0) {
			fMX_COMPUTER_FILTER_SIZE_ABOVE(hmx_maptr->y_tap, hmx_maptr->fy, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc);
			if (hmx_maptr->y_tap == 0)
				hmx_maptr->y_tap = 1;
		} else {
			hmx_maptr->y_tap = 1;
		}
	} else if (block_type == HMX_ACT_SINGLE) {
		hmx_maptr->y_start = hmx_maptr->fy;
		hmx_maptr->y_stop = hmx_maptr->fy;// fMX_INC_MASKED(hmx_maptr->fy, THREAD2STRUCT->tile_y_inc, (THREAD2STRUCT->tile_y_mask | (1<<31)));
		hmx_maptr->y_tap = 1;
	}

	if (THREAD2STRUCT->tile_x_mask!=0) {
		fMX_COMPUTER_FILTER_SIZE(hmx_maptr->x_tap, hmx_maptr->fx, THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc);
	} else {
		hmx_maptr->x_tap = 1;
	}
	if (block_type == HMX_ACT_DEEP) {
		int limit = thread->processor_ptr->arch_proc_options->QDSP6_MX_ROWS;
		//limit = (limit + hmx_maptr->x_tap - 1) / hmx_maptr->x_tap;

		if (hmx_maptr->blocks > limit) {
			warn(":deep block count=%d exceeded a max of %d. Limiting to max", hmx_maptr->blocks, limit);
			hmx_maptr->blocks = limit;
		}
	}
	THREAD2STRUCT->paddr[slot] = paddr & (paddr_t) fMX_GETADDRMASK(thread->processor_ptr);
	maptr->paddr = paddr & (paddr_t) fMX_GETADDRMASK(thread->processor_ptr);
	THREAD2STRUCT->type[slot] = type;

	THREAD2STRUCT->fxp_commit_state.acc_update = 0;

	THREAD2STRUCT->mac_cycle_limit = 512;



	THREAD2STRUCT->operand_ready |= (hmx_maptr->blocks>0)<<slot;

#ifdef VERIFICATION
    int slot_tmp = thread->ver_cur_slot;
    thread->ver_cur_slot = slot;
#endif
	if(!thread->bq_on) { MEMTRACE_LD(thread, thread->Regs[REG_PC], start, paddr, 0, DREAD, 0xfeedfacedeadbeefULL); }
#ifdef VERIFICATION
    thread->ver_cur_slot = slot_tmp;
	warn("MXMEM Debug %s Activations pktid=%x pktid=%d: Start=%x (PA=%llx) Range: %x dY: %x block_type=%s", "FXP", thread->pktid,thread->pktid, start, maptr->paddr, range, hmx_maptr->dY,  hmx_act_type_str[block_type]);
	warn("Spatial: tile_x_mask: 0x%x tile_x_inc: 0x%x tile_y_mask: 0x%x tile_y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc);
	warn("block count in loop %x channel start=%d stop=%d", hmx_maptr->blocks,  hmx_maptr->ch_start, hmx_maptr->ch_stop);
	warn("mac_cycle_limit: %d", THREAD2STRUCT->mac_cycle_limit);
	fMX_VERIF_DEBUG_LOG(1, "MXMEM Debug %s Mult pktid:%08x: Start=%x (PA=%llx) Range: %x dY: %x block_Type=%s", "FXP", thread->pktid, start, maptr->paddr, range, hmx_maptr->dY, hmx_act_type_str[block_type]);
	fMX_VERIF_DEBUG_LOG(1, "\tTile: x_mask: 0x%x x_inc: 0x%x y_mask: 0x%x y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc);
	fMX_VERIF_DEBUG_LOG(1, "\tBlock count in loop %x Channel start=%d stop=%d y start=%x y_stop = %x", hmx_maptr->blocks,  hmx_maptr->ch_start, hmx_maptr->ch_stop, hmx_maptr->y_start, hmx_maptr->y_stop);
	fMX_VERIF_DEBUG_LOG(1, "\tmac_cycle_limit: %d", THREAD2STRUCT->mac_cycle_limit);
	if (hmx_maptr->blocks == 0) {
			warn("!Number of blocks is zero so packet dropped!");
			fMX_VERIF_DEBUG_LOG(1, "\tNumber of blocks is zero so packet dropped");
		}
#endif
	fMX_DEBUG_LOG(0, "MXMEM Debug %s Mult: Start=%x (PA=%llx) Range: %x dY: %x block_Type=%s", "FXP", start, maptr->paddr, range, hmx_maptr->dY, hmx_act_type_str[block_type]);
	fMX_DEBUG_LOG(0, "\tTile: x_mask: 0x%x x_inc: 0x%x y_mask: 0x%x y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc);
	fMX_DEBUG_LOG(0, "\tBlock count in loop %x Channel start=%d stop=%d y start=%x y_stop = %x", hmx_maptr->blocks,  hmx_maptr->ch_start, hmx_maptr->ch_stop, hmx_maptr->y_start, hmx_maptr->y_stop);
	fMX_DEBUG_LOG(0, "\tmac_cycle_limit: %d", THREAD2STRUCT->mac_cycle_limit);
	if (hmx_maptr->blocks == 0) {
		fMX_DEBUG_LOG(0, "\tNumber of blocks is zero so packet dropped");
	}


}


int get_fp_behavior(thread_t* thread)  {
  return 0;
}

void hmx_weight_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int block_type, int weight_count)
{
  thread_t *env = thread;
    paddr_t paddr;
    mem_access_info_t *maptr = &thread->mem_access[slot=0]; // MGL: weights must be in slot 0
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;
	int length = (int)range;

	paddr_t last_wgt_mask = (thread->processor_ptr->arch_proc_options->hmx_output_depth * 4)-1;
	paddr_t wgt_align_mask = ~last_wgt_mask;



	mem_init_access_unaligned(thread, slot, start, start, length, access_type_hmx_load_wei, TYPE_LOAD);

    if (EXCEPTION_DETECTED) return;
  paddr = maptr->paddr;
	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;
    mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);
    mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr + length, SHOW_WARNING);
    mxmem_exception |= check_mxmem_page_cross(thread, start , length, thread->mem_access[slot].xlate_info.size);
    if (mxmem_exception) {
        register_coproc_ldst_exception(thread,slot,start);
	}
	if (EXCEPTION_DETECTED) return;

	maptr->width = length;
	maptr->vaddr = start;
	hmx_mem_access_info_t * hmx_act_maptr = &thread->mem_access[1].hmx_ma;

	maptr->hmx_ma = *hmx_act_maptr; // Copy Parameters from Activations

	maptr->range = range;

	hmx_maptr->weight_count = weight_count;

	hmx_maptr->weight_bits = THREAD2STRUCT->weight_bits;

	paddr = paddr & wgt_align_mask;
	maptr->paddr = paddr;
	THREAD2STRUCT->max_weight_pa = (paddr + range) | last_wgt_mask;
	THREAD2STRUCT->limit_execeeded = 0;
	THREAD2STRUCT->force_zero = 0;
	THREAD2STRUCT->weight_bits = 8;
	paddr_t limit = thread->processor_ptr->arch_proc_options->QDSP6_MX_ROWS * thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS;
	limit = limit * (1 << fMX_GETCHANNELSIZE(thread->processor_ptr));

	if ( (range | last_wgt_mask) & ~(limit-1)) {
		//printf("Weight size limit exceeded: %llx limit: %llx %x %llx", (range | last_wgt_mask), limit, range, last_wgt_mask);
		warn("Weight size limit exceeded: %llx limit: %llx ", (range | last_wgt_mask), limit);
		THREAD2STRUCT->max_weight_pa = paddr + limit - 1 ;
	}



	hmx_maptr->block_type = block_type;
	hmx_maptr->acc_range = hmx_act_maptr->acc_range = (hmx_act_maptr->fx > 0);
	hmx_maptr->wgt_deep  = 0;

	if ((block_type == HMX_WEI_NORMAL) || (block_type == HMX_WEI_DILATE) || (block_type == HMX_WEI_DROP)) {
		hmx_maptr->x_stop   = hmx_act_maptr->x_stop  = hmx_maptr->fx;
		hmx_maptr->x_dilate = hmx_act_maptr->x_dilate  = (block_type == HMX_WEI_DILATE);
		hmx_maptr->drop = hmx_act_maptr->drop = (block_type == HMX_WEI_DROP);
		if (hmx_maptr->drop) {
			hmx_maptr->acc_range = hmx_act_maptr->acc_range = 0;
		}
	} else if (block_type == HMX_WEI_DEEP) {
		hmx_maptr->deep = hmx_act_maptr->deep = 1;
		hmx_maptr->wgt_deep = hmx_act_maptr->wgt_deep = 1;
		hmx_maptr->acc_range = hmx_act_maptr->acc_range = 0;
		hmx_maptr->x_stop   = hmx_act_maptr->x_stop  = hmx_maptr->fx = 0;
		hmx_maptr->x_tap   = hmx_act_maptr->x_tap = 1;		// Force to single Tap
	} else if (block_type == HMX_WEI_AFTER) {
		hmx_maptr->x_start = hmx_act_maptr->x_start = hmx_act_maptr->fx;
		hmx_maptr->x_stop  = hmx_act_maptr->x_stop  = THREAD2STRUCT->tile_x_mask;
		hmx_maptr->x_tap = 0;
		if (THREAD2STRUCT->tile_x_mask!=0) {
			fMX_COMPUTER_FILTER_SIZE_ABOVE(hmx_maptr->x_tap, hmx_act_maptr->fx, THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc);
			hmx_act_maptr->x_tap = hmx_maptr->x_tap;
		} else {
			hmx_act_maptr->x_tap = hmx_maptr->x_tap = 1;
		}
	} else if (block_type == HMX_WEI_SINGLE) {
		hmx_maptr->x_tap   = hmx_act_maptr->x_tap = 1;		// Force to single Tap
		hmx_maptr->x_start = hmx_act_maptr->x_start = hmx_act_maptr->fx;
		hmx_maptr->x_stop  = hmx_act_maptr->x_stop  = hmx_act_maptr->fx;// fMX_INC_MASKED(hmx_act_maptr->fx, THREAD2STRUCT->tile_x_inc, (THREAD2STRUCT->tile_x_mask | (1<<31)));

	}


	THREAD2STRUCT->usr_fp = get_fp_behavior(thread);

	THREAD2STRUCT->paddr[slot] = paddr;
	THREAD2STRUCT->type[slot] = type;

	THREAD2STRUCT->fxp_commit_state.acc_update = 0;
	THREAD2STRUCT->operand_ready |= ((int)range>=0)<<slot;

	fMX_DEBUG_LOG(0, "MXMEM Debug Weights: Start=%x Range: %x Type=%s max pa=%llx", start, range, hmx_wei_type_str[block_type], THREAD2STRUCT->max_weight_pa);
	fMX_DEBUG_LOG(0, "\tFilter: fx: %d fy: %d Actual Size: %d by %d", hmx_maptr->fx, hmx_maptr->fy, hmx_maptr->x_tap, hmx_maptr->y_tap);
	fMX_DEBUG_LOG(0, "\tAccumulator Start: %d  Range: %d", hmx_maptr->acc_select, hmx_maptr->acc_range);
	fMX_DEBUG_LOG(0, "\tFlags dilate: %d %d deep: %d drop: %d IEEE FP RND: %x", hmx_maptr->x_dilate, hmx_maptr->y_dilate, hmx_maptr->deep, hmx_maptr->drop, THREAD2STRUCT->usr_fp);

	if (((int)range<0)) {
		hmx_maptr->blocks = 0;
		fMX_DEBUG_LOG(0, "\tRange Negative so packet dropped");
	}


#ifdef VERIFICATION
    int slot_tmp = thread->ver_cur_slot;
    thread->ver_cur_slot = slot;
	warn("MXMEM Debug Weights pktid=%8x (%d): Start=%x Range: %x Type=%s max pa=%llx", thread->pktid,thread->pktid, start, range, hmx_wei_type_str[block_type], THREAD2STRUCT->max_weight_pa);
	warn("Filter: fx: %d fy: %d Actual Size: %d by %d", hmx_maptr->fx, hmx_maptr->fy, hmx_maptr->x_tap, hmx_maptr->y_tap);
	warn("Accumulator Start: %d  Range: %d", hmx_maptr->acc_select, hmx_maptr->acc_range);
	warn("Flags dilate: %d %d deep: %d drop: %d", hmx_maptr->x_dilate, hmx_maptr->y_dilate, hmx_maptr->deep, hmx_maptr->drop);
	fMX_VERIF_DEBUG_LOG(1, "MXMEM Debug Weights pktid:%08x: Start=%x Range: %x Type=%s max pa=%llx", thread->pktid, start, range, hmx_wei_type_str[block_type], THREAD2STRUCT->max_weight_pa);
	fMX_VERIF_DEBUG_LOG(1, "\tFilter: fx: %d fy: %d Actual Size: %d by %d", hmx_maptr->fx, hmx_maptr->fy, hmx_maptr->x_tap, hmx_maptr->y_tap);
	fMX_VERIF_DEBUG_LOG(1, "\tAccumulator Start: %d  Range: %d", hmx_maptr->acc_select, hmx_maptr->acc_range);
	fMX_VERIF_DEBUG_LOG(1, "\tFlags dilate: %d %d deep: %d drop: %d IEEE FP RND: %x", hmx_maptr->x_dilate, hmx_maptr->y_dilate, hmx_maptr->deep, hmx_maptr->drop, THREAD2STRUCT->usr_fp);

	if (((int)range<0)) {
			warn("!Range Negative so packet dropped!");
		fMX_VERIF_DEBUG_LOG(1, "\tRange Negative so packet dropped");
		}
#endif
	if(!thread->bq_on) { MEMTRACE_LD(thread, thread->Regs[REG_PC], start, paddr, 0, DREAD, 0xfeedfacedeadbeefULL); }
#ifdef VERIFICATION
    thread->ver_cur_slot = slot_tmp;
#endif

}



void hmx_convert_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t start, vaddr_t range, int format, int direction, int type)
{
  thread_t *env= thread;
    paddr_t paddr;
    paddr_t tile_mask = (1 << (thread->processor_ptr->arch_proc_options->hmx_spatial_size + fMX_GETCHANNELSIZE(thread->processor_ptr)))-1;
	mem_access_info_t *maptr = &thread->mem_access[slot = 0];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;
	hmx_maptr->dY =  range & ~tile_mask;

	// Exception Checking
	MX_EXCEPTION_CHECK(start, hmx_maptr->dY, hmx_maptr->dY, access_type_hmx_store, TYPE_STORE);
	paddr = paddr & ~tile_mask;


	// MAPTR stuff needed by timing model
	maptr->paddr = paddr;
	maptr->width = 2048;
	hmx_maptr->format = format;

	hmx_maptr->acc_select = THREAD2STRUCT->current_acc_fxp;

		THREAD2STRUCT->fxp_commit_state.cvt_write = 1;
		THREAD2STRUCT->fxp_commit_state.acc_clear = 0;


	hmx_maptr->enable16x16 = (type == HMX_UH_UH);
	hmx_maptr->outputselect16x16 = start & 0x40; //using channel bits to select 16x16 upper or lower


	THREAD2STRUCT->cvt_type = direction;

	fVDOCHKPAGECROSS(start, start+hmx_maptr->dY);



	fMX_GETMASK(THREAD2STRUCT->tile_x_mask, ~range, format, 0);
	fMX_GETMASK(THREAD2STRUCT->tile_y_mask,  range, format, 0);



	fMX_MASKED_INC(THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_x_mask);
	fMX_MASKED_INC(THREAD2STRUCT->tile_y_inc, THREAD2STRUCT->tile_y_mask);


	hmx_maptr->x_offset = start & THREAD2STRUCT->tile_x_mask;
	hmx_maptr->y_offset = start & THREAD2STRUCT->tile_y_mask;


	THREAD2STRUCT->x_acc_offset = 0;
	if((THREAD2STRUCT->cvt_type == HMX_CVT_BEFORE) && (THREAD2STRUCT->tile_x_mask)){
		int x_count = hmx_maptr->x_offset;
		int x_acc_offset = 0;
		int mask = THREAD2STRUCT->tile_x_mask | (1 << 31);
		while (x_count>=0) {
			if (x_count>=0) {
				x_acc_offset = fMX_INC_MASKED(x_acc_offset, THREAD2STRUCT->tile_x_inc, mask);
			}
			x_count = fMX_INC_MASKED(x_count, THREAD2STRUCT->tile_x_inc, mask);
		}
		THREAD2STRUCT->x_acc_offset = x_acc_offset;
	}

	THREAD2STRUCT->usr_fp = get_fp_behavior(thread);

	fMX_DEBUG_LOG(0, "MXMEM Debug %s CVT: Start=%x (PA=%llx) Range: %x %s Major", "FXP", start, paddr, range, (format) ? "Spatial" : "Channel");
	fMX_DEBUG_LOG(0, "\tTile: x_mask: 0x%x x_inc: 0x%x y_mask: 0x%x y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc );
	fMX_DEBUG_LOG(0, "\tVA offset x: 0x%x y: 0x%x", hmx_maptr->x_offset, hmx_maptr->y_offset);
	fMX_DEBUG_LOG(0, "\tCurrent Accumulator: 0x%x offset: 0x%x IEEE RND mode=%x",hmx_maptr->acc_select, THREAD2STRUCT->x_acc_offset, THREAD2STRUCT->usr_fp);


#ifdef VERIFICATION
	warn("MXMEM Debug %s Cvt pktid=%x (%d): Start=%x (PA=%llx) Range: %x %s Major", "FXP", thread->pktid,thread->pktid, start, paddr, range, (format) ? "Spatial" : "Channel");
	warn("Tile: tile_x_mask: 0x%x tile_x_inc: 0x%x tile_y_mask: 0x%x tile_y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc);
	warn("va x_offset: %x y_offset: %x", hmx_maptr->x_offset, hmx_maptr->y_offset);
	warn("Accumulator a0: 0x%x offset: 0x%x", hmx_maptr->acc_select, THREAD2STRUCT->x_acc_offset);
	fMX_VERIF_DEBUG_LOG(1, "MXMEM Debug %s CVT pktid:%08x: Start=%x (PA=%llx) Range: %x %s Major", "FXP", thread->pktid, start, paddr, range, (format) ? "Spatial" : "Channel");
	fMX_VERIF_DEBUG_LOG(1, "\tTile: x_mask: 0x%x x_inc: 0x%x y_mask: 0x%x y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc );
	fMX_VERIF_DEBUG_LOG(1, "\tVA offset x: 0x%x y: 0x%x", hmx_maptr->x_offset, hmx_maptr->y_offset);
	fMX_VERIF_DEBUG_LOG(1, "\tCurrent Accumulator: 0x%x offset: 0x%x",hmx_maptr->acc_select, THREAD2STRUCT->x_acc_offset);

    int slot_tmp = thread->ver_cur_slot;
    thread->ver_cur_slot = slot;
#endif
	if(!thread->bq_on) { memwrap_memtrace_st(thread, thread->Regs[REG_PC], start, paddr, 1); }
#ifdef VERIFICATION
    thread->ver_cur_slot = slot_tmp;
#endif
}

#ifdef FIXME
void hmx_debug_file_log(thread_t* thread, int lvl, char * buf) {
	if (thread->processor_ptr->arch_proc_options->hmxdebugfile && (thread->processor_ptr->arch_proc_options->hmxdebuglvl >= lvl) && !thread->bq_on) {
		fprintf(thread->processor_ptr->arch_proc_options->hmxdebugfile, "%s\n", buf);
	}
}



void hmx_debug_print_acc(thread_t* thread, int hex, int type, int hmx2) {

}

void hmx_preload_file(thread_t* thread) {

	if (thread->processor_ptr->arch_proc_options->hmxaccpreloadfile && !thread->bq_on) {
		FILE *fp = thread->processor_ptr->arch_proc_options->hmxaccpreloadfile;
		while (!feof(fp)) {

			unsigned int row;
			unsigned int col;
			unsigned int sel;
			unsigned int val;
			fscanf(fp, "%d %d %d %x", &row, &col, &sel, &val);
			THREAD2STRUCT->accum_fxp[row][col].w[sel] = val;
			fMX_DEBUG_LOG(1, "TB HMX WRITE: FXP ACC[%02d][%02d][%02d]=%08x", row, col, sel, val);
		}
	}
}
#endif
