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

#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#endif
#include "utils.h"
#include "hmx/ext_hmx.h"
#include "hmx/hmx.h"
#include "arch_options_calc.h"
#include "hmx/macros_auto.h"
//#include "assert.h"
//#include "string.h"

#include <stdio.h>
#include <assert.h>
//#include "pmu.h"

#define THREAD2STRUCT ((hmx_state_t*)thread->processor_ptr->shared_extptr)

#ifdef CONFIG_USER_ONLY
#define sim_mem_write1(X, Y, addr, val)        put_user_u8(val, addr)
#define sim_mem_write2(X, Y, addr, val)        put_user_u16(val, addr)
#define sim_mem_write4(X, Y, addr, val)        put_user_u32(val, addr)
#else
#define sim_mem_write1(X, Y, addr, val)        hexagon_tools_memory_write(thread, addr, 1, val)
#define sim_mem_write2(X, Y, addr, val)        hexagon_tools_memory_write(thread, addr, 2, val)
#define sim_mem_write4(X, Y, addr, val)        hexagon_tools_memory_write(thread, addr, 4, val)
#endif

// Get Arch option through thread
#define ARCH_OPT_TH(OPTION) (thread->processor_ptr->arch_proc_options->OPTION)
#define MEMTRACE_LD(...)
#define memwrap_memtrace_ld(...)
#define memwrap_memtrace_st(...)
#define warn(...)
#define register_coproc_ldst_exception(...) {}
#define MEMTRACE_LD(...)


static void clear_xfp_accumulators(processor_t *proc, int idx) {
  if (proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP)
  {
    hmx_xfp_t xfp_zero = hmx_xfp_zero(proc);
    thread_t * thread = proc->thread[0];
    for (int x = 0; x < MAX_ACCUMULATORS_DEPTH; x++) {
      for(int y = 0; y < MAX_ACCUMULATORS_SPATIAL; y++) {
          THREAD2STRUCT->accum_flt[y][x].xfp[idx] = xfp_zero;
          THREAD2STRUCT->accum_flt[y][x].ovf[idx] = 0;
      }
    }
  }
}

void hmx_ext_init(processor_t *proc) {
  clear_xfp_accumulators(proc,0);
  clear_xfp_accumulators(proc,1);
}

#if 0
void hmx_ext_print_reg(thread_t *thread, FILE *fp, int rnum)
{
	int hmx_spatial_sz = 1 << thread->processor_ptr->arch_proc_options->hmx_spatial_size;
	int hmx_channel_depth = 1 << fMX_GETCHANNELSIZE(thread->processor_ptr);
	int print_col = 4;
	int j, k;
	size4s_t v;
	if(rnum >= hmx_spatial_sz) {
		fprintf(fp, "HMX row index is out of bounds, max is %d", hmx_spatial_sz-1);
		return;
	}
	fprintf(fp, "\tSet 0\t\t\tSet 1\n");
	for (j=0;j<hmx_channel_depth; j+=print_col) {
		fprintf(fp,"A%02d.%02d: ", rnum, j);
		for(k=0; k < print_col && (j + k) < hmx_channel_depth; k++) {
			v = THREAD2STRUCT->accum_fxp[rnum][j+k].w[0];
			fprintf(fp, "%04x ", v);
		}
		fprintf(fp, "\t");
		for(k=0; k < print_col && (j + k) < hmx_channel_depth; k++) {
			v = THREAD2STRUCT->accum_fxp[rnum][j+k].w[1];
			fprintf(fp, "%04x ", v);
		}
		fprintf(fp, "\n");
	}
}
void hmx_ext_print_regs(thread_t *thread, FILE *fp) {
	int hmx_spatial_sz = 1 << thread->processor_ptr->arch_proc_options->hmx_spatial_size;
	int hmx_channel_depth = 1 << fMX_GETCHANNELSIZE(thread->processor_ptr);
	int print_col = 4;
	int i, j, k;
	size4s_t v;
	fprintf(fp, "\nAccumulators size configured as %d <spatial size> by %d <channel depth>\n", hmx_spatial_sz, hmx_channel_depth);
	fprintf(fp, "\tSet 0\t\t\tSet 1\n");
	for (i=0;i<hmx_spatial_sz;i++) {
		for (j=0;j<hmx_channel_depth; j+=print_col) {
			fprintf(fp,"A%02d.%02d: ", i, j);
			for(k=0; k < print_col && (j + k) < hmx_channel_depth; k++) {
				v = THREAD2STRUCT->accum_fxp[i][j+k].w[0];
				fprintf(fp, "%04x ", v);
			}
			fprintf(fp, "\t");
			for(k=0; k < print_col && (j + k) < hmx_channel_depth; k++) {
				v = THREAD2STRUCT->accum_fxp[i][j+k].w[1];
				fprintf(fp, "%04x ", v);
			}
			fprintf(fp, "\n");
		}
	}
	fprintf(fp,"\n");
}
#endif

int hmx_ext_get_ovf(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t acc_select, size4u_t *result)
{
	*result = 0x0;
	if (THREAD2STRUCT != NULL) {
		spatial_index <<= 1;
		*result = THREAD2STRUCT->accum_flt[spatial_index][channel_index].ovf[acc_select];
		fMX_DEBUG_LOG(2, "TB HMX READ: FLT OVERFLOW BITS ACC[%02d][%02d][%02d]=%08x", spatial_index/2, channel_index, acc_select, *result);
		return 0;
	}

	return -1;
}

