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
#include "hmx/mpy_fp16.h"
#include "arch_options_calc.h"
#include "hmx/macros_auto.h"

#define env thread
#define thread_t         CPUHexagonState
#define THREAD2STRUCT ((hmx_state_t*)thread->processor_ptr->shared_extptr)

// Get Arch option through thread
#define ARCH_OPT_TH(OPTION) (thread->processor_ptr->arch_proc_options->OPTION)
#define MEMTRACE_LD(...)
#define memwrap_memtrace_ld(...)
#define memwrap_memtrace_st(...)
#define warn(...)
#define register_coproc_ldst_exception(...) {}

static int check_mxmem_page_cross(thread_t* thread, vaddr_t base, int length, int page_size)
{
	vaddr_t page_mask = (1ULL<<page_size)-1;
	if (((base+length) & ~page_mask) != (base & ~page_mask)) {
		warn("HMX Op crossed page: start =%x end=%x Page Size=%x Page Mask=%x", base, base+length, page_size,page_mask);
		return 1;
	}
    return 0;
}

#define MX_EXCEPTION_CHECK(VA,LEN,LEN2,TYPE1, TYPE2)\
	mem_init_access_unaligned(thread, slot, VA, VA, LEN, TYPE1, TYPE2);\
    if (EXCEPTION_DETECTED) return;\
	paddr = maptr->paddr;\
	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;\
    mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);\
    CHECK_ACCESS_RANGE(mxmem_exception, paddr, LEN);\
    mxmem_exception |= check_mxmem_page_cross(thread,VA,LEN2, thread->mem_access[slot].xlate_info.size);\
    if (mxmem_exception) {\
	        register_coproc_ldst_exception(thread,slot,VA);\
	}\
	if (EXCEPTION_DETECTED) return;



void hmx_bias_init(thread_t* thread, int slot, vaddr_t vaddr, int access_type, int size)
{
	paddr_t paddr;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;

	int type = (access_type == access_type_hmx_load_bias) ? TYPE_LOAD : TYPE_STORE;

	if(size / thread->processor_ptr->arch_proc_options->hmx_output_depth == 8)
	{
		hmx_maptr->bias_32bit = 1;
		//temprarily use bias load as bias load2 for Netrani before the correct modeling
		if(thread->processor_ptr->arch_proc_options->QDSP6_MX_RATE == 1)
			hmx_maptr->bias_32bit = 0;
	}
	else if (size / thread->processor_ptr->arch_proc_options->hmx_output_depth == 4)
		hmx_maptr->bias_32bit = 0;

	mem_init_access_unaligned(thread, slot, vaddr, vaddr, size, access_type, type);\

    if (EXCEPTION_DETECTED) return;
	maptr->paddr = paddr = maptr->paddr & ~(size-1);

	maptr->width = size;

	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;
	mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);
	CHECK_ACCESS_RANGE(mxmem_exception, paddr, size-1);
    if (mxmem_exception) {
        register_coproc_ldst_exception(thread,slot,vaddr);
	}
	if (EXCEPTION_DETECTED) return;

	if(!thread->bq_on) {
		if (access_type == access_type_hmx_load_bias) {
			memwrap_memtrace_ld(thread, thread->Regs[REG_PC], vaddr, paddr, 1);
		} else {
			memwrap_memtrace_st(thread, thread->Regs[REG_PC], vaddr, paddr, 1);
		}
	}

}

const char * hmx_act_type_str[] = {"BLOCK", "DEEP", "ABOVE", "SINGLE", "DILATE", "BATCH"};
const char * hmx_wei_type_str[] = {"NORMAL", "DEEP", "AFTER", "SINGLE", "DILATE", "DROP"};

