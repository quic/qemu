/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include "../max.h"
#include "hmx/hmx_int_ops.h"
#include "hmx/mpy_hmx_fp_custom.h"

#define MAX_ACCUMULATORS_DEPTH 64
#define MAX_ACCUMULATORS_SPATIAL 64
#define MAX_CONVERT_STATES 2
#define MAX_INPUT_CHANNELS 64
#define MAX_RATE_INPUT_CHANNELS 32
#define MAX_BIAS_STATES 4

#define MAX_HMX_ACC_BYTES	32
#define HMX_HEX 1

#define HMX_SPATIAL_MASK_BITS_CM 0x7E0
#define HMX_SPATIAL_MASK_BITS_SM 0x783

typedef enum {
	HMX_UB=0,
	HMX_B,
	HMX_UB4,
	HMX_UH,
	HMX_FP16,
	HMX_UH_UH,
	HMX_OP_TYPES
} hmx_operand_type_t;

typedef enum {
	HMX_MULT_FXP=0,
    HMX_MULT_FXP_SUBBYTE,
	HMX_MULT_XFP,
    HMX_MULT_TYPES
} hmx_mult_type_t;

typedef enum {
	HMX_UNPACK_BYTE_FROM_BYTE=0,
    HMX_UNPACK_SM_FROM_BYTE,
	HMX_UNPACK_NIBBLE_FROM_BYTE,
    HMX_UNPACK_CRUMB_FROM_BYTE,
	HMX_UNPACK_SCRUMB_FROM_BYTE,
	HMX_UNPACK_1BIT_FROM_BYTE,
	HMX_UNPACK_1SBIT_FROM_BYTE,
	HMX_UNPACK_NONE,
	HMX_UNPACK_TYPES
} hmx_unpack_type_t;

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

typedef struct hmx_bias_flt_poly {
	uint32_t scale:16;
	uint32_t out_bias:16;
	uint32_t scale_extra:4;
	uint32_t out_bias_extra:4;
	uint32_t shape:2;
    uint32_t negate:1;
	uint32_t acc_bias_extra:5;
	uint32_t acc_bias:16;
} hmx_bias_flt_poly_t;

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

typedef union hmx_bias {
	hmx_bias_fxp_t fxp;
	hmx_bias_flt_t flt;
	hmx_bias_flt_new_t flt_new;
	hmx_bias_flt_poly_t flt_poly;
	uint32_t val[2];
} hmx_bias_t;

typedef struct xfp_status {
    union {
        struct {
            uint32_t zero:1;
            uint32_t inf:2;
            uint32_t negative:1;
			uint32_t under:1;
            uint32_t in0_zero:1;
            uint32_t in1_zero:1;
            uint32_t reserved:26;
        };
        uint32_t val;
    };
} xfp_status_t;

typedef struct hmx_xfp {
    xfp_status_t status;
    int32_t exp;               // Unbiased Exponent
    int64_t sig;               // Signed significand in Q INT.FRAC format
    uint8_t INT;                // Number of integer bits + sign bit
    uint8_t FRAC;               // Number of fractional bits
    uint8_t EXP;                // Number of exponent bits
    uint8_t lza;
} hmx_xfp_t;

typedef struct {
	union {
		hmx_xfp_t xfp[2];			// Custom Floating Point format Accumulators s
		size16s_t val[MAX_HMX_ACC_BYTES/16];
		uint16_t  hf[MAX_HMX_ACC_BYTES/2];
		uint64_t  ud[MAX_HMX_ACC_BYTES/8];
		 int64_t   d[MAX_HMX_ACC_BYTES/8];
		uint32_t  uw[MAX_HMX_ACC_BYTES/4];
		 int32_t   w[MAX_HMX_ACC_BYTES/4];
		uint16_t  uh[MAX_HMX_ACC_BYTES/2];
		 int16_t   h[MAX_HMX_ACC_BYTES/2];
		 uint8_t  ub[MAX_HMX_ACC_BYTES/1];
		  int8_t   b[MAX_HMX_ACC_BYTES/1];
	};
	union {
		int8_t ovf[2];
		uint16_t bias_state;
	};
} hmx_acc_t;

typedef union usr_fp_reg  {
    struct {
        uint32_t inf_nan_enable:1;
        uint32_t nan_propagate:1;
        uint32_t reserved:30;
    };
    uint32_t raw;
} usr_fp_reg_t;


typedef union hmx_cvt_rs_reg  {
    struct {
        uint32_t acc_clear:1;
        uint32_t relu:1;
        uint32_t fb_dst:2;
        uint32_t fb_limit:1;
        uint32_t reserved0:1;
        uint32_t fp_maxnorm:1;
        uint32_t fp_type:1;
        uint32_t fp_rnd:1;
        uint32_t fxp16_ch_sel:2;
		uint32_t reserved2:1;
		uint32_t bias_sel:2;
        uint32_t reserved:18;
    };
    uint32_t raw;
} hmx_cvt_rs_reg_t;