int hmx_ext_set_ovf(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t acc_select, size4u_t val)
{
	if (THREAD2STRUCT != NULL) {
		spatial_index <<= 1;
		THREAD2STRUCT->accum_flt[spatial_index][channel_index].ovf[acc_select] = val;
		fMX_DEBUG_LOG(2, "TB HMX WRITE: FLT OVERFLOW BITS ACC[%02d][%02d][%02d]=%08x", spatial_index/2, channel_index, acc_select, val);
		return 0;
	}
	return -1;
}

void *hmx_ext_palloc(processor_t *proc, int slots)
{
	hmx_state_t *hmx_state;
	if ((hmx_state = calloc(1,sizeof(hmx_state_t))) == NULL) {
		assert(-1);
	}

	return hmx_state;
}

void hmx_ext_pfree(processor_t *proc, int xa, int slots)
{
	free(proc->shared_extptr);
	proc->shared_extptr = NULL;
}
void hmx_ext_tfree(processor_t *proc, int xa, int slots)
{
}

#if 0
void hmx_acc_ptr_reset(processor_t *proc) {
	((hmx_state_t*)proc->shared_extptr)->current_acc_flt = 0;
	((hmx_state_t*)proc->shared_extptr)->current_acc_fxp = 0;
}

void hmx_reset(processor_t *proc, thread_t *thread){
	hmx_acc_t reset_acc;
	hmx_acc_t reset_acc_flt;
	hmx_bias_t reset_bias;
	reset_bias.val[0] = 0;
	reset_bias.val[1] = 0;
	reset_acc.ovf[0] = 0;
	reset_acc.ovf[1] = 0;
	reset_acc_flt.ovf[0] = 0;
	reset_acc_flt.ovf[1] = 0;
	for(int i = 0; i < MAX_HMX_ACC_BYTES/8; i++){
		reset_acc.ud[i] = 0;
		reset_acc_flt.ud[i] = 0;
	}
	if (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP != 0){
		for(int acc_idx = 0; acc_idx < 2; acc_idx++) {
			reset_acc_flt.xfp[acc_idx].exp = (-1<<(thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP-1));
			reset_acc_flt.xfp[acc_idx].sig = 0;
			reset_acc_flt.xfp[acc_idx].status.inf = 0;
			reset_acc_flt.xfp[acc_idx].status.negative = 0;
			reset_acc_flt.xfp[acc_idx].status.zero = 1;
			reset_acc_flt.xfp[acc_idx].EXP = thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP;
			reset_acc_flt.xfp[acc_idx].INT = thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT;
			reset_acc_flt.xfp[acc_idx].FRAC = thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC;

		}
	}
	for(int depth = 0; depth < MAX_ACCUMULATORS_DEPTH; depth++){
		((hmx_state_t*)proc->shared_extptr)->bias[depth] = reset_bias;
		((hmx_state_t*)proc->shared_extptr)->future_bias[depth] = reset_bias;
		for(int spatial = 0; spatial < MAX_ACCUMULATORS_SPATIAL; spatial++){
			((hmx_state_t*)proc->shared_extptr)->accum_fxp[spatial][depth] = reset_acc;
			((hmx_state_t*)proc->shared_extptr)->accum_flt[spatial][depth] = reset_acc_flt;
			((hmx_state_t*)proc->shared_extptr)->cvt_future_accum_fxp[spatial][depth] = reset_acc;
			((hmx_state_t*)proc->shared_extptr)->future_accum_fxp[spatial][depth] = reset_acc;
			((hmx_state_t*)proc->shared_extptr)->future_accum_flt[spatial][depth] = reset_acc_flt;
			for(int cvt_state = 0; cvt_state < MAX_CONVERT_STATES; cvt_state++){
				((hmx_state_t*)proc->shared_extptr)->cvt_accum[cvt_state][spatial][depth] = reset_acc;
			}
		}
	}
	((hmx_state_t*)proc->shared_extptr)->current_acc_flt = 0;
	((hmx_state_t*)proc->shared_extptr)->current_acc_fxp = 0;
}
#endif