void hmx_activation_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int format, int block_type)
{
    paddr_t paddr;
    mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;

	vaddr_t addr_mask = fMX_GETADDRMASK(thread->processor_ptr);
	int dY = range & addr_mask;

	MX_EXCEPTION_CHECK(start, dY, dY, access_type_hmx_load_act, TYPE_LOAD);


	maptr->vaddr = start;

	memset(hmx_maptr, 0, sizeof(hmx_mem_access_info_t));

	hmx_maptr->flt = (type == HMX_FP16);


	maptr->range = range;
	hmx_maptr->format = format;
	hmx_maptr->block_type = block_type;

	maptr->width = 2048;

	size4u_t mask = ((1 << fMX_GETCHANNELSIZE(thread->processor_ptr))-1) << format;
	//hmx_maptr->ch_start = ((start & mask) >> format) & ~0x3;
	//hmx_maptr->ch_stop = ((range & mask) >> format) & ~0x3;

	hmx_maptr->ch_start = ((start & mask) >> format);
	hmx_maptr->ch_stop  = ((range & mask) >> format);


	hmx_maptr->dY = dY;

	fMX_GETMASK(THREAD2STRUCT->tile_x_mask, ~range, format, 0);
	fMX_GETMASK(THREAD2STRUCT->tile_y_mask,  range, format, 0);

	if(hmx_maptr->flt) {
		THREAD2STRUCT->tile_x_mask &= ~1;
		THREAD2STRUCT->tile_y_mask &= ~1;
	}


	fMX_MASKED_INC(THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_x_mask);
	fMX_MASKED_INC(THREAD2STRUCT->tile_y_inc, THREAD2STRUCT->tile_y_mask);

	hmx_maptr->blocks = 1;
	hmx_maptr->fx = start & THREAD2STRUCT->tile_x_mask;
	hmx_maptr->fy = start & THREAD2STRUCT->tile_y_mask;
	hmx_maptr->acc_select = (hmx_maptr->flt) ? THREAD2STRUCT->current_acc_flt : THREAD2STRUCT->current_acc_fxp;
	hmx_maptr->act_reuse = 0;

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
		if (THREAD2STRUCT->tile_y_mask != 0) {
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

	// if start > stop, we are in grouped convolution mode. we will process all 32 input channels so overwrite the start/stop values
	// leading 1s  | group size
	// 0 | 32
	// 1 | 16
	// 2 | 8
	// 3 | 4
	// no group conv when in deep mode

	size4u_t grouped_convolution = (hmx_maptr->ch_start > hmx_maptr->ch_stop) && (block_type != HMX_ACT_DEEP) && thread->processor_ptr->arch_proc_options->hmx_group_conv_mode;
	THREAD2STRUCT->group_convolution = grouped_convolution;

	size4u_t cl1 = fCL1_1(((hmx_maptr->ch_stop - hmx_maptr->ch_start) ^ hmx_maptr->ch_start ^ hmx_maptr->ch_stop) << 2); //count of leading ones

	hmx_maptr->group_size =  grouped_convolution ? (32 >> cl1) : 32; // maybe just 32>>clz
	hmx_maptr->group_count = 32 / hmx_maptr->group_size;

	fMX_DEBUG_LOG(5, "\nDEBUG!!!  start=0x%x stop=0x%x grouped_convolution=%d group size=%d count=%d %x %d\n", hmx_maptr->ch_start, hmx_maptr->ch_stop, grouped_convolution, hmx_maptr->group_size, hmx_maptr->group_count, ((hmx_maptr->ch_stop - hmx_maptr->ch_start) ^ hmx_maptr->ch_start ^ hmx_maptr->ch_stop) << 2, cl1);

	int group_ch_start = hmx_maptr->ch_start & (0x1F >> cl1);
	int group_ch_stop = hmx_maptr->ch_stop & (0x1F >> cl1);

	if(hmx_maptr->group_count == 16)
	{
		group_ch_start = (hmx_maptr->ch_start & ~0x1) & (0x1F >> cl1);
		group_ch_stop = (hmx_maptr->ch_stop | 0x1) & (0x1F >> cl1);
	}
	else if(hmx_maptr->group_count <= 8)
	{
		group_ch_start = (hmx_maptr->ch_start & ~0x3) & (0x1F >> cl1);
		group_ch_stop = (hmx_maptr->ch_stop | 0x3) & (0x1F >> cl1);
	}

	fMX_DEBUG_LOG(5, "\nDEBUG!!! group_ch_start = %d, group_ch_stop = %d, group_size = %d\n", group_ch_start, group_ch_stop, hmx_maptr->group_size);

	hmx_maptr->ch_start = grouped_convolution ? group_ch_start : (hmx_maptr->ch_start & ~0x3);
	hmx_maptr->ch_stop = grouped_convolution ? (group_ch_stop + 1) : ((hmx_maptr->ch_stop | 0x3)+1);
	fMX_DEBUG_LOG(5, "\nDEBUG!!!  actual start=%x stop=%x grouped_convolution=%d group size=%d count=%d count_ratio=%d\n", hmx_maptr->ch_start, hmx_maptr->ch_stop, grouped_convolution, hmx_maptr->group_size, hmx_maptr->group_count, hmx_maptr->group_count_ratio);

	if ((hmx_maptr->ch_start >= hmx_maptr->ch_stop) && (hmx_maptr->blocks==1))  {
		hmx_maptr->blocks = 0;
	}

	/*if(hmx_maptr->flt && (hmx_maptr->group_size <= 4)) {
		hmx_maptr->blocks = 0;
		hmx_maptr->ch_start = 8;
		hmx_maptr->ch_stop = 3;
		fMX_DEBUG_LOG(0, "\nDEBUG!!! This is not supported: flt = %d, ch_start = %d, ch_stop = %d, group_size = %d\n", hmx_maptr->flt, hmx_maptr->ch_start, hmx_maptr->ch_stop, hmx_maptr->group_size);
	}*/

	if (THREAD2STRUCT->tile_x_mask!=0) {
		fMX_COMPUTER_FILTER_SIZE(hmx_maptr->x_tap, hmx_maptr->fx, THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc);
	} else {
		hmx_maptr->x_tap = 1;
	}
	if (block_type == HMX_ACT_DEEP) {
		int limit = (hmx_maptr->flt) ? thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ROWS : thread->processor_ptr->arch_proc_options->QDSP6_MX_ROWS;
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
	THREAD2STRUCT->flt_commit_state.acc_update = 0;

	//int fxp_limit = thread->processor_ptr->arch_proc_options->QDSP6_MX_ROWS *  thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS / thread->processor_ptr->arch_proc_options->QDSP6_MX_RATE;
	//int flt_limit = thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ROWS *  thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_COLS / thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_RATE;
	THREAD2STRUCT->mac_cycle_limit = (hmx_maptr->flt) ? 256 : 512;
	THREAD2STRUCT->operand_ready |= (hmx_maptr->blocks>0)<<slot;

	if(!thread->bq_on) { MEMTRACE_LD(thread, thread->Regs[REG_PC], start, paddr, 0, DREAD, 0xfeedfacedeadbeefULL); }
	fMX_DEBUG_LOG(0, "MXMEM Debug %s Mult pktid:%08x: Start=%x (PA=%llx) Range: %x dY: %x block_Type=%s", (hmx_maptr->flt) ? "FLT" : "FXP", thread->pktid, start, maptr->paddr, range, hmx_maptr->dY, hmx_act_type_str[block_type]);
	fMX_DEBUG_LOG(0, "\tTile: x_mask: 0x%x x_inc: 0x%x y_mask: 0x%x y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc);
	fMX_DEBUG_LOG(0, "\tBlock count in loop %x Channel start=%d stop=%d group_size=%d grouped_convolution=%d y_start=%d y_stop = %x", hmx_maptr->blocks,  hmx_maptr->ch_start, hmx_maptr->ch_stop, hmx_maptr->group_size, grouped_convolution, hmx_maptr->y_start, hmx_maptr->y_stop);
	fMX_DEBUG_LOG(1, "\tmac_cycle_limit: %d RATE=%d FP_RATE=%d FP_ACC_INT=%d FP_ACC_FRAC=%d FP_ACC_EXP=%d", THREAD2STRUCT->mac_cycle_limit,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_RATE,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_RATE,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP);

	if (hmx_maptr->blocks == 0) {
		fMX_DEBUG_LOG(0, "\tNumber of blocks is zero so packet dropped");
	}


}

static uint32_t get_fp_behavior(thread_t* thread)  {
	return GET_FIELD(USR_FPCOPROC, thread->gpr[HEX_REG_USR]) ;
}

void hmx_weight_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int block_type, int weight_count, int output_ch_scale)
{
	paddr_t paddr;
    mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;


	// bfloat
	THREAD2STRUCT->is_bf16 = (start >> 6) & 0x1;
	THREAD2STRUCT->is_bf16 &= (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP >= 8);

	hmx_maptr->flt = (type == HMX_FP16);

	//processing fields for weight decompression
	int wgtc_start_offset_mask = 0xf;
	int wgtc_mode_mask = 0x10;
	int wgtc_total_bytes_mask = 0x7f;

	THREAD2STRUCT->wgtc_mode = (start & wgtc_mode_mask) >> 4;
	fMX_DEBUG_LOG(3, "HMX weight decompression mode: %d", THREAD2STRUCT->wgtc_mode);

	if(THREAD2STRUCT->wgtc_mode != 0)
	{
		THREAD2STRUCT->wgtc_start_offset = start & wgtc_start_offset_mask;
		THREAD2STRUCT->wgtc_total_bytes = range & wgtc_total_bytes_mask;
		//global density only falls on the compression lane boundary
		THREAD2STRUCT->wgtc_total_bytes |= 0xF;
		if(THREAD2STRUCT->wgtc_total_bytes >= 0x7F)
			THREAD2STRUCT->wgtc_total_bytes = 0x7F;
		fMX_DEBUG_LOG(3, "HMX is in weight decompression mode, start offset: %x, global density: %x ", THREAD2STRUCT->wgtc_start_offset, THREAD2STRUCT->wgtc_total_bytes);
	}

	//temporarily ignore weight decompression offset
	THREAD2STRUCT->wgtc_start_offset = 0;


	int length = (int)range;
	mem_init_access_unaligned(thread, slot, start, start, length, access_type_hmx_load_wei, TYPE_LOAD);

	if (EXCEPTION_DETECTED) return;
	paddr = maptr->paddr;
	int mxmem_exception = (maptr->xlate_info.memtype.device && !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING)) ? 1 : 0;
	mxmem_exception |= !in_vtcm_space(thread->processor_ptr, paddr, SHOW_WARNING);
	CHECK_ACCESS_RANGE(mxmem_exception, paddr, length);
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

	hmx_maptr->weight_bits = 8 / pow(2, weight_count);

	paddr_t last_wgt_mask = (thread->processor_ptr->arch_proc_options->hmx_output_depth * 4);
	last_wgt_mask -= 1;
	paddr_t wgt_align_mask = ~last_wgt_mask;


	//printf("last_wgt_mask=%llx\n", last_wgt_mask);


	paddr = paddr & wgt_align_mask;
	maptr->paddr = paddr;
	paddr_t wgt_pa_at_mac_limit = paddr;
	THREAD2STRUCT->max_weight_pa = (paddr + range) | last_wgt_mask;
	THREAD2STRUCT->limit_execeeded = 0;
	THREAD2STRUCT->force_zero = 0;
	THREAD2STRUCT->weight_bits = 8;

	THREAD2STRUCT->negate_flt_wgt = (hmx_maptr->flt) &&  ((start & (0x20)) > 0);

	weight_count = (output_ch_scale==2) ? (weight_count-1) :  weight_count;	// Move Elsewhere?


	paddr_t limit = thread->processor_ptr->arch_proc_options->QDSP6_MX_ROWS * thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS;
	limit = limit * (1 << fMX_GETCHANNELSIZE(thread->processor_ptr));

	//for density=127 weight decompression case, we need to increase the limit to 576 vectors
	limit = limit * 9 / 8;

	if(THREAD2STRUCT->wgtc_mode == 0)
	limit >>= weight_count;

	if ( (range | last_wgt_mask) >= limit) {
		fMX_DEBUG_LOG(0, "Weight size limit exceeded: %llx limit: %llx ", (range | last_wgt_mask), limit);
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


	int32_t fxp_max_wgt = 4 * thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS;
	int32_t flt_max_wgt = 8 * thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_COLS;
	wgt_pa_at_mac_limit += (THREAD2STRUCT->mac_cycle_limit * ((hmx_maptr->flt) ? flt_max_wgt  : fxp_max_wgt )) >>  weight_count;
	wgt_pa_at_mac_limit -= 1;
	if ((wgt_pa_at_mac_limit < THREAD2STRUCT->max_weight_pa) && (THREAD2STRUCT->wgtc_mode != 1)) {
		fMX_DEBUG_LOG(0, "Needed wgt less than wgt range pa so lowering limit. needed pa=%llx max=%llx weight_scaling=%d", wgt_pa_at_mac_limit,  THREAD2STRUCT->max_weight_pa, weight_count);
		THREAD2STRUCT->max_weight_pa = wgt_pa_at_mac_limit ;
	}


	THREAD2STRUCT->usr_fp.raw = get_fp_behavior(thread);

	THREAD2STRUCT->paddr[slot] = paddr;
	THREAD2STRUCT->type[slot] = type;

	THREAD2STRUCT->fxp_commit_state.acc_update = 0;
	THREAD2STRUCT->flt_commit_state.acc_update = 0;
	THREAD2STRUCT->operand_ready |= ((int)range>=0)<<slot;



	fMX_DEBUG_LOG(0, "MXMEM DEBUG WGT OPCODE pktid=%8x Rs32=%x Rt32=%x Type=%s max pa=%llx starting pa=%llx limit=%llx wgt needed=%llx, decompression=%d", thread->pktid, start, range, hmx_wei_type_str[block_type], THREAD2STRUCT->max_weight_pa, paddr, limit, wgt_pa_at_mac_limit, THREAD2STRUCT->wgtc_mode);
	fMX_DEBUG_LOG(0, "\tFilter: fx: %d fy: %d Actual Size: %d by %d", hmx_maptr->fx, hmx_maptr->fy, hmx_maptr->x_tap, hmx_maptr->y_tap);
	fMX_DEBUG_LOG(0, "\tAccumulator Start: %d  Range: %d", hmx_maptr->acc_select, hmx_maptr->acc_range);
	fMX_DEBUG_LOG(0, "\tFlags dilate: %d %d deep: %d drop: %d IEEE FP RND: %x negate_flt: %x bf16:%d ", hmx_maptr->x_dilate, hmx_maptr->y_dilate, hmx_maptr->deep, hmx_maptr->drop, THREAD2STRUCT->usr_fp.raw, THREAD2STRUCT->negate_flt_wgt, THREAD2STRUCT->is_bf16);

	if (((int)range<0)) {
		hmx_maptr->blocks = 0;
		fMX_DEBUG_LOG(0, "\tRange Negative so packet dropped");
	}


	if(!thread->bq_on) { MEMTRACE_LD(thread, thread->Regs[REG_PC], start, paddr, 0, DREAD, 0xfeedfacedeadbeefULL); }

}



void hmx_legacy_convert_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t start, vaddr_t range, int format, int direction, int type)
{
    paddr_t paddr;
    paddr_t tile_mask = (1 << (thread->processor_ptr->arch_proc_options->hmx_spatial_size + fMX_GETCHANNELSIZE(thread->processor_ptr)))-1;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;
	hmx_maptr->dY =  range & ~tile_mask;

	// Exception Checking
	MX_EXCEPTION_CHECK(start, hmx_maptr->dY, hmx_maptr->dY, access_type_hmx_store, TYPE_STORE);

	paddr = paddr & ~tile_mask;


	// MAPTR stuff needed by timing model
	maptr->paddr = paddr;
	maptr->width = 2048;
	hmx_maptr->format = format;

	hmx_maptr->flt = type == HMX_FP16;
	hmx_maptr->acc_select = (hmx_maptr->flt) ? THREAD2STRUCT->current_acc_flt : THREAD2STRUCT->current_acc_fxp;

	if (hmx_maptr->flt) {
		THREAD2STRUCT->flt_commit_state.cvt_update = 1;
		THREAD2STRUCT->flt_commit_state.cvt_advance = 0;
		THREAD2STRUCT->flt_commit_state.cvt_write = 1;
		THREAD2STRUCT->flt_commit_state.acc_clear = 0;
	} else {
		THREAD2STRUCT->fxp_commit_state.cvt_update = 1;
		THREAD2STRUCT->fxp_commit_state.cvt_advance = 0;
		THREAD2STRUCT->fxp_commit_state.cvt_write = 1;
		THREAD2STRUCT->fxp_commit_state.acc_clear = 0;
	}


	hmx_maptr->enable16x16 = (type == HMX_UH_UH);
	hmx_maptr->outputselect16x16 = start & 0x40; //using channel bits to select 16x16 upper or lower


	THREAD2STRUCT->cvt_type = direction;

	fVDOCHKPAGECROSS(start, start+hmx_maptr->dY);



	fMX_GETMASK(THREAD2STRUCT->tile_x_mask, ~range, format, 0);
	fMX_GETMASK(THREAD2STRUCT->tile_y_mask,  range, format, 0);

	if(hmx_maptr->flt) {
		THREAD2STRUCT->tile_x_mask &= ~1;
		THREAD2STRUCT->tile_y_mask &= ~1;
	}



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

	THREAD2STRUCT->usr_fp.raw = get_fp_behavior(thread);

	fMX_DEBUG_LOG(0, "MXMEM Debug %s CVT: Start=%x (PA=%llx) Range: %x %s Major", (hmx_maptr->flt) ? "FLT" : "FXP", start, paddr, range, (format) ? "Spatial" : "Channel");
	fMX_DEBUG_LOG(0, "\tTile: x_mask: 0x%x x_inc: 0x%x y_mask: 0x%x y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc );
	fMX_DEBUG_LOG(0, "\tVA offset x: 0x%x y: 0x%x", hmx_maptr->x_offset, hmx_maptr->y_offset);
	fMX_DEBUG_LOG(0, "\tCurrent Accumulator: 0x%x offset: 0x%x IEEE RND mode=%x",hmx_maptr->acc_select, THREAD2STRUCT->x_acc_offset, THREAD2STRUCT->usr_fp.raw);
	fMX_DEBUG_LOG(0, "RATE=%d FP_RATE=%d FP_ACC_INT=%d FP_ACC_FRAC=%d FP_ACC_EXP=%d",
	thread->processor_ptr->arch_proc_options->QDSP6_MX_RATE,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_RATE,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC,
	thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP);

	if(!thread->bq_on) { memwrap_memtrace_st(thread, thread->Regs[REG_PC], start, paddr, 1); }

}

void hmx_convert_init(thread_t* thread, int slot, int type, int feedback)
{
	mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;

	hmx_maptr->flt = (type == HMX_FP16);
	hmx_maptr->acc_select = (hmx_maptr->flt) ? THREAD2STRUCT->current_acc_flt : THREAD2STRUCT->current_acc_fxp;
	int rmw = 0;
	if(feedback && (type != HMX_UH)){
		rmw = 1;
	}
	if (hmx_maptr->flt) {
		THREAD2STRUCT->flt_commit_state.cvt_update = 1;
		THREAD2STRUCT->flt_commit_state.cvt_advance = rmw;
		THREAD2STRUCT->flt_commit_state.acc_clear = 0;
	} else {
		THREAD2STRUCT->fxp_commit_state.cvt_update = 1;
		THREAD2STRUCT->fxp_commit_state.cvt_advance = rmw;
		THREAD2STRUCT->fxp_commit_state.acc_clear = 0;
	}

	THREAD2STRUCT->usr_fp.raw = get_fp_behavior(thread);
}

void hmx_convert_store_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t start, vaddr_t range, int format, int type)
{
    paddr_t paddr;
    paddr_t tile_mask = (1 << (thread->processor_ptr->arch_proc_options->hmx_spatial_size + fMX_GETCHANNELSIZE(thread->processor_ptr)))-1;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;
	hmx_maptr->dY =  range & ~tile_mask;

	// Exception Checking
	MX_EXCEPTION_CHECK(start, hmx_maptr->dY, hmx_maptr->dY, access_type_hmx_store_cvt_state, TYPE_STORE);

	paddr = paddr & ~tile_mask;


	// MAPTR stuff needed by timing model
	maptr->paddr = paddr;
	maptr->width = 2048;
	hmx_maptr->format = format;

	hmx_maptr->flt = (type == HMX_FP16);
	hmx_maptr->acc_select = (hmx_maptr->flt) ? THREAD2STRUCT->current_acc_flt : THREAD2STRUCT->current_acc_fxp;

	if (hmx_maptr->flt) {
		THREAD2STRUCT->flt_commit_state.cvt_write = 1;
	} else {
		THREAD2STRUCT->fxp_commit_state.cvt_write = 1;
	}


	hmx_maptr->enable16x16 = (type == HMX_UH_UH);
	hmx_maptr->outputselect16x16 = start & 0x40; //using channel bits to select 16x16 upper or lower


	THREAD2STRUCT->cvt_type = HMX_CVT_BOTH;

	fVDOCHKPAGECROSS(start, start+hmx_maptr->dY);



	fMX_GETMASK(THREAD2STRUCT->tile_x_mask, ~range, format, 0);
	fMX_GETMASK(THREAD2STRUCT->tile_y_mask,  range, format, 0);

	if(hmx_maptr->flt) {
		THREAD2STRUCT->tile_x_mask &= ~1;
		THREAD2STRUCT->tile_y_mask &= ~1;
	}



	fMX_MASKED_INC(THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_x_mask);
	fMX_MASKED_INC(THREAD2STRUCT->tile_y_inc, THREAD2STRUCT->tile_y_mask);


	hmx_maptr->x_offset = start & THREAD2STRUCT->tile_x_mask;
	hmx_maptr->y_offset = start & THREAD2STRUCT->tile_y_mask;


	THREAD2STRUCT->x_acc_offset = 0;
	if(THREAD2STRUCT->tile_x_mask){
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

	THREAD2STRUCT->usr_fp.raw = get_fp_behavior(thread);

	fMX_DEBUG_LOG(0, "MXMEM Debug %s CVT: Start=%x (PA=%llx) Range: %x %s Major", (hmx_maptr->flt) ? "FLT" : "FXP", start, paddr, range, (format) ? "Spatial" : "Channel");
	fMX_DEBUG_LOG(0, "\tTile: x_mask: 0x%x x_inc: 0x%x y_mask: 0x%x y_inc: 0x%x", THREAD2STRUCT->tile_x_mask, THREAD2STRUCT->tile_x_inc, THREAD2STRUCT->tile_y_mask, THREAD2STRUCT->tile_y_inc );
	fMX_DEBUG_LOG(0, "\tVA offset x: 0x%x y: 0x%x", hmx_maptr->x_offset, hmx_maptr->y_offset);
	fMX_DEBUG_LOG(0, "\tCurrent Accumulator: 0x%x offset: 0x%x IEEE RND mode=%x",hmx_maptr->acc_select, THREAD2STRUCT->x_acc_offset, THREAD2STRUCT->usr_fp.raw);


	if(!thread->bq_on) { memwrap_memtrace_st(thread, thread->Regs[REG_PC], start, paddr, 1); }
}

void hmx_debug_file_log(thread_t* thread, int lvl, char * buf) {
	if (thread->processor_ptr->arch_proc_options->hmxdebugfile && (thread->processor_ptr->arch_proc_options->hmxdebuglvl >= lvl) && !thread->bq_on) {
		fprintf(thread->processor_ptr->arch_proc_options->hmxdebugfile, "%s\n", buf);
	}
}



void hmx_debug_print_acc(thread_t* thread, int hex, int type, int flt ) {
}

void hmx_preload_file(thread_t* thread) {

}
