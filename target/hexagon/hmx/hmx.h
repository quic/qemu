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
#include "../max.h"
//#include "hmx/macros_auto.h"
#include "mpy_fp16.h"
#include "mpy_hmx_fp_custom.h"

#define MAX_ACCUMULATORS_DEPTH 64
#define MAX_ACCUMULATORS_SPATIAL 64
#define MAX_CONVERT_STATES 2
#define MAX_INPUT_CHANNELS 64

#define MAX_HMX_ACC_BYTES	32
#define HMX_HEX 1

#define MAX_MX_RATE 8

#define thread_t CPUHexagonState

typedef enum {
	HMX_UB=0,
	HMX_B,
	HMX_H,
	HMX_UH,
	HMX_W,
	HMX_FULL,
	HMX_FP16,
	HMX_UH_UH,
	HMX_BF16
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
	HMX_CVT_BOTH=2,
	HMX_CVT_TYPES=3
} hmx_cvt_type_t;

typedef enum {
	HMX_FP_CVT_NORMAL=0,
	HMX_FP_CVT_HI=1,
	HMX_FP_CVT_LO=2
} hmx_fp_cvt_type_t;

typedef enum {
	HMX_POLY_NONE=0,
	HMX_POLY_ADD_MIN=1,
	HMX_POLY_MPY_MIN=2,
	HMX_POLY_ADD_MAX=5,
	HMX_POLY_MPY_MAX=6
} hmx_poly_states;

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


typedef struct {
	union {
		hmx_xfp_t xfp[2];			// Custom Floating Point format Accumulators s
		size16s_t val[MAX_HMX_ACC_BYTES/16];
		size2s_t  hf[MAX_HMX_ACC_BYTES/2];
		size8u_t  ud[MAX_HMX_ACC_BYTES/8];
		size8s_t   d[MAX_HMX_ACC_BYTES/8];
		size4u_t  uw[MAX_HMX_ACC_BYTES/4];
		size4s_t   w[MAX_HMX_ACC_BYTES/4];
		size2u_t  uh[MAX_HMX_ACC_BYTES/2];
		size2s_t   h[MAX_HMX_ACC_BYTES/2];
		size1u_t  ub[MAX_HMX_ACC_BYTES/1];
		size1s_t   b[MAX_HMX_ACC_BYTES/1];
	};
	size1s_t ovf[2];
} hmx_acc_t;

typedef struct {
	uint32_t sig:10;
	uint32_t exp:5;
	uint32_t negate:1;
	uint32_t sigmsb:1;
	uint32_t zeroneg:1;
	uint32_t zeropos:1;
	uint32_t bias0:3;
	uint32_t rnd_bit:1;
	uint32_t bias1:8;
	uint32_t siglsb:1;
	uint32_t bias32;
} hmx_bias_fxp_t;

typedef struct {
	uint32_t exp_delta:6;
	uint32_t sat_exp:5;
	uint32_t bias:21;
	uint32_t bias32;
} hmx_bias_flt_t;

// This will deprecate and become hmx_bias_flt_t
typedef struct {
	uint32_t scale:16;
	uint32_t output_bias:16;
	uint32_t output_bias_extra:4;
	uint32_t scale_extra:4;
	uint32_t negate:1;
	uint32_t zeroneg:1;
	uint32_t zeropos:1;
	uint32_t bias:21;
} hmx_bias_flt_new_t;

typedef union {
	hmx_bias_fxp_t fxp;
	hmx_bias_flt_t flt;
	hmx_bias_flt_new_t flt_new;
	hmx_bias_flt_poly_t flt_poly;
	uint32_t val[2];
} hmx_bias_t;




typedef union {
	struct {
		size2u_t cvt_write:1;
		size2u_t cvt_update:1;
		size2u_t cvt_advance:1;
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
	uint16_t wgt;
	uint8_t valid;
} hmx_wgt_cache_t;


typedef struct HMX_State {


	vaddr_t start[2];
	vaddr_t range[2];
	paddr_t paddr[2];
	size1u_t type[2];

  size2u_t pkt_total_mx;
	size4u_t mac_cycle_limit;
	size4u_t limit_execeeded;
	size4u_t force_zero;
	size4s_t dY;

	paddr_t max_weight_pa;
	size4u_t operand_ready:4;
	size4u_t cvt_type:4;
	size1s_t weight_count;
	usr_fp_reg_t usr_fp;

	size4u_t tile_x_mask;
	size4u_t tile_y_mask;
	size4u_t tile_x_inc;
	size4u_t tile_y_inc;
	size4u_t x_acc_offset;

	hmx_commit_state_t fxp_commit_state;
	hmx_commit_state_t flt_commit_state;

	size4u_t current_acc_fxp:1;
	size4u_t current_acc_flt:1;
	size4u_t negate_flt_wgt:1;
	size4u_t is_bf16:1;

	hmx_acc_t accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
	hmx_acc_t accum_flt[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

	size1u_t cvt_accum_current_index;
	hmx_acc_t cvt_accum[MAX_CONVERT_STATES][MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

	union {
		hmx_acc_t cvt_future_accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
		hmx_acc_t cvt_future_accum_flt[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
	};

	hmx_bias_t bias[MAX_ACCUMULATORS_DEPTH];
	hmx_bias_t future_bias[MAX_ACCUMULATORS_DEPTH];

	//union {
		hmx_acc_t future_accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
		hmx_acc_t future_accum_flt[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
	//};

	hmx_wgt_cache_t wgt_cache[2148][64];	// WGT Stream per output channel

	union {
		uint8_t act_cache_ub[2048*2];
		uint16_t act_cache_uh[2048*2/2];
		uint64_t act_cache_dw[2048*2/8];
	};
	hmx_xfp_t tmp_flt_acc_cache[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH][8];	// Max Input Channel Rate by Max output channels for FP16/BF16

	size1s_t mpy_matrix[MAX_ACCUMULATORS_SPATIAL][MAX_INPUT_CHANNELS][MAX_ACCUMULATORS_DEPTH];

	size1s_t mpy_matrix_pre[MAX_ACCUMULATORS_SPATIAL][4][MAX_ACCUMULATORS_DEPTH];

	size4u_t array_mpy[MAX_MX_RATE];
	size4u_t array_acc;
	size4u_t array_cvt;

	size1u_t weight_bits;
	size8u_t hmxmpytrace_cycle;


	size1u_t group_convolution;

	size1u_t wgtc_mode;
	size1u_t wgtc_start_offset;
	size2u_t wgtc_total_bytes;

	size1u_t weight_sm_mode;
} hmx_state_t;

void hmx_legacy_convert_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t vaddr, vaddr_t range, int format, int direction, int type);
void hmx_convert_init(thread_t* thread, int slot, int type, int feedback);
void hmx_convert_store_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t start, vaddr_t range, int format, int type);

void hmx_activation_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int format, int block_type);
void hmx_weight_init(thread_t* thread, vaddr_t start, vaddr_t range, int slot, int type, int block_type, int weight_count, int output_ch_scale);


void hmx_bias_init(thread_t* thread, int slot, vaddr_t vaddr, int access_type, int size);

void hmx_debug_print_acc(thread_t* thread, int hex, int type, int f);
void matrix_mult(thread_t * thread, hmx_state_t * hmx_state);

void hmx_debug_file_log(thread_t* thread, int lvl, char * buf);

void hmx_preload_file(thread_t* thread);

void hmx_acc_reset(processor_t * proc);


#define fMX_DEBUG_LOG(...)

#endif

