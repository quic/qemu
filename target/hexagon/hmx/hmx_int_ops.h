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
#ifndef _MPY_INT_OPS_H_
#define _MPY_INT_OPS_H_

#include "hex_arch_types.h"
#include "int16_emu.h"
#include "mpy_hmx_support.h"

#define ThreadState CPUArchState
#define thread_t    CPUHexagonState

struct HMX_State;
union hmx_bias;
union hmx_cvt_rs_reg;

#ifndef ARCH_FUNCTION_DECL
#define ARCH_FUNCTION_DECL(rval, fname, ...) \
rval fname(__VA_ARGS__);
#endif


#ifdef STANDALONE

typedef struct arch_proc_opt {
    int hmx_mxmem_debug;
	int	hmx_fp16_acc_width_int;
    int	hmx_fp16_acc_width_frac;
    int hmx_mxmem_debug_depth;
    int hmx_mxmem_debug_spatial;
    int QDSP6_MX_FP_ACC_FRAC;
    int QDSP6_MX_FP_PRESENT;
    int QDSP6_MX_CVT_WIDTH;
} arch_proc_opt_t;

typedef struct th {
        int tmp;
} struct ThreadState;

typedef struct ProcessorState {
    struct ThreadState * thread[1];
    arch_proc_opt_t * arch_proc_options;
} processor_t;

#else

#endif

// Multiply Functions
#define hmx_fxp_mac(state_ptr, acc, act, wgt, callback_wgt, spatial_idx, output_ch, acc_idx, input_ch, x_tap, y_tap, block_idx, deep_block_idx) \
	((int32_t)acc + ((int32_t)((int16_t)wgt) * (int32_t)((uint16_t)act)))
ARCH_FUNCTION_DECL(void, hmx_multiply,thread_t *env, struct HMX_State * state_ptr, uint32_t weights_per_byte_log, uint32_t wgt_per_word, uint32_t unpack, uint32_t type, uint32_t mult_type, uint32_t output_channel_scale);
ARCH_FUNCTION_DECL(void, hmx_mult_inner,struct HMX_State * state_ptr, int32_t row,uint32_t acc_select,uint32_t act,uint32_t wgt_stream_idx,uint32_t mult_type,uint32_t input_channel,uint32_t x_tap,uint32_t y_tap,uint32_t block,uint32_t deep_block,uint32_t output2x,uint32_t is_flt,uint32_t grp_idx,uint32_t grp_start,uint32_t grp_end,uint32_t grp_size, uint32_t fp8_ch_start, uint32_t fp8_ch_end);
ARCH_FUNCTION_DECL(void, hmx_mult_xfp,struct HMX_State * state_ptr, uint32_t row, uint32_t col, uint32_t sel, uint32_t act, uint32_t wgt, uint32_t in_chan, uint32_t x_tap, uint32_t y_tap, uint32_t block, uint32_t output2x_unused, uint32_t deep_block, uint32_t grp_idx, uint32_t grp_size);

// Convert Functions
ARCH_FUNCTION_DECL(int64_t,  hmx_combine_redundant_acc, struct HMX_State * state_ptr, int32_t hi,     int32_t lo, int32_t bias);
ARCH_FUNCTION_DECL(int64_t,  hmx_non_redundant_acc,     struct HMX_State * state_ptr, int32_t hi,     int32_t lo, int32_t bias);
ARCH_FUNCTION_DECL(uint32_t, hmx_u8_cvt,                struct HMX_State * state_ptr, int64_t acc,    int32_t bias32, int16_t exp,    int16_t zeroing, int16_t sig,     uint16_t out_bias, int32_t sat,     int16_t legacy);
ARCH_FUNCTION_DECL(uint32_t, hmx_u16_cvt,               struct HMX_State * state_ptr, int64_t acc_hl, int64_t acc_ll, int32_t bias32, int16_t exp,     int16_t zeroing, int16_t sig,       int16_t rnd_bit, int32_t sat,   int16_t legacy);
ARCH_FUNCTION_DECL(uint32_t, hmx_u16x16_cvt,            struct HMX_State * state_ptr, int64_t acc_hh, int64_t acc_hl, int64_t acc_lh, int64_t acc_ll,  int64_t bias48,  int16_t exp,       int16_t zeroing, int32_t sig,   uint32_t out_bias, int32_t sat, int16_t legacy);

ARCH_FUNCTION_DECL(void, hmx_xfp_convert_body,   struct HMX_State * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, union hmx_bias bias, uint32_t subchannel, uint32_t legacy, union hmx_cvt_rs_reg rs, void * acc_select_fptr);
ARCH_FUNCTION_DECL(void, hmx_8x4_convert_body,   struct HMX_State * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, union hmx_bias bias, uint32_t subchannel, uint32_t legacy, union hmx_cvt_rs_reg rs, void * acc_select_fptr);
ARCH_FUNCTION_DECL(void, hmx_8x8_convert_body,   struct HMX_State * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, union hmx_bias bias, uint32_t subchannel, uint32_t legacy, union hmx_cvt_rs_reg rs, void * acc_select_fptr);
ARCH_FUNCTION_DECL(void, hmx_16x8_convert_body,  struct HMX_State * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, union hmx_bias bias, uint32_t subchannel, uint32_t legacy, union hmx_cvt_rs_reg rs, void * acc_select_fptr);
ARCH_FUNCTION_DECL(void, hmx_16x16_convert_body, struct HMX_State * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, union hmx_bias bias, uint32_t subchannel, uint32_t legacy, union hmx_cvt_rs_reg rs, void * acc_select_fptr);
ARCH_FUNCTION_DECL(void, hmx_acc_convert,        struct HMX_State * state_ptr, union hmx_cvt_rs_reg cvt_rs, const uint32_t type, const uint32_t subchannel, const uint32_t legacy);