typedef enum {
	HMX_CVT_FB_OUTBIAS=1,
	HMX_CVT_FB_SCALE=2
} hmx_cvt_fd_dst_t;


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
		size2u_t bias_sel:2;
	};
	size2u_t val;
} hmx_commit_state_t;

typedef struct {
	uint16_t wgt;
	uint8_t valid;
} hmx_wgt_cache_t;

#define WGT_CACHE_MAX_SIZE 2148

typedef struct HMX_State {


	vaddr_t start[2];
	vaddr_t range[2];
	paddr_t paddr[2];
	uint16_t type[2];

	uint32_t mac_cycle_limit;
	uint32_t limit_execeeded;

	paddr_t act_addr;
	paddr_t wgt_addr;
	paddr_t max_weight_pa;

	uint32_t operand_ready:4;
	uint8_t weight_count;


	hmx_commit_state_t fxp_commit_state;
	hmx_commit_state_t flt_commit_state;

	// reviewed parameters (convert so far)
	usr_fp_reg_t usr_fp;

	uint32_t enable16x16:1;
	uint32_t outputselect16x16:1;
	uint32_t cvt_type:4;
	uint32_t current_acc_fxp:1;
	uint32_t current_acc_flt:1;
	uint32_t is_flt:1;
	uint32_t is_bf16:1;
	uint32_t format:2;
	uint32_t act_block_type:4;
	uint32_t wgt_block_type:4;

	uint32_t redundant_acc:1;
	uint32_t acc_select:1;
	uint32_t wgt_deep:1;
	uint32_t deep:1;
	uint32_t drop:1;
	uint32_t y_dilate:1;
	uint32_t x_dilate:1;
	uint32_t batch:1;
	uint32_t support_bf16:1;
	uint32_t support_fp16:1;
	uint32_t wgtc_mode:1;
	uint32_t group_convolution:1;
	uint32_t group_size:6;
	uint32_t group_count:6;
	uint32_t bias_32bit:1;
	uint32_t weight_bits:5;
	uint32_t weight_bytes_per_cycle:32;
	uint32_t wgtc_global_density:8;

	uint8_t wgtc_start_offset;
	uint16_t wgtc_total_bytes;

	uint8_t blocks;
	uint8_t ch_start;
	uint8_t ch_stop;

	int32_t dY;

	// Todo: Put in a struct?
	uint32_t tile_x_mask;
	uint32_t tile_x_inc;
	uint32_t x_offset;
	uint32_t x_start;
	uint32_t x_stop;
	uint32_t fx;
	uint32_t x_tap;

	uint32_t tile_y_mask;
	uint32_t tile_y_inc;
	uint32_t y_start;
	uint32_t y_stop;
	uint32_t y_offset;
	uint32_t fy;
	uint32_t y_tap;

	uint32_t x_acc_offset;
	uint16_t wgt_negate;
	uint32_t acc_range:1;
	int64_t lo_msb;
	int64_t lo_mask;
	uint8_t data_type;

	uint32_t internal_bias_value;


	uint8_t QDSP6_MX_FP_ACC_NORM;
	uint8_t QDSP6_MX_FP_ACC_INT;
	uint8_t QDSP6_MX_FP_ACC_FRAC;
	uint8_t QDSP6_MX_FP_ACC_EXP;
	uint8_t QDSP6_MX_FP_RATE;
	uint8_t QDSP6_MX_RATE;
	uint8_t QDSP6_MX_CVT_WIDTH;
	uint8_t QDSP6_MX_FP_ROWS;
	uint8_t QDSP6_MX_ROWS;
	uint8_t QDSP6_MX_FP_COLS;
	uint8_t QDSP6_MX_COLS;
 	uint8_t QDSP6_MX_PARALLEL_GRPS;
	uint8_t QDSP6_MX_SUB_COLS;

	// TEMP, until we can remove Pointers back to processor, eventually become void pointers
	system_t * system;
	thread_t * thread;
	processor_t * proc;

	uint32_t threadId;

	uint32_t pktid;

	FILE * fp_hmx_debug;
	uint32_t hmx_debug_lvl;


	hmx_acc_t accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
	hmx_acc_t accum_flt[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

	size1u_t cvt_accum_current_index;
	hmx_acc_t cvt_accum[MAX_CONVERT_STATES][MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

		hmx_acc_t cvt_future_accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
		hmx_acc_t cvt_future_accum_flt[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

	hmx_acc_t cvt_fallback_future_accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
	hmx_acc_t cvt_fallback_future_accum_flt[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

	hmx_bias_t bias[MAX_BIAS_STATES][MAX_ACCUMULATORS_DEPTH/2];
	hmx_bias_t future_bias[MAX_BIAS_STATES][MAX_ACCUMULATORS_DEPTH/2];
		hmx_acc_t future_accum_fxp[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];
		hmx_acc_t future_accum_flt[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH];

	hmx_wgt_cache_t wgt_cache[WGT_CACHE_MAX_SIZE][64];	// WGT Stream per output channel
	uint16_t 	    act_cache[WGT_CACHE_MAX_SIZE][64];	// WGT Stream per output channel

	union {
		uint8_t act_cache_ub[2048*2];
		uint16_t act_cache_uh[2048*2/2];
		uint64_t act_cache_dw[2048*2/8];
	};
#ifdef VERIFICATION
	union {
		uint8_t act_cache_ub_accessed[2048*2];
		uint16_t act_cache_uh_accessed[2048*2/2];
		uint64_t act_cache_dw_accessed[2048*2/8];
	};
#endif
	hmx_xfp_t tmp_flt_acc_cache[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH][8];	// Max Input Channel Rate by Max output channels for FP16/BF16

	uint8_t mpy_matrix[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH][MAX_INPUT_CHANNELS];
	uint8_t mpy_matrix_pre[MAX_ACCUMULATORS_SPATIAL][MAX_ACCUMULATORS_DEPTH][MAX_RATE_INPUT_CHANNELS];
	uint32_t array_mpy[MAX_RATE_INPUT_CHANNELS];
	uint32_t array_acc;

	uint32_t fp8_batch_ch_start_wgt_idx;

} hmx_state_t;


// Function Pointers

typedef int64_t (*hmx_acc_select_ptr_t )(hmx_state_t *,  int32_t,  int32_t, int32_t );
typedef void (*hmx_cvt_body_ptr_t)(hmx_state_t *,  uint32_t,  uint32_t,  uint32_t,  hmx_bias_t,  uint32_t,  uint32_t, hmx_cvt_rs_reg_t,  void *); // void pointer???
typedef void (*hmx_mult_body_ptr_t)(hmx_state_t *, uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t,  uint32_t);
typedef int8_t (*hmx_unpack_ptr_t)( int8_t,  int32_t);

uint32_t hmx_get_usr_reg_coproc_field(thread_t* thread);

void hmx_legacy_convert_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t vaddr, vaddr_t range, int format, int direction, int type);
void hmx_convert_init(thread_t* thread, int slot, int type, int feedback);
void hmx_convert_store_init(thread_t* thread, int slot, vaddr_t ea, vaddr_t start, vaddr_t range, int format, int type);

void hmx_mxmem_wr_xlate(thread_t* thread, int slot, vaddr_t ea, vaddr_t start, vaddr_t range, int access_type);

void hmx_act_init(thread_t* thread, vaddr_t start, vaddr_t range, int32_t slot);
void hmx_wgt_init(thread_t* thread, vaddr_t start, vaddr_t range, int32_t slot, int32_t weight_count, int32_t output_ch_scale);

void hmx_bias_init(thread_t* thread, int slot, vaddr_t vaddr, int access_type, int size);

void hmx_debug_print_acc(thread_t* thread, int hex, int flt);
void hmx_debug_file_log(thread_t* thread, int lvl, char * buf);
void hmx_acc_reset(processor_t * proc);
void hmx_debug_log_mac_info(thread_t * thread);


#define THREAD2HMXSTRUCT ((hmx_state_t*)thread->processor_ptr->shared_extptr)


#ifdef VERIFICAITON
#define CALL_HMX_CMD(NAME, STATE, ...) NAME##_##debug(STATE, __VA_ARGS__);
#else
#define CALL_HMX_CMD(NAME, STATE, ...) \
    {NAME(STATE, __VA_ARGS__);}
#if 0
	if ((STATE->fp_hmx_debug != NULL))  { NAME##_##debug(STATE, __VA_ARGS__); }\
	else {NAME(STATE, __VA_ARGS__); }
#endif
#endif



#ifdef VERIFICATION
#define  fMX_DEBUG_LOG(LVL,...) \
	{\
		if (THREAD2HMXSTRUCT->fp_hmx_debug && (THREAD2HMXSTRUCT->hmx_debug_lvl >= LVL)) {\
			fprintf(THREAD2HMXSTRUCT->fp_hmx_debug, __VA_ARGS__);\
			fprintf(THREAD2HMXSTRUCT->fp_hmx_debug, "\n");\
		}\
	}
#else
#if 1
#define  fMX_DEBUG_LOG(LVL,...)
#else
#define  fMX_DEBUG_LOG(LVL,...) \
	{\
		if (thread->processor_ptr->arch_proc_options->hmxdebugfile && (thread->processor_ptr->monotonic_pcycles > thread->processor_ptr->arch_proc_options->hmxdebugfile_start_pcycle) && (thread->processor_ptr->monotonic_pcycles < thread->processor_ptr->arch_proc_options->hmxdebugfile_end_pcycle)) {\
			char buf[1024];\
			sprintf(buf, __VA_ARGS__);\
			hmx_debug_file_log(thread, LVL, buf);\
		}\
	}
#endif
#endif

#endif