/* Commit registers */
void hmx_ext_commit_regs(thread_t *thread)
{
	THREAD2STRUCT->operand_ready = 0;
	// Fixed Point Array
	// int clear_fxp_future = 0;
	// int clear_flt_future = 0;
	if(THREAD2STRUCT->fxp_commit_state.cvt_update) {
		if(!THREAD2STRUCT->fxp_commit_state.cvt_advance){
		hmx_age_cvt_state(thread);
		}
		memcpy(THREAD2STRUCT->cvt_accum[THREAD2STRUCT->cvt_accum_current_index], THREAD2STRUCT->cvt_future_accum_fxp, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		// clear_fxp_future = 1;
	}
	if(THREAD2STRUCT->fxp_commit_state.acc_update) {
		memcpy(THREAD2STRUCT->accum_fxp, THREAD2STRUCT->future_accum_fxp, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		// clear_fxp_future = 1;
	}
	if(THREAD2STRUCT->fxp_commit_state.bias_update) {
		memcpy(THREAD2STRUCT->bias,  THREAD2STRUCT->future_bias,  sizeof(size4u_t)*MAX_ACCUMULATORS_DEPTH);
	}
	// if(clear_fxp_future){
	// 	memset(THREAD2STRUCT->cvt_future_accum_fxp, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	// }
	THREAD2STRUCT->fxp_commit_state.cvt_update = 0;
	THREAD2STRUCT->fxp_commit_state.cvt_advance = 0;
	THREAD2STRUCT->fxp_commit_state.acc_update = 0;
	THREAD2STRUCT->fxp_commit_state.bias_update = 0;


	// Floating Point Array
	if(THREAD2STRUCT->flt_commit_state.cvt_update) {
		if(!THREAD2STRUCT->flt_commit_state.cvt_advance){
		hmx_age_cvt_state(thread);
		}
		memcpy(THREAD2STRUCT->cvt_accum[THREAD2STRUCT->cvt_accum_current_index], THREAD2STRUCT->cvt_future_accum_flt, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		// clear_flt_future = 1;
	}
	if(THREAD2STRUCT->flt_commit_state.acc_update) {
		if(thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_PRESENT == 0){
			if (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP) {
				clear_xfp_accumulators(thread->processor_ptr,0);
			} else {
			memset(THREAD2STRUCT->accum_flt, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
			}
		}
		else{
			memcpy(THREAD2STRUCT->accum_flt, THREAD2STRUCT->future_accum_flt, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		}
		// clear_flt_future = 1;
	}


	THREAD2STRUCT->flt_commit_state.cvt_update = 0;
	THREAD2STRUCT->flt_commit_state.cvt_advance = 0;
	THREAD2STRUCT->flt_commit_state.acc_update = 0;

	if(THREAD2STRUCT->fxp_commit_state.acc_clear_both) {
		THREAD2STRUCT->fxp_commit_state.acc_clear_both = 0;
		memset(THREAD2STRUCT->accum_fxp, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		memset(THREAD2STRUCT->future_accum_fxp, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}
	if(THREAD2STRUCT->flt_commit_state.acc_clear_both) {
    THREAD2STRUCT->flt_commit_state.acc_clear_both = 0;
    if (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP) {
      clear_xfp_accumulators(thread->processor_ptr,0);
      clear_xfp_accumulators(thread->processor_ptr,1);
    } else {
      memset(THREAD2STRUCT->accum_flt, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
    }

		memset(THREAD2STRUCT->future_accum_flt, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
  }
	// if(clear_flt_future){
	// 	memset(THREAD2STRUCT->cvt_future_accum_flt, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	// }


	if(THREAD2STRUCT->fxp_commit_state.swap_acc) {
        THREAD2STRUCT->current_acc_fxp ^= 1;
		THREAD2STRUCT->fxp_commit_state.swap_acc = 0;
	}
	if(THREAD2STRUCT->flt_commit_state.swap_acc) {
		THREAD2STRUCT->current_acc_flt ^= 1;
		THREAD2STRUCT->flt_commit_state.swap_acc = 0;
	}
}


/* Cancel packet in progress */
void hmx_ext_rewind(thread_t *thread)
{
	THREAD2STRUCT->operand_ready = 0;
	THREAD2STRUCT->fxp_commit_state.val = 0;
	THREAD2STRUCT->flt_commit_state.val = 0;
};


void hmx_ext_commit_rewind(thread_t *thread)
{
	THREAD2STRUCT->operand_ready = 0;
	THREAD2STRUCT->fxp_commit_state.val = 0;
	THREAD2STRUCT->flt_commit_state.val = 0;
}

static void write_converted_peg(thread_t *thread, int x_idx, int y_idx, int x_acc_idx, int y_acc_idx, int old_state) {
	hmx_state_t *hmx_state = THREAD2STRUCT;
	int acc_idx = (x_acc_idx|y_acc_idx);
	//int current_acc = thread->mem_access[0].hmx_ma.acc_select;
	int format =  thread->mem_access[0].hmx_ma.format;
	int flt = thread->mem_access[0].hmx_ma.flt;
	int enable16x16 = thread->mem_access[0].hmx_ma.enable16x16;
	int outputselect16x16 = thread->mem_access[0].hmx_ma.outputselect16x16;
	paddr_t paddr_base = thread->mem_access[0].paddr;
	fMX_GET_ACC_INDEX(acc_idx, format);

	int beginning = 0;
	int end = thread->processor_ptr->arch_proc_options->hmx_output_depth;
	int offset_16x16 = 0;
	int scale_16x16 = 1;
	int adjust_16x16 = 0;
	int z_idx_adjusted;
	//modifying to only print 16 outputs
	if(enable16x16){
		adjust_16x16 = 1;
		scale_16x16 = 2;
		if(outputselect16x16){
			beginning = thread->processor_ptr->arch_proc_options->hmx_output_depth / 2;
			offset_16x16 = 16;
		}
		else{
			end = thread->processor_ptr->arch_proc_options->hmx_output_depth / 2;
		}
	}

  int cvt_accum_index = hmx_state->cvt_accum_current_index;

  if (old_state)
  {
    cvt_accum_index += MAX_CONVERT_STATES; //Force backwards iteration to be positive
    cvt_accum_index = (cvt_accum_index - 1) % MAX_CONVERT_STATES;
  }

  int convert_width = thread->processor_ptr->arch_proc_options->QDSP6_MX_CVT_WIDTH;
  if((convert_width < 8) || (convert_width > 16)){
    CPUState *cs = env_cpu(thread);
    cpu_abort(cs, "convert state width does not fall in acceptable range");
  }

	for(int z_idx = beginning; z_idx < end; z_idx++) {

		paddr_t pa = paddr_base + (x_idx|y_idx) + (z_idx << format);

		size1u_t byte = 0;
		size1u_t byte_zero = 0;
		size1u_t byte_one = 0;
		size2u_t hf = 0;

		z_idx_adjusted = ((z_idx - offset_16x16) * scale_16x16) + adjust_16x16;
    if(flt) {
      byte_zero = (hmx_state->cvt_accum[cvt_accum_index][acc_idx][z_idx_adjusted].uh[0] >> (convert_width - 8)) & 0xFF;
      byte_one  = (hmx_state->cvt_accum[cvt_accum_index][acc_idx + 1][z_idx_adjusted].uh[0] >> (convert_width - 8)) & 0xFF;
      hf = byte_zero + (((size2u_t)byte_one) << 8);
      sim_mem_write2(thread->system_ptr,thread->threadId, pa, hf);
      fMX_DEBUG_LOG(3,        "HMX_CVT_FLT: write ACC[%02d][%02d][%02d] to PA=%08llx Half word: %04x pktid:%08x" , acc_idx, z_idx, current_acc, pa, byte_zero, thread->pktid);
      fMX_DEBUG_LOG(3,        "HMX_CVT_FLT: write ACC[%02d][%02d][%02d] to PA=%08llx Half word: %04x pktid:%08x" , acc_idx+1, z_idx, current_acc, pa, byte_one, thread->pktid);
    } else {
      byte = (hmx_state->cvt_accum[cvt_accum_index][acc_idx][z_idx_adjusted].uh[0] >> (convert_width - 8)) & 0xFF;
      sim_mem_write1(thread->system_ptr,thread->threadId, pa, byte);
      fMX_DEBUG_LOG(3,         "HMX_CVT_FXP: write ACC[%02d][%02d][%02d] to PA=%08llx Byte: %02x pktid:%08x" , acc_idx, z_idx, current_acc, pa, byte, thread->pktid);
    }

	}
}


static void write_x_row(thread_t *thread, int y_idx, int y_acc_idx) {
	hmx_state_t *hmx_state = THREAD2STRUCT;
	int x_idx  = 0;
	int x_acc_idx = hmx_state->x_acc_offset;
	int x_inc = hmx_state->tile_x_inc;
	int x_mask = hmx_state->tile_x_mask | (1<<31);
	int x_offset  = thread->mem_access[0].hmx_ma.x_offset;	// Start at offset

	fMX_DEBUG_LOG(5,  "HMX_CVT_FLT: starting x=%x offset = %x pktid:%08x" , x_idx, x_offset, thread->pktid);
	for (; x_idx < x_offset; ) {
		if ((hmx_state->cvt_type == HMX_CVT_BEFORE) || (hmx_state->cvt_type == HMX_CVT_BOTH)) {
			fMX_DEBUG_LOG(5,  "HMX_CVT_FLT: before: writing peg x=%x pktid:%08x" , x_idx, thread->pktid);
			write_converted_peg(thread, x_idx, y_idx, x_acc_idx, y_acc_idx, hmx_state->cvt_type == HMX_CVT_BOTH);
			x_acc_idx = fMX_INC_MASKED(x_acc_idx, x_inc, x_mask);
		}
		x_idx = fMX_INC_MASKED(x_idx, x_inc, x_mask);
		if (x_inc == 0) break;
	}
	//printf("x_idx=%x x_acc_idx=%x\n", x_idx, x_acc_idx);
	x_acc_idx = 0;

	while(x_idx>=0) {
		if ((hmx_state->cvt_type == HMX_CVT_AFTER) || (hmx_state->cvt_type == HMX_CVT_BOTH)) {
			fMX_DEBUG_LOG(5,  "HMX_CVT_FLT: after: writing peg x=%x pktid:%08x" , x_idx, thread->pktid);
			write_converted_peg(thread, x_idx, y_idx, x_acc_idx, y_acc_idx, 0);
			x_acc_idx = fMX_INC_MASKED(x_acc_idx, x_inc, x_mask);
		}
		x_idx = fMX_INC_MASKED(x_idx, x_inc, x_mask);
		if (x_inc == 0) break;
	}
}

static void write_out_convert(thread_t *thread) {
		hmx_state_t *hmx_state = THREAD2STRUCT;
		int y_mask = hmx_state->tile_y_mask | (1<<31);
		int y_inc  = hmx_state->tile_y_inc;
		int y_idx  = thread->mem_access[0].hmx_ma.y_offset;
		int dY = thread->mem_access[0].hmx_ma.dY;
		int y_offset = y_idx;
		int y_acc_idx  = 0;

		while(y_idx>=0) {
			// Write out Horizontal Strip
			fMX_DEBUG_LOG(5,  "HMX_CVT_FLT: writing row offset y=%x pktid:%08x" , y_idx, thread->pktid);
			write_x_row(thread, y_idx, y_acc_idx);
			y_idx = fMX_INC_MASKED(y_idx, y_inc, y_mask);
			y_acc_idx = fMX_INC_MASKED(y_acc_idx, y_inc, y_mask);
			if (y_inc == 0) break;
		}
		if (dY != 0) {
			// Address to next tile
			thread->mem_access[0].paddr += dY;
			fMX_DEBUG_LOG(5,  "HMX_CVT_FLT: second block writing row y=%x pktid:%08x" , y_idx, thread->pktid);
			for (y_idx  = 0; y_idx < y_offset; )  {
				write_x_row(thread, y_idx, y_acc_idx);
				y_idx = fMX_INC_MASKED(y_idx, y_inc, y_mask);
				y_acc_idx = fMX_INC_MASKED(y_acc_idx, y_inc, y_mask);
				if (y_inc == 0) break;
			}
		} else {
			for (y_idx  = 0; y_idx < y_offset; )  {
				fMX_DEBUG_LOG(5,  "HMX_CVT_FLT: writing row y=%x pktid:%08x" , y_idx, thread->pktid);
				write_x_row(thread, y_idx, y_acc_idx );
				y_idx = fMX_INC_MASKED(y_idx, y_inc, y_mask);
				y_acc_idx = fMX_INC_MASKED(y_acc_idx, y_inc, y_mask);
				if (y_inc == 0) break;
			}
		}
}

void hmx_ext_commit_mem(thread_t *thread, int slot)
{
	hmx_state_t *hmx_state = THREAD2STRUCT;
	// NPU: Matrix Multiply Accumulators

	if(hmx_state->fxp_commit_state.cvt_write) {
		write_out_convert(thread);
		hmx_state->fxp_commit_state.cvt_write = 0;
		memset(THREAD2STRUCT->future_accum_fxp, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}

	if (hmx_state->fxp_commit_state.acc_clear) {
		int current_acc = thread->mem_access[0].hmx_ma.acc_select;
		for (int x = 0; x < MAX_ACCUMULATORS_DEPTH; x++) {
			for(int y = 0; y < MAX_ACCUMULATORS_SPATIAL; y++) {
				hmx_state->accum_fxp[y][x].w[current_acc+0] = 0;	// Lo
				hmx_state->accum_fxp[y][x].w[current_acc+2] = 0;	// Hi
			}
		}
		hmx_state->current_acc_fxp ^= 0x1 ; // Flip Accumulator
		hmx_state->fxp_commit_state.acc_clear = 0;
		fMX_DEBUG_LOG(1, "\t HMX CVT Accumulator swapped to %x", hmx_state->fxp_commit_state.acc_clear);
	}

	// Float Array
	if(hmx_state->flt_commit_state.cvt_write) {
		write_out_convert(thread);
		hmx_state->flt_commit_state.cvt_write = 0;
		memset(THREAD2STRUCT->future_accum_flt, 0, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}

	if (hmx_state->flt_commit_state.acc_clear) {
		int current_acc = thread->mem_access[0].hmx_ma.acc_select;
		if (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_EXP) {
				clear_xfp_accumulators(thread->processor_ptr,current_acc);
		}
		else
		{
			for (int x = 0; x < MAX_ACCUMULATORS_DEPTH; x++) {
				for(int y = 0; y < MAX_ACCUMULATORS_SPATIAL; y++) {
					// Legacy accumulators
					hmx_state->accum_flt[y][x].val[current_acc].w[0] = 0;
					hmx_state->accum_flt[y][x].val[current_acc].w[1] = 0;
					hmx_state->accum_flt[y][x].val[current_acc].w[2] = 0;
					hmx_state->accum_flt[y][x].val[current_acc].w[3] = 0;
					hmx_state->accum_flt[y][x].ovf[current_acc] = 0;
				}
			}
		}
		hmx_state->flt_commit_state.acc_clear = 0;
		hmx_state->current_acc_flt ^= 0x1 ; // Flip Accumulator
	}

	// This will get cleaned up when old bias instructions are replicated
	if(hmx_state->fxp_commit_state.bias_write) {
		for(int output_ch_idx = 0; output_ch_idx < thread->processor_ptr->arch_proc_options->hmx_output_depth; output_ch_idx++) {
			size4u_t temp = hmx_state->bias[output_ch_idx].val[0];
			paddr_t pa = thread->mem_access[0].paddr + 4*output_ch_idx;
			sim_mem_write4(thread->system_ptr,thread->threadId, pa , temp);
		}
		if(hmx_state->fxp_commit_state.bias_write == 2) {
			for(int output_ch_idx = 0; output_ch_idx < thread->processor_ptr->arch_proc_options->hmx_output_depth; output_ch_idx++) {
				size4u_t temp = hmx_state->bias[output_ch_idx].val[1];
				paddr_t pa = thread->mem_access[0].paddr + 4*output_ch_idx + 128;
				sim_mem_write4(thread->system_ptr,thread->threadId, pa , temp);
			}
		}
		hmx_state->fxp_commit_state.bias_write = 0;
	}



}


#if 0
void hmx_ext_print_acc(thread_t *thread, FILE *fp) {
	hmx_state_t *mmvecx = THREAD2STRUCT;
	int r,i,k;
	int len_words = (fVECSIZE())/4;
	fprintf(fp,"Vector size configured as %d words\n",len_words);

	for (r=0; r<NUM_VREGS; r++) {
		for (k = 0; k < len_words/8; k++) {
			fprintf(fp,"V%02d.%02d: ", r, k*8);
			for (i = 0; i < 8; i++) {
				fprintf(fp,"%08x ", mmvecx->VRegs[r].uw[i+k*8]);
			}
			fprintf(fp,"\n");
		}
	}
	for (r=0; r<NUM_QREGS; r++) {
		fprintf(fp,"Q%02d: ", r);
		for (i = 0; i < len_words/2; i++) {
			fprintf(fp,"%02x ", mmvecx->QRegs[r].ub[i]);
		}
		fprintf(fp,"\n");
	}
}
#endif

int	hmx_ext_set_fp_acc(thread_t *thread,  int spatial_idx, int channel_idx, int acc_idx, size4s_t exponent, size8s_t significand_hi, size8u_t significand_lo, size4u_t ovf) {

	if (THREAD2STRUCT != NULL)
	{
		processor_t * proc = thread->processor_ptr;
		spatial_idx <<= 1;
		if (proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP==0) // ACC TYPE - Fractional format Q63.64 format
		{
			exponent = (proc->arch_proc_options->QDSP6_MX_FP_ACC_INT-2);
			size16s_t acc_128 = {.hi = significand_hi, .lo = significand_lo };
			acc_128 = shiftr128(acc_128, 62-exponent);	// Align to Q63.64 format
			THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_idx].hi = acc_128.hi;
			THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_idx].lo = acc_128.lo;
			THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].ovf[acc_idx] = ovf;
			fMX_DEBUG_LOG(2,  "TB HMX WRITE: Q63.64 FLT ACC[%02d][%02d][%02d] = %016llx.%016llx ovf=%x (input exponent: %4d significand=%016llx%016llx)",
			spatial_idx/2, channel_idx, acc_idx, acc_128.hi, acc_128.lo, ovf, exponent, significand_hi, significand_lo );
    } else {
      hmx_xfp_t acc;
      int32_t shift = proc->arch_proc_options->QDSP6_MX_FP_ACC_FRAC+proc->arch_proc_options->QDSP6_MX_FP_ACC_INT;
      acc.EXP = proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP;
			// mask
      acc.exp = exponent & ((1<<proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP)-1);
			// sign extend
			acc.exp <<= (32 - proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP);
			acc.exp >>= (32 - proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP);
      acc.FRAC = proc->arch_proc_options->QDSP6_MX_FP_ACC_FRAC;
			acc.INT = proc->arch_proc_options->QDSP6_MX_FP_ACC_INT;
      acc.sig = significand_hi >> (64 - shift );
      acc.status.val = 0;
      acc.status.inf = ovf ;
			acc.lza = 0;
      if (acc.status.inf==0) {
        acc.status.negative = (significand_hi < 0);
				int32_t min_exp = -(1 << (acc.EXP-1));
				acc.status.zero = (significand_hi==0) && (acc.exp == min_exp);
      } else {
        acc.status.negative = ((acc.status.inf & 0x2) == 2);
      }
      THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].ovf[acc_idx] = ovf;
       THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].xfp[acc_idx] = acc;

			fMX_DEBUG_LOG(2,  "TB HMX WRITE: XFP ACC[%02d][%02d][%02d] exp: %04x raw sig: %016llx q=%02x.%06x Q%d.%de%d status inf: %d neg: %d zero: %d -> legacy: in ovf: %d exp: 0x%04x sig=%016llx.%016llx",
			spatial_idx/2, channel_idx, acc_idx, (uint16_t) acc.exp, (long long int)acc.sig, (uint8_t)((acc.sig >> acc.FRAC) & 0xFF), (uint32_t)(((acc.sig) & ((1<<acc.FRAC)-1)) << (24 - acc.FRAC)), acc.INT, acc.FRAC, acc.EXP,  acc.status.inf, acc.status.negative, acc.status.zero, ovf,  exponent, significand_hi, significand_lo );
    }
    return 0;
  }
  return -1;

}

int hmx_ext_get_fp_acc(thread_t *thread,  int spatial_idx, int channel_idx, int acc_idx, size4s_t * exponent, size8s_t * significand_hi, size8u_t * significand_lo, size4u_t * ovf) {
	*significand_hi = 0xDEADBEEF;
	*significand_lo = 0xDEADBEEF;
	*ovf = 0xDEADBEEF;
	if (THREAD2STRUCT != NULL)
	{
		processor_t * proc = thread->processor_ptr;
		spatial_idx <<= 1;
		*exponent = 0;
		if (proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP==0) // ACC TYPE - Fractional format Q63.64 format
		{
			*exponent =  (proc->arch_proc_options->QDSP6_MX_FP_ACC_INT-2);
			size16s_t acc_128 = {.hi = THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_idx].hi, .lo = THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_idx].lo };
			size16s_t acc_normalized = shiftl128(acc_128, 62 - *exponent);	// Align to S1.126 format. Legacy format uses a provided exponent
			*significand_hi = acc_normalized.hi;
			*significand_lo = acc_normalized.lo;
			*ovf = THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].ovf[acc_idx];
			fMX_DEBUG_LOG(2,  "TB HMX READ: Q63.64 FLT ACC[%02d][%02d][%02d] = %016llx.%016llx converted to (exponent: %4d significand=%016llx%016llx ovf=%x)",
			spatial_idx, channel_idx, acc_idx, acc_normalized.hi, acc_normalized.lo, *exponent, *significand_hi, *significand_lo, *ovf);
		} else {
			hmx_xfp_t acc = THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].xfp[acc_idx];
			size16s_t acc_128 = hmx_xfp_to_tb_callback(thread->processor_ptr, exponent, ovf, acc);

			*significand_hi = acc_128.hi;
			*significand_lo = acc_128.lo;


			fMX_DEBUG_LOG(2,  "TB HMX READ: XFP ACC[%02d][%02d][%02d] exp: %08x sig: %016llx status inf: %d neg: %d true zero: %d ovf: %d exponent: %08x significand=%016llx%016llx ",
				spatial_idx/2, channel_idx, acc_idx, acc.exp,  (long long int)acc.sig, acc.status.inf, acc.status.negative,  acc.status.zero, *ovf, *exponent, *significand_hi, *significand_lo);

		}

		return 0;
	}
	return -1;

}

int hmx_ext_get_acc_flt_qformat(thread_t *thread, int spatial_idx, size4u_t channel_idx, size4u_t acc_index, size8s_t * integer, size8u_t * fractional, size4u_t * ovf) {
	*integer = 0xDEADBEEF;
	*fractional = 0xDEADBEEF;
	*ovf = 0xDEADBEEF;
	if (THREAD2STRUCT != NULL) {
		spatial_idx <<= 1;
		*integer = THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_index].hi;
		*fractional = THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_index].lo;
		*ovf = THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].ovf[acc_index];
		return 0;
	}
	return -1;
}
int hmx_ext_set_acc_flt_qformat(thread_t *thread, int spatial_idx, size4u_t channel_idx, size4u_t acc_index, size8s_t integer, size8u_t fractional, size4u_t ovf)
{

	if (THREAD2STRUCT != NULL) {
		spatial_idx <<= 1;
		THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_index].hi = integer;
		THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_index].lo = fractional;
		THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].ovf[acc_index] = ovf;
		fMX_DEBUG_LOG(2,  "TB HMX WRITE: FLT ACC[%02d][%02d][%02d] = %016llx.%016llx ovf=%x",
			spatial_idx/2, channel_idx, acc_index,
			THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_index].hi,
			THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].val[acc_index].lo,
			THREAD2STRUCT->accum_flt[spatial_idx][channel_idx].ovf[acc_index] );
		return 0;
	}
	return -1;
}

