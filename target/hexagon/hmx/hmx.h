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

#ifndef _HMX_ARCH_H
#define _HMX_ARCH_H

#include "arch_types.h"
#include <stdint.h>
#include "max.h"
#include "macros.h"

#define MAX_ACCUMULATORS_DEPTH 64
#define MAX_ACCUMULATORS_SPATIAL 64
#define MAX_INPUT_CHANNELS 64

#define MAX_HMX_ACC_BYTES	8
#define HMX_HEX 1

#define thread_t CPUHexagonState

typedef enum {
	HMX_UB=0,
	HMX_B,
	HMX_H,
	HMX_UH,
	HMX_W,
	HMX_FULL,
	HMX_UH_UH
} hmx_operand_type_t;



typedef enum {
	HMX_ROW=0,
	HMX_COLUMN=1,
	HMX_INPUTS=2
} hmx_index_t;

typedef enum {
	HMX_WEIGHTS=0,
	HMX_ACTIVATIONS=1,
	HMX_INPUTS_BY_TYPE=2
} hmx_index_type_t;

typedef enum {
	HMX_MATRIX=0,
	HMX_INDENTITY=1,
	HMX_SCALAR=2,
	HMX_TYPES=3
} hmx_matrix_type_t;

typedef enum {
	HMX_CVT_BEFORE=0,
	HMX_CVT_AFTER=1,
	HMX_CVT_TYPES=2
} hmx_cvt_type_t;

typedef enum {
	HMX_ACT_BLOCK=0,
	HMX_ACT_DEEP=1,
	HMX_ACT_ABOVE=2,
	HMX_ACT_SINGLE=3,
	HMX_ACT_DILATE=4,
	HMX_ACT_BATCH=5
} hmx_activation_type_t;

typedef enum {
	HMX_WEI_NORMAL=0,
	HMX_WEI_DEEP=1,
	HMX_WEI_AFTER=2,
	HMX_WEI_SINGLE=3,
	HMX_WEI_DILATE=4,
	HMX_WEI_DROP=5
} hmx_weight_type_t;



typedef union {
	union {
		size8u_t  ud[MAX_HMX_ACC_BYTES/8];
		size8s_t   d[MAX_HMX_ACC_BYTES/8];
		size4u_t  uw[MAX_HMX_ACC_BYTES/4];
		size4s_t val[MAX_HMX_ACC_BYTES/4];
		size4s_t   w[MAX_HMX_ACC_BYTES/4];
		size2u_t  uh[MAX_HMX_ACC_BYTES/2];
		size2s_t   h[MAX_HMX_ACC_BYTES/2];
		size1u_t  ub[MAX_HMX_ACC_BYTES/1];
		size1s_t   b[MAX_HMX_ACC_BYTES/1];
	};
} hmx_acc_fxp_t;


typedef struct {
	size4u_t sig:10;
	size4u_t exp:5;
	size4u_t sgn:1;
	size4u_t reserved0:6;
	size4u_t rnd_bit:1;
	size4u_t reserved1:9;
	size4u_t bias32;
} hmx_bias_fxp_t;


typedef union {
	hmx_bias_fxp_t fxp;
	size4u_t val[2];
} hmx_bias_t;




typedef union {
	struct {
		size2u_t cvt_write:1;
		size2u_t acc_clear_both:1;
		size2u_t acc_clear:1;
		size2u_t swap_acc:1;
		size2u_t acc_update:1;
		size2u_t bias_update:1;
		size2u_t bias_write:2;
	};
	size2u_t val;
} hmx_commit_state_t;


typedef struct {
	union {
		size2s_t val;
		size1s_t fxp;
	};
	size1s_t valid;
} hmx_wgt_cache_t;

typedef struct HMX_State {


	vaddr_t start[2];
	vaddr_t range[2];
	paddr_t paddr[2];
	size1u_t type[2];

	size4u_t mac_cycle_limit;
	size4u_t limit_execeeded;
	size4u_t force_zero;
	size4s_t dY;

	paddr_t max_weight_pa;
	size4u_t operand_ready:4;
	size4u_t cvt_type:4;
	size1s_t weight_count;
	size1s_t usr_fp;

	size4u_t tile_x_mask;
	size4u_t tile_y_mask;
	size4u_t tile_x_inc;
	size4u_t tile_y_inc;
	size4u_t x_acc_offset;

	hmx_commit_state_t fxp_commit_state;

	size4u_t current_acc_fxp:1;

	hmx_acc_fxp_t accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

	hmx_bias_t bias[MAX_ACCUMULATORS_DEPTH];
	hmx_bias_t future_bias[MAX_ACCUMULATORS_DEPTH];

	//union {
		hmx_acc_fxp_t future_accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
	//};

	hmx_wgt_cache_t wgt_cache[8*256];	// TODO: see if we can reduce the size

	size1s_t mpy_matrix[MAX_ACCUMULATORS_SPATIAL][MAX_INPUT_CHANNELS][MAX_ACCUMULATORS_DEPTH];

	size1s_t mpy_matrix_pre[MAX_ACCUMULATORS_SPATIAL][4][MAX_ACCUMULATORS_DEPTH];

	size4u_t array_mpy[4];
	size4u_t array_acc;
	size4u_t array_cvt;

	size1u_t weight_bits;

} mmvecx_t, hmx_state_t;

void hmx_convert_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t vaddr, vaddr_t range, int format, int direction, int type);

void hmx_activation_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int format, int block_type);
void hmx_weight_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int block_type, int weight_count);
int get_fp_behavior(thread_t* thread);


void hmx_bias_init(thread_t* thread, int slot, vaddr_t vaddr, int access_type, int size);

void hmx_debug_print_acc(thread_t* thread, int hex, int type, int f);
void matrix_mult(thread_t * thread, hmx_state_t * hmx_state);

void hmx_debug_file_log(thread_t* thread, int lvl, char * buf);

void hmx_preload_file(thread_t* thread);

#if 0
void hmx_acc_reset(processor_t * proc);

//#define THREAD2STRUCT ((hmx_state_t*)thread->processor_ptr->shared_extptr)

#define  fMX_DEBUG_LOG(LVL,...) \
	{\
		if (thread->processor_ptr->arch_proc_options->hmxdebugfile && (thread->processor_ptr->monotonic_pcycles > thread->processor_ptr->arch_proc_options->hmxdebugfile_start_pcycle) && (thread->processor_ptr->monotonic_pcycles < thread->processor_ptr->arch_proc_options->hmxdebugfile_end_pcycle)) {\
			char buf[1024];\
			sprintf(buf, __VA_ARGS__);\
			hmx_debug_file_log(thread, LVL, buf);\
		}\
	}
#else
#define fMX_DEBUG_LOG(...)
#endif
#ifdef VERIFICATION
#define  fMX_VERIF_DEBUG_LOG(LVL,...) \
	{\
		if (thread->processor_ptr->ver_hmx_debug_print_level >= LVL) {\
			fprintf(thread->processor_ptr->fp_hmx_debug, __VA_ARGS__);\
			fprintf(thread->processor_ptr->fp_hmx_debug, "\n");\
		}\
	}
#else
#define  fMX_VERIF_DEBUG_LOG(LVL,...)

#endif

#endif

