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

#ifdef FIXME
void hmx_ext_alloc(processor_t *proc, int slots) { }
void *hmx_ext_talloc(processor_t *proc, int slots) { return NULL; }
void hmx_ext_init(processor_t *proc) { }
int hmx_ext_decode_checks(thread_t *thread, packet_t *pkt, exception_info *einfo) {	return 0; }
const char * hmx_ext_decode_find_iclass_slots(int opcode) { return ""; }

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


void *hmx_ext_palloc(processor_t *proc, int slots)
{
	hmx_state_t *hmx_state;
	if ((hmx_state = calloc(1,sizeof(hmx_state_t))) == NULL) {
		assert(-1);
	}

	return hmx_state;
}

#ifdef FIXME
void hmx_ext_pfree(processor_t *proc, int xa, int slots)
{
	free(proc->shared_extptr);
	proc->shared_extptr = NULL;
}
void hmx_ext_tfree(processor_t *proc, int xa, int slots)
{
}
void hmx_acc_ptr_reset(processor_t *proc) {
	((hmx_state_t*)proc->shared_extptr)->current_acc_fxp = 0;
}
#endif

/* Commit registers */
void hmx_ext_commit_regs(thread_t *thread)
{
	THREAD2STRUCT->operand_ready = 0;
	// Fixed Point Array
	if(THREAD2STRUCT->fxp_commit_state.acc_update) {
		memcpy(THREAD2STRUCT->accum_fxp, THREAD2STRUCT->future_accum_fxp, sizeof(hmx_acc_fxp_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		memset(THREAD2STRUCT->future_accum_fxp, 0, sizeof(hmx_acc_fxp_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}
	if(THREAD2STRUCT->fxp_commit_state.bias_update) {
		memcpy(THREAD2STRUCT->bias,  THREAD2STRUCT->future_bias,  sizeof(size4u_t)*MAX_ACCUMULATORS_DEPTH);
	}
	THREAD2STRUCT->fxp_commit_state.acc_update = 0;
	THREAD2STRUCT->fxp_commit_state.bias_update = 0;



	if(THREAD2STRUCT->fxp_commit_state.acc_clear_both) {
		THREAD2STRUCT->fxp_commit_state.acc_clear_both = 0;
		memset(THREAD2STRUCT->accum_fxp, 0, sizeof(hmx_acc_fxp_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
		memset(THREAD2STRUCT->future_accum_fxp, 0, sizeof(hmx_acc_fxp_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}


	if(THREAD2STRUCT->fxp_commit_state.swap_acc) {
        THREAD2STRUCT->current_acc_fxp ^= 1;
		THREAD2STRUCT->fxp_commit_state.swap_acc = 0;
	}
}


/* Cancel packet in progress */
void hmx_ext_rewind(thread_t *thread)
{
	THREAD2STRUCT->operand_ready = 0;
	THREAD2STRUCT->fxp_commit_state.val = 0;
};


void hmx_ext_commit_rewind(thread_t *thread)
{
	THREAD2STRUCT->operand_ready = 0;
	THREAD2STRUCT->fxp_commit_state.val = 0;
}

static void write_converted_peg(thread_t *thread, int x_idx, int y_idx, int x_acc_idx, int y_acc_idx) {
	hmx_state_t *hmx_state = THREAD2STRUCT;
	int acc_idx = (x_acc_idx|y_acc_idx);
	int current_acc __attribute__ ((unused)) = thread->mem_access[0].hmx_ma.acc_select;
	int format =  thread->mem_access[0].hmx_ma.format;
	int enable16x16 = thread->mem_access[0].hmx_ma.enable16x16;
	int outputselect16x16 = thread->mem_access[0].hmx_ma.outputselect16x16;
	paddr_t paddr_base = thread->mem_access[0].paddr;
	fMX_GET_ACC_INDEX(acc_idx, format);

	int beginning = 0;
	int end = thread->processor_ptr->arch_proc_options->hmx_output_depth;

	//modifying to only print 16 outputs
	if(enable16x16){
		if(outputselect16x16){
			beginning = thread->processor_ptr->arch_proc_options->hmx_output_depth / 2;
		}
		else{
			end = thread->processor_ptr->arch_proc_options->hmx_output_depth / 2;
		}
	}


	for(int z_idx = beginning; z_idx < end; z_idx++) {

		paddr_t pa = paddr_base + (x_idx|y_idx) + (z_idx << format);

		size1u_t byte = 0;
			byte = hmx_state->future_accum_fxp[acc_idx][z_idx].b[0];
			sim_mem_write1(thread->system_ptr,thread->threadId, pa, byte);
			fMX_DEBUG_LOG(3,         "HMX_CVT_FXP: write ACC[%02d][%02d][%02d] to PA=%08llx Byte: %02x pktid:%08x" , acc_idx, z_idx, current_acc, pa, byte, thread->pktid);
			fMX_VERIF_DEBUG_LOG(1,   "HMX_CVT_FXP: write ACC[%02d][%02d][%02d] to PA=%08llx Byte: %02x pktid:%08x" , acc_idx, z_idx, current_acc, pa, byte, thread->pktid);

#ifdef VERIFICATION
		int slot_tmp = thread->ver_cur_slot;
		thread->ver_cur_slot = 0;
		int width = 1;
		int val = byte;
		if (thread->processor_ptr->options->sim_vtcm_memory_callback) {
			thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, width, DWRITE, val);
		}
		thread->ver_cur_slot = 0;
		thread->ver_cur_slot = slot_tmp;
#endif
	}
}


static void write_x_row(thread_t *thread, int y_idx, int y_acc_idx) {
	hmx_state_t *hmx_state = THREAD2STRUCT;
	int x_idx  = 0;
	int x_acc_idx = hmx_state->x_acc_offset;
	int x_inc = hmx_state->tile_x_inc;
	int x_mask = hmx_state->tile_x_mask | (1<<31);
	int x_offset  = thread->mem_access[0].hmx_ma.x_offset;	// Start at offset

	fMX_VERIF_DEBUG_LOG(1,  "HMX_CVT: starting x=%x offset = %x pktid:%08x" , x_idx, x_offset, thread->pktid);
	for (; x_idx < x_offset; ) {
		if (hmx_state->cvt_type == HMX_CVT_BEFORE) {
			fMX_VERIF_DEBUG_LOG(1,  "HMX_CVT: before: writing peg x=%x pktid:%08x" , x_idx, thread->pktid);
			write_converted_peg(thread, x_idx, y_idx, x_acc_idx, y_acc_idx );
			x_acc_idx = fMX_INC_MASKED(x_acc_idx, x_inc, x_mask);
		}
		x_idx = fMX_INC_MASKED(x_idx, x_inc, x_mask);
		if (x_inc == 0) break;
	}
	//printf("x_idx=%x x_acc_idx=%x\n", x_idx, x_acc_idx);

	while(x_idx>=0) {
		if (hmx_state->cvt_type == HMX_CVT_AFTER) {
			fMX_VERIF_DEBUG_LOG(1,  "HMX_CVT: after: writing peg x=%x pktid:%08x" , x_idx, thread->pktid);
			write_converted_peg(thread, x_idx, y_idx, x_acc_idx, y_acc_idx );
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
			fMX_VERIF_DEBUG_LOG(1,  "HMX_CVT: writing row offset y=%x pktid:%08x" , y_idx, thread->pktid);
			write_x_row(thread, y_idx, y_acc_idx);
			y_idx = fMX_INC_MASKED(y_idx, y_inc, y_mask);
			y_acc_idx = fMX_INC_MASKED(y_acc_idx, y_inc, y_mask);
			if (y_inc == 0) break;
		}
		if (dY != 0) {
			// Address to next tile
			thread->mem_access[0].paddr += dY;
			fMX_VERIF_DEBUG_LOG(1,  "HMX_CVT: second block writing row y=%x pktid:%08x" , y_idx, thread->pktid);
			for (y_idx  = 0; y_idx < y_offset; )  {
				write_x_row(thread, y_idx, y_acc_idx);
				y_idx = fMX_INC_MASKED(y_idx, y_inc, y_mask);
				y_acc_idx = fMX_INC_MASKED(y_acc_idx, y_inc, y_mask);
				if (y_inc == 0) break;
			}
		} else {
			for (y_idx  = 0; y_idx < y_offset; )  {
				fMX_VERIF_DEBUG_LOG(1,  "HMX_CVT: writing row y=%x pktid:%08x" , y_idx, thread->pktid);
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

		if (hmx_state->fxp_commit_state.acc_clear) {
			int current_acc = thread->mem_access[0].hmx_ma.acc_select;
			for (int x = 0; x < MAX_ACCUMULATORS_DEPTH; x++) {
				for(int y = 0; y < MAX_ACCUMULATORS_SPATIAL; y++) {
					hmx_state->accum_fxp[y][x].w[current_acc] = 0;
				}
			}
			hmx_state->current_acc_fxp ^= 0x1 ; // Flip Accumulator
			fMX_DEBUG_LOG(1, "\t HMX CVT Accumulator swapped to %x", hmx_state->fxp_commit_state.acc_clear);
#ifdef VERIFICATION
			warn("MXMEM cvt clear swapped accumulator: %x", hmx_state->fxp_commit_state.acc_clear);
#endif
		}
		hmx_state->fxp_commit_state.cvt_write = 0;
		hmx_state->fxp_commit_state.acc_clear = 0;
		memset(THREAD2STRUCT->future_accum_fxp, 0, sizeof(hmx_acc_fxp_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL);
	}


	// This will get cleaned up when old bias instructions are replicated
	if(hmx_state->fxp_commit_state.bias_write) {
		for(int output_ch_idx = 0; output_ch_idx < thread->processor_ptr->arch_proc_options->hmx_output_depth; output_ch_idx++) {
			size4u_t temp = hmx_state->bias[output_ch_idx].val[0];
			paddr_t pa = thread->mem_access[0].paddr + 4*output_ch_idx;
			sim_mem_write4(thread->system_ptr,thread->threadId, pa , temp);
#ifdef VERIFICATION
			int slot_tmp = thread->ver_cur_slot;
			thread->ver_cur_slot = 0;
			if (thread->processor_ptr->options->sim_vtcm_memory_callback) {
				thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, 4, DWRITE, temp);
				warn("MXMEM Bias Write Callback to PA: %llx %8x (state: %04x)", pa, temp, hmx_state->bias[output_ch_idx].val[0]);
			}
			thread->ver_cur_slot = slot_tmp;
#endif
		}
		if(hmx_state->fxp_commit_state.bias_write == 2) {
			for(int output_ch_idx = 0; output_ch_idx < thread->processor_ptr->arch_proc_options->hmx_output_depth; output_ch_idx++) {
				size4u_t temp = hmx_state->bias[output_ch_idx].val[1];
				paddr_t pa = thread->mem_access[0].paddr + 4*output_ch_idx + 128;
				sim_mem_write4(thread->system_ptr,thread->threadId, pa , temp);
	#ifdef VERIFICATION
				int slot_tmp = thread->ver_cur_slot;
				thread->ver_cur_slot = 0;
				if (thread->processor_ptr->options->sim_vtcm_memory_callback) {
					thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, 4, DWRITE, temp);
					warn("MXMEM Bias Write Callback to PA: %llx %8x (state: %04x)", pa, temp, hmx_state->bias[output_ch_idx].val[0]);
				}
				thread->ver_cur_slot = slot_tmp;
	#endif
			}
		}
		hmx_state->fxp_commit_state.bias_write = 0;
	}



}

#ifdef FIXME
void hmx_ext_print_acc(thread_t *thread, FILE *fp) {
#if 0
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
#endif
}
#endif

int hmx_ext_get_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2STRUCT != NULL) {
		*result = THREAD2STRUCT->accum_fxp[spatial_index][channel_index].w[wordno];
		fMX_VERIF_DEBUG_LOG(2, "TB HMX READ: FXP ACC[%02d][%02d][%02d]=%08x pktid:%08x", spatial_index, channel_index, wordno, *result, thread->pktid);
		return 0;
	}
	return -1;
}
int hmx_ext_set_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t val) {
	if (THREAD2STRUCT != NULL) {
		THREAD2STRUCT->accum_fxp[spatial_index][channel_index].w[wordno] = val;
		fMX_DEBUG_LOG(1, "TB HMX WRITE: FXP ACC[%02d][%02d][%02d]=%08x", spatial_index, channel_index, wordno, val);
		fMX_VERIF_DEBUG_LOG(2, "TB HMX WRITE: FXP ACC[%02d][%02d][%02d] = %08x", spatial_index, channel_index, wordno, val);
		return 0;
	}
	return -1;
}

int hmx_ext_get_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t *result) {
	*result = 0xDEADBEEF;
	if (THREAD2STRUCT != NULL) {
		*result = THREAD2STRUCT->bias[channel_index].val[wordno];
		fMX_VERIF_DEBUG_LOG(2, "TB HMX READ: BIAS [%02d].w[%d] = %08x", channel_index, wordno, *result);
		return 0;
	}
	return -1;
}

int hmx_ext_set_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t val) {
	if (THREAD2STRUCT != NULL) {
		fMX_VERIF_DEBUG_LOG(2, "TB HMX WRITE: BIAS [%02d].w[%d] = %08x", channel_index, wordno, val);
		THREAD2STRUCT->bias[channel_index].val[wordno] = val;
		return 0;
	}
	return -1;
}

#ifdef FIXME
/* Checkpoint save/restore functions; ignore SYSCFG vector length here */
void hmx_ext_dump_acc(FILE *fp, processor_t *proc, int extno) {


}

void hmx_ext_analyze_packet(thread_t * thread, packet_t *pkt) {
	processor_t *proc = thread->processor_ptr;
	if (pkt->pkt_has_shared_extension && !thread->bq_on) {
		for (int i = 0; i < pkt->num_insns; i++) {
			size2u_t opcode = pkt->insn[i].opcode;
			int swap_inc = (opcode == M8_mxswap) || (opcode == M8_mxswap_hf) || (opcode == M8_mxaccshl);
			int clr_inc = (opcode == M8_mxclracc) || (opcode == M8_mxclracc_hf );
			INC_PSTATNPC(phmx_cvt, (swap_inc || clr_inc) , pkt->PC_VA);
			INC_PSTATNPC(phmxcvt_clr, clr_inc, pkt->PC_VA);

		}
	}
}
#endif