int hmx_ext_get_acc_flt(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2STRUCT != NULL) {
		int acc_select = (wordno > 3);
		int word_select = wordno & 0x3;
		int acc_shift = 16+66 - (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC+thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT);
		spatial_index <<= 1;
		size16s_t acc =  THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select];

		acc = shiftr128(acc, acc_shift);
		*result = acc.w[word_select];

		fMX_DEBUG_LOG(2, "TB HMX READ: FLT ACC[%02d][%02d][%02d].w[%d]=%08x pktid:%08x", spatial_index/2, channel_index, acc_select, wordno, *result, thread->pktid);


		return 0;
	}
	return -1;
}
int hmx_ext_set_acc_flt(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t val) {

	if (THREAD2STRUCT != NULL) {
		int acc_select = (wordno > 3);
		int word_select = wordno & 0x3;
		int acc_shift = 16+(66 - (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC+thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT)) + (word_select*32);
		size16s_t acc =  {0};
		spatial_index <<= 1;
		acc.w[0] = val;
		acc = shiftl128(acc, acc_shift);
    if (word_select == 0) {
      THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[0] = 0;
      THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[1] = 0;
      THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[2] = 0;
      THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[3] = 0;
    }
		THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[0] |= acc.w[0];
		THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[1] |= acc.w[1];
		THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[2] |= acc.w[2];
		THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].w[3] |= acc.w[3];

		// Sign extend
		THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select] = shiftr128(shiftl128(THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select], 46), 46);
		fMX_DEBUG_LOG(2,  "TB HMX WRITE: FLT ACC[%02d][%02d][%02d] = %016llx.%016llx w[%d]=%08x",
			spatial_index/2, channel_index, acc_select,
			THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].hi,
			THREAD2STRUCT->accum_flt[spatial_index][channel_index].val[acc_select].lo, wordno, val);
		return 0;
	}
	return -1;
}