// Memory Access Functions
ARCH_FUNCTION_DECL(void, hmx_unpack_bytes,struct HMX_State * state_ptr, uint32_t raw_wgt, uint32_t output_ch_wgt_idx, uint32_t wgt_cache_idx, uint32_t wgt_per_word, uint32_t output_scale, uint32_t unpack_idx, paddr_t wgt_addr );
ARCH_FUNCTION_DECL(int8_t, hmx_wgt_ld_meta_data,thread_t *env, struct HMX_State * state_ptr, uint32_t * metadata, paddr_t wgt_uc_metadata_addr,  paddr_t wgt_addr, paddr_t wgt_addr_end);
ARCH_FUNCTION_DECL(void, hmx_ld_wgt,thread_t *env, struct HMX_State * state_ptr, paddr_t wgt_addr, paddr_t wgt_addr_end, uint32_t wgt_per_word, uint32_t output_scale, uint32_t is_flt, uint32_t unpack_idx );
ARCH_FUNCTION_DECL(void, hmx_ld_act,thread_t *env, struct HMX_State * state_ptr, const paddr_t paddr, const uint32_t block_idx);

// Misc, PMU related, timing mode related
ARCH_FUNCTION_DECL(void, hmx_mac_pmu,struct HMX_State * state_ptr, const uint32_t is_flt, uint32_t current_block);
ARCH_FUNCTION_DECL(void, hmx_populate_hmx_maptr,struct ThreadState * thread, struct HMX_State * state_ptr);

ARCH_FUNCTION_DECL(void, hmx_populate_hmx_mac_maptr,struct ThreadState * thread, struct HMX_State * state_ptr);

// Interfacing functions
ARCH_FUNCTION_DECL(void, hmx_configure_state, struct ThreadState * thread);
ARCH_FUNCTION_DECL(void, hmx_tmp_set_state, struct ThreadState * thread, struct HMX_State * state_ptr);
ARCH_FUNCTION_DECL(void, hmx_cvt_mem_parameters, struct HMX_State * state_ptr, uint32_t start, uint32_t range, uint32_t type, uint32_t format, uint32_t direction);
ARCH_FUNCTION_DECL(void, hmx_cvt_tx_parameters, struct HMX_State * state_ptr, uint32_t usr, uint32_t type, uint32_t feedback);
ARCH_FUNCTION_DECL(void, hmx_act_paramaters, struct HMX_State * state_ptr, vaddr_t start, vaddr_t range, uint32_t slot, uint32_t type, uint32_t format, uint32_t block_type);
ARCH_FUNCTION_DECL(void, hmx_wgt_paramaters, struct HMX_State * state_ptr, vaddr_t start, vaddr_t range, uint32_t slot, uint32_t type, uint32_t block_type, uint32_t weight_count, uint32_t output_ch_scale, uint32_t unpack, uint32_t usr);

ARCH_FUNCTION_DECL(int32_t, hmx_inc_with_spatial_mask, int32_t in, int32_t inc, int32_t mask);
ARCH_FUNCTION_DECL(int32_t, hmx_inc_with_spatial_mask_ovf, int32_t in, int32_t inc, int32_t mask, int32_t * overflow);
ARCH_FUNCTION_DECL(int32_t, hmx_inc_tap_with_spatial_mask_dilate, int32_t in, int32_t inc, int32_t mask, int32_t dilate);
ARCH_FUNCTION_DECL(int32_t, hmx_compute_filter_size, int32_t size, int32_t start, int32_t mask,  int32_t inc);
ARCH_FUNCTION_DECL(int32_t, hmx_compute_filter_size_above, int32_t size, int32_t start, int32_t mask,  int32_t inc);

ARCH_FUNCTION_DECL(int8_t, hmx_unpack_none,int8_t in, int32_t bit_select);
ARCH_FUNCTION_DECL(int8_t, hmx_unpack_byte_from_byte,int8_t in, int32_t bit_select);
ARCH_FUNCTION_DECL(int8_t, hmx_unpack_sm_from_byte,int8_t in, int32_t bit_select);
ARCH_FUNCTION_DECL(int8_t, hmx_unpack_1sbit_from_byte,int8_t in, int32_t bit_select);
ARCH_FUNCTION_DECL(int8_t, hmx_unpack_1bit_from_byte,int8_t in, int32_t bit_select);
ARCH_FUNCTION_DECL(int8_t, hmx_unpack_nibble_from_byte,int8_t in, int32_t bit_select);
ARCH_FUNCTION_DECL(int8_t, hmx_unpack_crumb_from_byte, int8_t in, int32_t bit_select);
ARCH_FUNCTION_DECL(int8_t, hmx_unpack_scrumb_from_byte,int8_t in, int32_t bit_select);



void hmx_update_verif_trace(struct ThreadState * thread);
#endif