int hmx_ext_get_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2STRUCT != NULL) {
		*result = THREAD2STRUCT->accum_fxp[spatial_index][channel_index].w[wordno];
		fMX_DEBUG_LOG(2, "TB HMX READ: FXP ACC[%02d][%02d][%02d]=%08x pktid:%08x", spatial_index, channel_index, wordno, *result, thread->pktid);
		return 0;
	}
	return -1;
}
int hmx_ext_set_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t val) {
	if (THREAD2STRUCT != NULL) {
		THREAD2STRUCT->accum_fxp[spatial_index][channel_index].w[wordno] = val;
		fMX_DEBUG_LOG(1, "TB HMX WRITE: FXP ACC[%02d][%02d][%02d]=%08x", spatial_index, channel_index, wordno, val);
		return 0;
	}
	return -1;
}

int hmx_ext_get_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2STRUCT != NULL) {
		*result = THREAD2STRUCT->bias[channel_index].val[wordno];
		fMX_DEBUG_LOG(2, "TB HMX READ: BIAS [%02d].w[%d] = %08x", channel_index, wordno, *result);
		return 0;
	}
	return -1;
}

int hmx_ext_set_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t val) {
	if (THREAD2STRUCT != NULL) {
		fMX_DEBUG_LOG(2, "TB HMX WRITE: BIAS [%02d].w[%d] = %08x", channel_index, wordno, val);
		THREAD2STRUCT->bias[channel_index].val[wordno] = val;
		return 0;
	}
	return -1;
}

size4u_t hmx_ext_set_cvt_state(thread_t *thread, size4u_t age, size4u_t spatial_idx, size4u_t channel_idx, size4u_t state_index, size4u_t val) {
	if (THREAD2STRUCT != NULL){
		THREAD2STRUCT->cvt_accum[age ^ THREAD2STRUCT->cvt_accum_current_index][spatial_idx][channel_idx].w[state_index] = val;
		if(age == 0){
			THREAD2STRUCT->cvt_future_accum_fxp[spatial_idx][channel_idx].uh[0] = val;
		}
		fMX_DEBUG_LOG(2, "TB SET HMX CVT STATE[%02d][%02d] [%02d][%02d] = 0x%02x", age, state_index, spatial_idx, channel_idx, val);
		return 0;
	}
	return -1;
}

size4u_t hmx_ext_get_cvt_state(thread_t *thread, size4u_t age, size4u_t spatial_idx, size4u_t channel_idx, size4u_t state_index){
	if (THREAD2STRUCT != NULL){
		fMX_DEBUG_LOG(2, "TB GET HMX CVT STATE[%02d][%02d] [%02d][%02d]", age, state_index, spatial_idx, channel_idx);
		return THREAD2STRUCT->cvt_accum[age ^ THREAD2STRUCT->cvt_accum_current_index][spatial_idx][channel_idx].w[state_index];
	}
	return -1;
}

void hmx_age_cvt_state(thread_t *thread){
	THREAD2STRUCT->cvt_accum_current_index = (THREAD2STRUCT->cvt_accum_current_index + 1) % MAX_CONVERT_STATES;
}

int hmx_read_flt_acc_idx(thread_t *thread){
	return THREAD2STRUCT->current_acc_flt;
}

int hmx_read_fxp_acc_idx(thread_t *thread){
	return THREAD2STRUCT->current_acc_fxp;
}

#if 0
/* Checkpoint save/restore functions; ignore SYSCFG vector length here */
void hmx_ext_dump_acc(FILE *fp, processor_t *proc, int extno) {


}
#endif

void hmx_ext_analyze_packet(thread_t * thread, Packet *pkt) {
#if 0
	processor_t *proc = thread->processor_ptr;
	if (!thread->bq_on) {
		INC_TSTATNPC(thmx_pkt_thread, (GET_SSR_FIELD(SSR_XE2) != 0), pkt->PC_VA);
	}
	if (pkt->pkt_has_shared_extension && !thread->bq_on) {
		for (int i = 0; i < pkt->num_insns; i++) {
			size2u_t opcode = pkt->insn[i].opcode;
			//int swap_inc = (opcode == M8_mxswap) || (opcode == M8_mxswap_hf) || (opcode == M8_mxaccshl);
			int clr_inc = (opcode == M8_mxclracc) || (opcode == M8_mxclracc_hf );
			INC_PSTATNPC(phmx_cvt, clr_inc , pkt->PC_VA);
			INC_PSTATNPC(phmxcvt_clr, clr_inc, pkt->PC_VA);

		}
		INC_TSTAT(thmx_pkt);

		INC_TSTATN(tpktstat_ext_insn1, ((pkt->num_insns-pkt->pkt_has_endloop)==1));
		INC_TSTATN(tpktstat_ext_insn2, ((pkt->num_insns-pkt->pkt_has_endloop)==2));
		INC_TSTATN(tpktstat_ext_insn3, ((pkt->num_insns-pkt->pkt_has_endloop)==3));
		INC_TSTATN(tpktstat_ext_insn4, ((pkt->num_insns-pkt->pkt_has_endloop)==4));

		INC_TSTATN(thmx_ld_cnt,  (pkt->pkt_hmx_ld_ct==1));
		INC_TSTATN(thmx_ld2_cnt, (pkt->pkt_hmx_ld_ct==2));
		INC_TSTATN(thmx_st_cnt,  (pkt->pkt_hmx_st_ct==1));

	} else if (!pkt->pkt_has_shared_extension && !thread->bq_on && GET_SSR_FIELD(SSR_XE2)) {

		INC_TSTATN(tpktstat_insn1, ((pkt->num_insns-pkt->pkt_has_endloop)==1));
		INC_TSTATN(tpktstat_insn2, ((pkt->num_insns-pkt->pkt_has_endloop)==2));
		INC_TSTATN(tpktstat_insn3, ((pkt->num_insns-pkt->pkt_has_endloop)==3));
		INC_TSTATN(tpktstat_insn4, ((pkt->num_insns-pkt->pkt_has_endloop)==4));

	}
#endif
}

