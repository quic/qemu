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

#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "../internal.h"
#include "cpu.h"
#include "arch.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#endif
#include "op_helper.h"
//#include "utils.h"
#include "hmx/ext_hmx.h"
#include "hmx/hmx.h"
#include "arch_options_calc.h"
//#define env state_ptr
#include "macros.h"
#include "hmx/macros_auto.h"

#include <math.h>
#include "hmx_int_ops.h"

#define INC_PSTAT(...)
#define INC_PSTATN(...)
#define INC_PSTATNPC(...)

#ifndef ARCH_FUNCTION
#define ARCH_FUNCTION(func_name) func_name
#endif

//#ifdef DBG_TRACING

#define UNUSED(var) do { (void)var; } while (0)

#if defined (DBG_TRACING) || defined (VERIFICATION)
#define DEBUG_PRINT(LVL, ...) \
if (state_ptr->fp_hmx_debug && (state_ptr->hmx_debug_lvl >= LVL)) {\
    fprintf(state_ptr->fp_hmx_debug, __VA_ARGS__);\
    fprintf(state_ptr->fp_hmx_debug, "\n");\
    }
#define DEBUG_ONLY_VAR(var)
#else
#define DEBUG_PRINT(LVL, ...)
#define DEBUG_ONLY_VAR(var) UNUSED(var)
#endif

#if defined (DBG_TRACING) || defined (VERIFICATION)
#define DEBUG_PRINT_VAR(VAR)\
VAR
#else
#define DEBUG_PRINT_VAR(VAR)\

#endif



#ifdef STANDALONE
#define VERIF(...)
#else
#ifdef VERIFICATION
#define VERIF(...) __VA_ARGS__
#else
#define VERIF(...)
#endif
#endif


static uint32_t ARCH_FUNCTION(sat_to_max)(hmx_state_t * state_ptr, int64_t in, int32_t element_size, int32_t sat) {
    int convert_width = 12;
    int64_t max_element = (1 << (element_size*convert_width)) - 1;
    uint32_t out = 0;
    if (sat) {
        if (in < 0) {
            out = 0;
        } else if (in > max_element) {
            out = max_element;
        } else {
            out = (in & max_element);
        }
    } else {
       out = (in & max_element);
    }
    DEBUG_PRINT(2, "in = %llx out=%02x.%01x (sat=%d)", (long long int)in, (out>>4), (out & 0xF), sat);    // Might need to get fixed for elements bigger than byte
    return out;
}

static inline int64_t ARCH_FUNCTION(acc_shift)(hmx_state_t * state_ptr, int64_t acc_biased, int32_t exp, int32_t sat, int32_t frac_bits, int32_t int_bits) {
    int32_t shift_acc = (32-frac_bits);
    int64_t acc_shifted = (acc_biased << exp);
    int64_t mask =((int64_t)((1llu << (frac_bits + int_bits)) - 1) ) << shift_acc;
    if (sat) {
        mask |= ((int64_t)((1llu << 32) - 1) ) << 32;
    }
    acc_shifted &= mask ;  // Clip only fractional bits

    DEBUG_PRINT(2, "mask:         %08x.%08x (Q32.32)", (int32_t)(mask   >> 32), (uint32_t)mask);
    DEBUG_PRINT(2, "shifted:      %08x.%08x (Q32.%d) exp=%d" , (uint32_t)(acc_shifted  >> 32), (uint32_t)acc_shifted, frac_bits, exp);

    return acc_shifted;
}

static inline int64_t ARCH_FUNCTION(acc_rectify)(hmx_state_t * state_ptr, int64_t acc_shifted, int16_t zeroing, int16_t legacy, int64_t acc_biased, uint16_t element_size) {
    int64_t acc_rectified = 0;
    int64_t summarize_output = 0;
    int64_t sign_bit = 0;
    uint16_t frac_bits = element_size * 12;
    int64_t summarization_bits = acc_shifted & 0xFFFFFFFE00000000ll;

    if(acc_biased < 0){
        sign_bit = 0x400000000ll;
    }
    if(sign_bit){
        if (summarization_bits == 0xFFFFFFFE00000000ll){
            summarize_output = 0x200000000ll;
        }
    }
    else{
        if (summarization_bits){
            summarize_output = 0x200000000ll;
        }
    }

    int64_t maskbits = ((1ll << (frac_bits+1)) - 1) << (32 - frac_bits);
    acc_shifted &= maskbits;
    acc_shifted |= summarize_output;
    acc_shifted |= sign_bit;
    if(!legacy && acc_shifted){
        acc_shifted |= 1ll << (31 - frac_bits);
    }
    if(((zeroing >= 4) || ((zeroing == 3) && sign_bit)) && !((zeroing == 7) && sign_bit)){
        acc_shifted &= (0x000000FFFFF00000ll | maskbits);
    }
    acc_shifted = (acc_shifted << 29) >> 29 >> (31 - frac_bits);
    DEBUG_PRINT(2, "acc_shifted:  %08x.%08x (Q4.%d)" , (uint32_t)(acc_shifted  >> 32), (uint32_t)acc_shifted, frac_bits);

    switch(zeroing) {
        case 1 :
            acc_rectified = (acc_shifted > 0) ? 0 : acc_shifted; //min
            break;
        case 2 :
            acc_rectified = (acc_shifted < 0) ? 0 : acc_shifted; //max
            break;
        case 3 :
            if(acc_shifted != 0){
                acc_rectified = (acc_shifted >= 0) ? acc_shifted : -acc_shifted-1; //abs
            }
            break;
        case 4 :
            if(acc_biased != 0){
                acc_rectified = -acc_shifted-1;
            }
            break;
        case 5 :
            acc_rectified = (acc_biased >= 0) ? 0 : -acc_shifted-1; //-min
            break;
        case 6 :
            acc_rectified = (acc_biased <= 0) ? 0 : -acc_shifted-1; //-max
            break;
        case 7 :
            if(acc_biased != 0){
                acc_rectified = (acc_shifted >= 0) ? -acc_shifted-1 : acc_shifted; //-abs
            }
            break;
        default :
            acc_rectified = acc_shifted;
            break;
    }

    acc_rectified = acc_rectified << (31 - frac_bits);

    DEBUG_PRINT(2, "acc_rectified:%08x.%08x (Q4.%d) zeroing=%d" , (uint32_t)(acc_rectified  >> 32), (uint32_t)acc_rectified, frac_bits, zeroing);

    return acc_rectified;
}

static inline size16s_t ARCH_FUNCTION(acc_scale)(hmx_state_t * state_ptr, int64_t acc_rectified, int64_t scale_cvt) {
    size16s_t acc_scaled = mult64_to_128(scale_cvt, acc_rectified);
    DEBUG_PRINT(2, "scale:        %08x.%08x (Q32.10)", (int32_t)(scale_cvt   >> 32), (uint32_t)scale_cvt);
    DEBUG_PRINT(2, "acc_scaled:   %016llx.%016llx (Q64.64)", (long long int)(acc_scaled.hi), (long long int)acc_scaled.lo);
    return acc_scaled;
}

static inline int64_t ARCH_FUNCTION(acc_bias)(hmx_state_t * state_ptr, size16s_t acc_scaled, int32_t element_size, uint32_t rnd_bit, int32_t frac_bits){
    int64_t ulp_bit = 64 - 8 * element_size - 3 - 1;
    size16s_t ulp;
    size16s_t acc_rnd;

    ulp = cast8s_to_16s( (int64_t)rnd_bit << ulp_bit);
#ifdef HEX_CONFIG_INT128
    size8u_t lo = int128_getlo(ulp);
    ulp= int128_make128(lo, 0);
#else
    ulp.hi = 0;
#endif
    acc_scaled = add128(acc_scaled, acc_scaled);
    acc_rnd = add128(acc_scaled, ulp); // + ULP
    ulp_bit += 3;
    int convert_width = 12;;

    int64_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1-((convert_width - 8)*element_size)))); // Q31.16

    DEBUG_PRINT(2, "ulp:          %016llx.%016llx (Q64.64) ulp_bit=%lld vrnd_bit=%x", (long long int)(ulp.hi), (long long int)ulp.lo, (long long int)ulp_bit, rnd_bit);
    DEBUG_PRINT(2, "acc_rnd:      %016llx.%016llx (Q64.64)", (long long int)(acc_rnd.hi), (long long int)acc_rnd.lo);
    DEBUG_PRINT(2, "acc_final:    %08x.%08x (Q32.%d)", (uint32_t)(acc_final    >> (element_size*convert_width)), (uint32_t)acc_final<<(32-(element_size*convert_width)), frac_bits );

    return acc_final;
}

static inline int64_t ARCH_FUNCTION(acc_rnd)(hmx_state_t * state_ptr, size16s_t acc_scaled, int32_t element_size, int32_t rnd_bit, int32_t frac_bits){
    int convert_width = 12;
    int64_t ulp_bit = 64 - 8 * element_size - 1;
    size16s_t ulp;
    size16s_t acc_rnd;

    ulp = cast8s_to_16s( (int64_t)rnd_bit << ulp_bit);
    acc_scaled = add128(acc_scaled, acc_scaled);
    acc_rnd = add128(acc_scaled, ulp); // + ULP
    int64_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1)-((convert_width - 8)*element_size))); // Q31.24

    DEBUG_PRINT(2, "acc_rnd:      %016llx.%016llx (Q64.64) ulp_bit=%lld vrnd_bit=%x", (long long int)(acc_rnd.hi), (long long int)acc_rnd.lo, (long long int)ulp_bit, rnd_bit);
    DEBUG_PRINT(2, "acc_final:    %08x.%08x (Q32.%d)", (uint32_t)(acc_final    >> (ulp_bit+1)), (uint32_t)acc_final<<(ulp_bit+1), frac_bits );

    return acc_final;
}

uint32_t ARCH_FUNCTION(hmx_u8_cvt)(hmx_state_t * state_ptr, int64_t acc, int32_t bias32, int16_t exp, int16_t zeroing, int16_t sig, uint16_t out_bias, int32_t sat, int16_t legacy){

    const int32_t element_size = 1;
    const int32_t frac_bits = 12;
    int64_t bias32_add = (int64_t)bias32;
    int64_t acc_biased  = (acc + bias32_add);

    DEBUG_PRINT(2, "bias32:       %08x.%08x (Q32.32)", (int32_t)(bias32_add   >> 32), (uint32_t)bias32_add);
    DEBUG_PRINT(2, "acc_cvt:      %08x.%08x (Q32.32)", (uint32_t)(acc      >> 32), (uint32_t)acc);
    DEBUG_PRINT(2, "acc_biased:   %08x.%08x (Q32.31)", (uint32_t)(acc_biased   >> 32), (uint32_t)acc_biased);

    int64_t scale_cvt = ((int64_t)sig) << 20;                   // Q0.12
    int32_t int_bits = 32;
    int64_t acc_shifted = ARCH_FUNCTION(acc_shift)(state_ptr, acc_biased, exp, sat, frac_bits, int_bits);
    int64_t acc_rectified = ARCH_FUNCTION(acc_rectify)(state_ptr, acc_shifted, zeroing, legacy, acc_biased, element_size);
    size16s_t acc_scaled = ARCH_FUNCTION(acc_scale)(state_ptr, acc_rectified, scale_cvt);
    int64_t acc_final  = ARCH_FUNCTION(acc_bias)(state_ptr, acc_scaled, element_size, out_bias, frac_bits); //left shift to get proper decimal location

    return ARCH_FUNCTION(sat_to_max)(state_ptr, acc_final,  element_size, sat);
}


uint32_t ARCH_FUNCTION(hmx_u16_cvt)(hmx_state_t * state_ptr, int64_t acc_hl, int64_t acc_ll, int32_t bias32, int16_t exp, int16_t zeroing, int16_t sig, int16_t rnd_bit, int32_t sat, int16_t legacy) {
    const int32_t element_size = 2;
    const int32_t frac_bits = 24;

    int64_t bias32_add = (int64_t)bias32; // sign extend from Q0.31 -> Q31.32
    int64_t acc_combined = acc_hl + (acc_ll >> 8);
    int64_t acc_biased = acc_combined + bias32_add;

    DEBUG_PRINT(2, "bias32:       %08x.%08x (Q32.32)", (int32_t) (bias32_add   >> 32), (uint32_t)bias32_add);
    DEBUG_PRINT(2, "acc_ll_cvt:   %08x.%08x (Q32.32)", (uint32_t)(acc_ll       >> 32), (uint32_t)acc_ll);
    DEBUG_PRINT(2, "acc_hl_cvt:   %08x.%08x (Q32.32)", (uint32_t)(acc_hl       >> 32), (uint32_t)acc_hl);
    DEBUG_PRINT(2, "acc_combined: %08x.%08x (Q32.32)", (uint32_t)(acc_combined >> 32), (uint32_t)acc_combined);
    DEBUG_PRINT(2, "acc_biased:   %08x.%08x (Q32.32)", (uint32_t)(acc_biased   >> 32), (uint32_t)acc_biased);

    int64_t scale_cvt = ((int64_t)sig) << 20;                   // Q0.12
    int32_t int_bits = 32;
    int64_t acc_shifted = ARCH_FUNCTION(acc_shift)(state_ptr, acc_biased, exp, sat, frac_bits, int_bits);
    int64_t acc_rectified = ARCH_FUNCTION(acc_rectify)(state_ptr, acc_shifted, zeroing, legacy, acc_biased, element_size);
    size16s_t acc_scaled = ARCH_FUNCTION(acc_scale)(state_ptr, acc_rectified, scale_cvt);
    int64_t acc_final = ARCH_FUNCTION(acc_rnd)(state_ptr, acc_scaled, element_size, rnd_bit, frac_bits);
    return ARCH_FUNCTION(sat_to_max)(state_ptr, acc_final,  element_size, sat);
}


uint32_t ARCH_FUNCTION(hmx_u16x16_cvt)(hmx_state_t * state_ptr, int64_t acc_hh, int64_t acc_hl,  int64_t acc_lh, int64_t acc_ll, int64_t bias48, int16_t exp, int16_t zeroing, int32_t sig, uint32_t out_bias, int32_t sat, int16_t legacy) {
    const int32_t element_size = 2;
    const int32_t frac_bits = 24;

    bias48 = (bias48 << 16) >> 16;          // sign extend from Q15.31 -> Q31.32

    int64_t acc_combined = acc_ll + ((acc_lh + acc_hl + (acc_hh << 8) ) << 8);
    int64_t acc_biased = (acc_combined + bias48) >> 16;

    DEBUG_PRINT(2, "bias48:       %08x.%08x (Q32.32)", (int32_t) (bias48       >> 32), (uint32_t)bias48);
    DEBUG_PRINT(2, "acc_ll_cvt:   %08x.%08x (Q32.32)", (uint32_t)(acc_ll       >> 32), (uint32_t)acc_ll);
    DEBUG_PRINT(2, "acc_lh_cvt:   %08x.%08x (Q32.32)", (uint32_t)(acc_lh       >> 32), (uint32_t)acc_lh);
    DEBUG_PRINT(2, "acc_hl_cvt:   %08x.%08x (Q32.32)", (uint32_t)(acc_hl       >> 32), (uint32_t)acc_hl);
    DEBUG_PRINT(2, "acc_hh_cvt:   %08x.%08x (Q32.32)", (uint32_t)(acc_hh       >> 32), (uint32_t)acc_hh);
    DEBUG_PRINT(2, "acc_combined: %08x.%08x (Q32.32)", (uint32_t)(acc_combined >> 32), (uint32_t)acc_combined);
    DEBUG_PRINT(2, "acc_biased:   %08x.%08x (Q32.32)", (int32_t) (acc_biased   >> 32), (uint32_t)acc_biased);

    int64_t scale_cvt = (int64_t)sig << 10;                   // Q0.22
    int32_t int_bits = 32;
    int64_t acc_shifted = ARCH_FUNCTION(acc_shift)(state_ptr, acc_biased, exp, sat, frac_bits, int_bits);
    int64_t acc_rectified = ARCH_FUNCTION(acc_rectify)(state_ptr, acc_shifted, zeroing, legacy, acc_biased, element_size);
    size16s_t acc_scaled = ARCH_FUNCTION(acc_scale)(state_ptr, acc_rectified, scale_cvt);
    int64_t acc_final = ARCH_FUNCTION(acc_bias)(state_ptr, acc_scaled, element_size, out_bias, frac_bits);

    return ARCH_FUNCTION(sat_to_max)(state_ptr, acc_final,  element_size, sat);
}


void ARCH_FUNCTION(hmx_mult_xfp)(hmx_state_t * state_ptr, uint32_t row, uint32_t col, uint32_t sel, uint32_t act, uint32_t wgt, uint32_t in_chan, uint32_t x_tap, uint32_t y_tap, uint32_t block, uint32_t output2x_unused, uint32_t deep_block, uint32_t grp_idx, uint32_t grp_size)
{
    uint32_t rate = state_ptr->QDSP6_MX_FP_RATE-1;
    uint32_t parallel_grp_size = state_ptr->QDSP6_MX_COLS /state_ptr->QDSP6_MX_PARALLEL_GRPS; //this should be a rtl parameter
    uint32_t mod_in_chan = in_chan & rate;
    uint32_t mod_col = col & (parallel_grp_size-1);
    usr_fp_reg_t usr_reg = state_ptr->usr_fp;
    hmx_xfp_t acc_xfp = state_ptr->future_accum_flt[row][col].xfp[sel];

    if(parallel_grp_size > 8)
	{
		if( (grp_size <= 8) && (mod_col/grp_size != grp_idx % (parallel_grp_size/grp_size)) )
        wgt = 0;
    }  else {
		if( (grp_size <= 4) && (mod_col/grp_size != grp_idx % (parallel_grp_size/grp_size)) )
        wgt = 0;
    }

    DEBUG_PRINT(2, "HMX FP16 XFP MULT INPUT        : pktid=0x%08x  A: %04x W: %04x ACC[%02d][%02d][%02d] in_chan=%d : { exp=%08x sig=%016llx status=%01x Q%02d.%02d e%02d}",
    state_ptr->pktid, (uint16_t)act, (uint16_t)wgt, row>>1, col, sel, in_chan,  acc_xfp.exp, (long long int)acc_xfp.sig, acc_xfp.status.val, acc_xfp.INT, acc_xfp.FRAC, acc_xfp.EXP);

    state_ptr->tmp_flt_acc_cache[row][col][mod_in_chan] = hmx_fp_xfp_mult(state_ptr, usr_reg, act, wgt, state_ptr->is_bf16);	// store results to temporary buffer
    DEBUG_PRINT(2, "HMX FP16 XFP MULT RESULT       : pktid=0x%08x  A: %04x W: %04x ACC[%02d][%02d][%02d] in_chan=%d x_tap=%d y_tap=%d : { exp=%08x sig=%016llx status=%08x Q%02d.%02d e%02d}, grp_size: %d", state_ptr->pktid, (uint16_t)act, (uint16_t)wgt, row>>1, col, sel, in_chan, x_tap, y_tap,  state_ptr->tmp_flt_acc_cache[row][col][mod_in_chan].exp, (long long int)state_ptr->tmp_flt_acc_cache[row][col][mod_in_chan].sig, state_ptr->tmp_flt_acc_cache[row][col][mod_in_chan].status.val, state_ptr->tmp_flt_acc_cache[row][col][mod_in_chan].INT, state_ptr->tmp_flt_acc_cache[row][col][mod_in_chan].FRAC, state_ptr->tmp_flt_acc_cache[row][col][mod_in_chan].EXP, grp_size);

    if (mod_in_chan == rate) {	 // only accumulate after last channel of reduction size
        acc_xfp = ARCH_FUNCTION(hmx_xfp_mac_reduce)(state_ptr, usr_reg, &state_ptr->tmp_flt_acc_cache[row][col][0], acc_xfp, rate+1);
        state_ptr->future_accum_flt[row][col].xfp[sel] = acc_xfp;
    }
#ifdef VERIFICATION
    if (state_ptr->proc->options->hmx_mac_flt_callback) {
        uint32_t ovf = 0;
        int32_t exponent = 0;
        hmx_xfp_t acc_xfp = state_ptr->future_accum_flt[row][col].xfp[sel];
        size16s_t acc = ARCH_FUNCTION(hmx_xfp_to_tb_callback)(state_ptr, &exponent, &ovf, acc_xfp);
        DEBUG_PRINT(2, "HMX XFP MULT CALLBACK          : pktid=0x%08x  A: %04x W: %04x ACC[%02d][%02d][%02d] exponent=%x Significand: 0x%02x.%016llx ovf=%02x true zero=%d channel=%d x_tap=%d y_tap=%d block=%d deep_block=%d", state_ptr->pktid, (uint16_t)act, (uint16_t)wgt, row>>1, col, sel, exponent,
        (uint8_t)(acc.hi>>58) & 0xFF, (long long int)(acc.hi<<6), ovf, acc_xfp.status.zero, in_chan, x_tap, y_tap, block, deep_block);
        state_ptr->proc->options->hmx_mac_flt_callback(state_ptr->system, state_ptr->proc, state_ptr->pktid, (row>>1), col, sel, in_chan, wgt, act, exponent, acc.hi, acc.lo, ovf, x_tap, y_tap, block, deep_block);
	}
#endif
}
#ifdef VERIFICATION
static void ARCH_FUNCTION(verif_mac_callback)(hmx_state_t * state_ptr, int32_t acc, uint8_t act, int8_t wgt, int32_t spatial_idx, int32_t output_ch, int32_t acc_idx, int32_t input_ch, int32_t x_tap, int32_t y_tap, int32_t block_idx, int32_t deep_block_idx )
{
	if (state_ptr->proc->options->hmx_mac_fxp_callback) {
        state_ptr->proc->options->hmx_mac_fxp_callback(state_ptr->system, state_ptr->proc, state_ptr->pktid, spatial_idx, output_ch, acc_idx, input_ch, wgt, act, acc, x_tap, y_tap, block_idx, deep_block_idx);
	}
}
#endif

static inline QEMU_ALWAYS_INLINE void ARCH_FUNCTION(hmx_mult_fxp)(hmx_state_t * state_ptr, uint32_t row, uint32_t col, uint32_t sel, uint32_t act, uint32_t wgt, uint32_t in_chan, uint32_t x_tap, uint32_t y_tap, uint32_t block, uint32_t output2x_unused, uint32_t deep_block, uint32_t grp_idx, uint32_t grp_size)
{
    if (state_ptr->redundant_acc)
    {
        // This could potentially be done on the weight load
        int8_t wgt_in = (int8_t)wgt;
        int32_t wgt_lo_msb = (wgt_in & 0x08);
        int32_t wgt_lo = (wgt_in & 0x07) -wgt_lo_msb;
        int32_t wgt_hi = (wgt_in - wgt_lo) >> 4;
        state_ptr->future_accum_fxp[row][col].w[sel+0] = ARCH_FUNCTION(hmx_fxp_mac)(state_ptr, state_ptr->future_accum_fxp[row][col].w[sel+0], act, wgt_lo,      (wgt & 0xF), row, col, sel+0, in_chan,  x_tap, y_tap, block, deep_block);
        state_ptr->future_accum_fxp[row][col].w[sel+2] = ARCH_FUNCTION(hmx_fxp_mac)(state_ptr, state_ptr->future_accum_fxp[row][col].w[sel+2], act, wgt_hi, ((wgt>>4) & 0xF), row, col, sel+2, in_chan,  x_tap, y_tap, block, deep_block);
    } else {
        state_ptr->future_accum_fxp[row][col].w[sel] = ARCH_FUNCTION(hmx_fxp_mac)(state_ptr, state_ptr->future_accum_fxp[row][col].w[sel], act, wgt, wgt, row, col, sel, in_chan,  x_tap, y_tap, block, deep_block);
    }
    DEBUG_PRINT(2, "HMX FXP OUTPUT MULT       : pktid=0x%08x  FINAL ACC[%02d][%02d][%02d] = %llx", state_ptr->pktid,row, col, sel, (long long int)hmx_combine_redundant_acc(state_ptr, state_ptr->future_accum_fxp[row][col].w[sel+2], state_ptr->future_accum_fxp[row][col].w[sel+0], 0));

}

static inline QEMU_ALWAYS_INLINE void ARCH_FUNCTION(hmx_mult_fxp_subbyte)(hmx_state_t * state_ptr, uint32_t row, uint32_t col, uint32_t sel, uint32_t act, uint32_t wgt, uint32_t in_chan, uint32_t x_tap, uint32_t y_tap, uint32_t block, uint32_t output2x, uint32_t deep_block, uint32_t grp_idx, uint32_t grp_size) {
    int32_t acc_idx = sel + ((output2x) ? 2 : 0);
    state_ptr->future_accum_fxp[row][col].w[acc_idx] = ARCH_FUNCTION(hmx_fxp_mac)(state_ptr, state_ptr->future_accum_fxp[row][col].w[acc_idx], act, wgt, wgt, row, col, acc_idx, in_chan,  x_tap, y_tap, block, deep_block);
}


static inline uint32_t
hmx_get_spatial_mask(uint32_t in, uint32_t offset) {
    return in & ((offset) ? HMX_SPATIAL_MASK_BITS_SM: HMX_SPATIAL_MASK_BITS_CM);
}

int64_t
ARCH_FUNCTION(hmx_non_redundant_acc)(hmx_state_t * state_ptr, int32_t hi,  int32_t lo, int32_t internal_bias) {
    return (int64_t)lo;
}

int64_t
ARCH_FUNCTION(hmx_combine_redundant_acc)(hmx_state_t * state_ptr, int32_t hi,  int32_t lo, int32_t internal_bias) {
    int64_t hi64 = (int64_t)(hi << 4);
    int64_t lo64 = (int64_t)(lo &  state_ptr->lo_mask) - (int64_t)(lo & state_ptr->lo_msb);
    int64_t bias64 =(int64_t)(internal_bias << 4) + (int64_t)(internal_bias);
#ifdef VERIFICATION
    DEBUG_PRINT(2, "HMX REDUNDNAT ACC COMBINE MULT: hi=%llx lo=%llx bias=%llx" , (long long int )hi64, (long long int )lo64, (long long int )bias64);
#endif
    return hi64 + lo64 - bias64;
}


void ARCH_FUNCTION(hmx_mac_pmu)(hmx_state_t * state_ptr, const uint32_t is_flt, uint32_t current_block)
{
    if (!state_ptr->thread->timing_on || state_ptr->thread->bq_on)
        return;

    int hmx_power_on =  (state_ptr->proc->arch_proc_options->hmx_power_config == 0) ? (!state_ptr->thread->bq_on) : 1;
	if (state_ptr->proc->arch_proc_options->pmu_enable && hmx_power_on)
	{
		processor_t * proc __attribute__((unused)) = state_ptr->proc;
		const uint32_t mac_tile_size = is_flt ? state_ptr->QDSP6_MX_FP_RATE : state_ptr->QDSP6_MX_RATE;
        const uint32_t mac_pmu_counter = 2048;	// This is calculation based on the total number of macs and the rate
        const uint32_t max_cycles = 32/mac_tile_size;
        const uint32_t mac_pmu_counter_mask = mac_pmu_counter-1;
        uint32_t array_stat __attribute__((unused)) = (is_flt) ? phmx_array_flt_mpy0 : phmx_array_fxp_mpy0;
        uint32_t acc_stat __attribute__((unused)) = (is_flt) ? phmx_array_flt_acc : phmx_array_fxp_acc;
        uint32_t shift = 0;  // rate == 1
        shift += (mac_tile_size >= 2); // rate == 2
        shift += (mac_tile_size >= 4); // rate == 4
        shift += (mac_tile_size >= 8); // rate == 8
	    shift += (mac_tile_size >= 16);// rate == 16
        shift += (mac_tile_size >= 32);// rate == 32

        for(uint32_t in_idx = 0; in_idx < 32; in_idx += mac_tile_size)
        {
            for(uint32_t row_idx = 0; row_idx < 64; row_idx++)
            {
                for(uint32_t col_idx = 0; col_idx < 32; col_idx++)
                {
                    uint32_t active_acc = 0;
                    for(uint32_t tile_idx = 0; tile_idx < mac_tile_size; tile_idx++)
                    {
                        // Active Multipliers
                        // We will need to fix this for runs of PMU events
                        uint8_t current  = state_ptr->mpy_matrix[row_idx][col_idx][in_idx+tile_idx];		// For FP16, every other row should be zero, so it's ok we're just going to go through the zeros
                        uint8_t inc = current > 0;
                        uint8_t previous = state_ptr->mpy_matrix_pre[row_idx][col_idx][tile_idx];
                        inc |= (previous > 0);
                        state_ptr->mpy_matrix_pre[row_idx][col_idx][tile_idx] = current;
                        state_ptr->array_mpy[tile_idx] += inc;
                        active_acc |= inc;
                        INC_PSTAT(((inc) ? phmx_array_raw_nonzero : phmx_array_raw_zero));
                    }
                    state_ptr->array_acc += active_acc;
                }
            }

            uint8_t pmu_valids = 0; // For Timing model and power throttling
            uint32_t mac_increment = 0;
            for(uint32_t tile_idx = 0; tile_idx < mac_tile_size; tile_idx++)
            {
                if (state_ptr->array_mpy[tile_idx] >= mac_pmu_counter)
                {
                    mac_increment++;
                    state_ptr->array_mpy[tile_idx] &= mac_pmu_counter_mask;
                    pmu_valids |= (1<<tile_idx);
                }
            }
            INC_PSTATN(array_stat, mac_increment);
            if (state_ptr->array_acc >= mac_pmu_counter) {
                INC_PSTAT(acc_stat);
                state_ptr->array_acc &= mac_pmu_counter_mask;
                //pmu_valids |= 0x16;	//TODO: Fix me
            }
            int cycle_idx = (in_idx >> shift) + current_block*max_cycles;
            if (cycle_idx > 511) {
                cycle_idx = 511;
            }
            state_ptr->thread->mem_access[1].cdata[cycle_idx] = pmu_valids;
            //printf("block: %d pmu[%d] = %x\n", current_block, cycle_idx, pmu_valids);
        }
    }
}

// Load upto two blocks of activations
void
ARCH_FUNCTION(hmx_ld_act)(thread_t *env, hmx_state_t * state_ptr, const paddr_t paddr, const uint32_t block_idx)
{
	const paddr_t block_size = state_ptr->QDSP6_MX_COLS * state_ptr->QDSP6_MX_ROWS;
	const paddr_t act_pa_base0 = paddr + block_idx * block_size;
	const paddr_t act_pa_base1 = paddr + state_ptr->dY;
    const uint32_t second_block = (state_ptr->blocks == 1) && ((state_ptr->y_tap > 1) || (state_ptr->y_start != 0));

    for (uint32_t act_pa_offset = 0, cache_idx = 0; act_pa_offset < block_size; act_pa_offset+=8, cache_idx++)
	{
		state_ptr->act_cache_dw[cache_idx] = sim_mem_read8(state_ptr->system, state_ptr->threadId, act_pa_base0 + act_pa_offset);		// would be nice to have a sim_mem_read128
#ifdef VERIFICATION
        state_ptr->act_cache_dw_accessed[cache_idx] = 0;  // Clear read flag, mainly used for verif, wrap in some MACRO?
#endif
    }

    for (uint32_t act_pa_offset = 0, cache_idx = (block_size>>3); act_pa_offset < block_size*second_block; act_pa_offset+=8, cache_idx++)
	{
        state_ptr->act_cache_dw[cache_idx] = sim_mem_read8(state_ptr->system, state_ptr->threadId, act_pa_base1 + act_pa_offset);
#ifdef VERIFICATION
        state_ptr->act_cache_dw_accessed[cache_idx] = 0;  // Clear read flag, mainly used for verif, wrap in some MACRO?
#endif
    }
}



/////// VERIFICATION CALLBACKS
void ARCH_FUNCTION(hmx_8x8_verif_callback)(hmx_state_t * state_ptr,  uint32_t spatial_idx,  uint32_t output_idx,  uint32_t acc_idx,  uint16_t result, hmx_cvt_rs_reg_t rs);
void ARCH_FUNCTION(hmx_8x8_verif_callback)(hmx_state_t * state_ptr,  uint32_t spatial_idx,  uint32_t output_idx,  uint32_t acc_idx,  uint16_t result, hmx_cvt_rs_reg_t rs)
{
    if (state_ptr->proc->options->hmx_cvt_state_transfer_callback) {
        uint32_t pktid = state_ptr->pktid;
        int feedback = (rs.fb_dst != 0);
        const uint32_t cvt_idx = (state_ptr->cvt_accum_current_index + MAX_CONVERT_STATES - feedback) % MAX_CONVERT_STATES;
		state_ptr->proc->options->hmx_cvt_state_transfer_callback(state_ptr->system, state_ptr->proc, pktid, 0, spatial_idx, output_idx, acc_idx, result);
		state_ptr->proc->options->hmx_cvt_state_transfer_callback(state_ptr->system, state_ptr->proc, pktid, 1, spatial_idx, output_idx, acc_idx, state_ptr->cvt_accum[cvt_idx][spatial_idx][output_idx].uh[0] & 0xFF0);
        DEBUG_PRINT(2, "HMX_XFP_CALLBACK: pktid=0x%08x %d [%02d][%02d][%02d]=%x",  pktid, 0, spatial_idx, output_idx, acc_idx, result);
		DEBUG_PRINT(2, "HMX_XFP_CALLBACK: pktid=0x%08x %d [%02d][%02d][%02d]=%x",  pktid, 1, spatial_idx, output_idx, acc_idx, state_ptr->cvt_accum[cvt_idx][spatial_idx][output_idx].uh[0] & 0xFF0);
	}
}

void ARCH_FUNCTION(hmx_16x8_verif_callback)(hmx_state_t * state_ptr,  uint32_t spatial_hi_idx,  uint32_t spatial_lo_idx,  uint32_t output_idx,  uint32_t acc_idx,  uint16_t result_hi,  uint16_t result_lo, hmx_cvt_rs_reg_t rs);
void ARCH_FUNCTION(hmx_16x8_verif_callback)(hmx_state_t * state_ptr,  uint32_t spatial_hi_idx,  uint32_t spatial_lo_idx,  uint32_t output_idx,  uint32_t acc_idx,  uint16_t result_hi,  uint16_t result_lo, hmx_cvt_rs_reg_t rs)
{
    rs.fb_dst = 0; // No feedback in 16x8
    ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_lo_idx, output_idx, acc_idx, result_lo, rs);
    ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_hi_idx, output_idx, acc_idx, result_hi, rs);
}

void ARCH_FUNCTION(hmx_16x16_verif_callback)(hmx_state_t * state_ptr,  uint32_t spatial_hi_idx,  uint32_t spatial_lo_idx,  uint32_t output_hi_idx,  uint32_t output_lo_idx,  uint32_t output_adjust,  uint32_t acc_idx,  uint16_t result_hi,  uint16_t result_lo, hmx_cvt_rs_reg_t rs);
void ARCH_FUNCTION(hmx_16x16_verif_callback)(hmx_state_t * state_ptr,  uint32_t spatial_hi_idx,  uint32_t spatial_lo_idx,  uint32_t output_hi_idx,  uint32_t output_lo_idx,  uint32_t output_adjust,  uint32_t acc_idx,  uint16_t result_hi,  uint16_t result_lo, hmx_cvt_rs_reg_t rs)
{
    hmx_cvt_rs_reg_t rs2 = rs;

    rs2.fb_dst = (rs.fb_dst != 0);    // write unconverted
    ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_lo_idx, output_lo_idx + output_adjust, acc_idx, result_lo, rs );
    ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_hi_idx, output_lo_idx + output_adjust, acc_idx, result_hi, rs );

    uint32_t lo = rs2.fb_dst ? state_ptr->cvt_future_accum_fxp[spatial_lo_idx][output_lo_idx + !output_adjust].uh[0] : 0 ;
    uint32_t hi = rs2.fb_dst ? state_ptr->cvt_future_accum_fxp[spatial_hi_idx][output_lo_idx + !output_adjust].uh[0] : 0 ;

    ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_lo_idx, output_lo_idx + !output_adjust, acc_idx, lo, rs);
    ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_hi_idx, output_lo_idx + !output_adjust, acc_idx, hi, rs);
}

void ARCH_FUNCTION(hmx_wgt_verif_callback)(hmx_state_t * state_ptr, uint32_t testgen_check_in, paddr_t pa, paddr_t pa_end, uint32_t wgt_per_word, uint32_t wgt_raw, uint32_t decomp, uint32_t byte_offset);
void ARCH_FUNCTION(hmx_wgt_verif_callback)(hmx_state_t * state_ptr, uint32_t testgen_check_in, paddr_t pa, paddr_t pa_end, uint32_t wgt_per_word, uint32_t wgt_raw, uint32_t decomp, uint32_t byte_offset)
{
#ifdef VERIFICATION
    uint32_t testgen_check = testgen_check_in ? state_ptr->proc->options->testgen_mode : 1;
    if(pa <= pa_end)
    {
        if (state_ptr->proc->options->sim_vtcm_memory_callback && testgen_check)
        {
            uint32_t slot_tmp = state_ptr->thread->cur_slot;
            state_ptr->thread->cur_slot = 1;
            uint32_t stride = (wgt_per_word==2) ? 2 : 1;
            uint32_t loop_end = (decomp) ? 1 : 4;
            if (decomp)
            {   // wgt decompress,
                pa = (pa & ~0x3ll) + byte_offset;
            }
            uint32_t mask = (wgt_per_word==2) ? 0xFFFF : 0xFF;
            for(int t = 0; t < loop_end; t+=stride)
            {
                uint32_t tmp_wgt = (wgt_raw >> (t*8)) & mask;
                DEBUG_PRINT(2, "CALLBACK WGT LOAD PA=%llx WGT=%02x", (long long int)pa+t, tmp_wgt);
                state_ptr->proc->options->sim_vtcm_memory_callback(state_ptr->system, state_ptr->proc, state_ptr->threadId, 0, pa+t, stride, DREAD, tmp_wgt);

            }
            state_ptr->thread->cur_slot = slot_tmp;
        }
    }
#endif
}

/////////////















// Small Static/inline shared ops
// Assume 12-bit convert
static inline uint16_t hmx_cvt_out_lo(uint32_t in) { return (in >> 0) & 0xFFF; }
static inline uint16_t hmx_cvt_out_hi(uint32_t in) { return (in >> 8) & 0xFF0; }
static inline uint32_t hmx_cvt_combine_feedback(uint16_t hi, uint16_t lo) {
    uint32_t result = (uint32_t)(hi & 0xFF0) << 8;
    return result | ((uint32_t)lo & 0xFFF);
}
static inline uint32_t get_zeroing(hmx_bias_t bias) { return (bias.fxp.negate << 2) | (bias.fxp.zeropos << 1) | bias.fxp.zeroneg; }
static inline uint32_t get_bias(hmx_bias_t bias) { return (((bias.fxp.bias1 << 1) | bias.fxp.rnd_bit) << 3) | bias.fxp.bias0; }
static inline uint32_t get_scale(hmx_bias_t bias) { return (((!bias.fxp.sigmsb << 10) + bias.fxp.sig) << 1) + bias.fxp.siglsb; }
static inline uint32_t poly_feedback_select(const uint32_t is_feedback, const uint32_t limit, const uint32_t current, const uint32_t feedback )
{
    if (!is_feedback)
        return current;

    if(limit){
        return  (current > feedback ) ? current : feedback;
    }
    return  (current > feedback) ?  feedback : current;
}
static inline int32_t hmx_get_internal_bias(hmx_state_t * state_ptr, uint32_t spatial_idx,  uint32_t output_idx,  uint32_t acc_idx) {
    return (state_ptr->accum_fxp[spatial_idx][output_idx].bias_state & (1<<(acc_idx) ) ) ? state_ptr->internal_bias_value : 0;
}


void ARCH_FUNCTION(hmx_8x4_convert_body)(hmx_state_t * state_ptr,  uint32_t spatial_idx,  uint32_t output_idx,  uint32_t acc_idx,  hmx_bias_t bias,  uint32_t subchannel,  uint32_t legacy, hmx_cvt_rs_reg_t rs, void * acc_select_fptr)
{
    // Indices
    const uint32_t acc_lo_idx = acc_idx + 0 + subchannel;
	const int32_t acc_in  = state_ptr->accum_fxp[spatial_idx][output_idx].w[acc_lo_idx];
	const int32_t internal_bias    =  hmx_get_internal_bias(state_ptr, spatial_idx, output_idx, acc_lo_idx ) ;
    hmx_acc_select_ptr_t fptr = (hmx_acc_select_ptr_t)acc_select_fptr;
    const int64_t acc = fptr(state_ptr,  acc_in, internal_bias, internal_bias);
    DEBUG_PRINT(2,       "HMX_CVT8X4_IN: pktid=0x%08x ACC[%02d][%02d][%02d].%s=%08x combined=%09llx internal_bias=%x",  state_ptr->pktid, spatial_idx, output_idx, acc_idx, subchannel ? "hi" : "lo", acc_in, (long long int)acc, internal_bias);

    // Feedback
    const uint32_t feedback = state_ptr->cvt_future_accum_fxp[spatial_idx][output_idx].uh[0];
    const uint32_t poly_bias  =  poly_feedback_select((rs.fb_dst==HMX_CVT_FB_OUTBIAS), rs.fb_limit, get_bias(bias), feedback );
    const uint32_t poly_scale =  poly_feedback_select((rs.fb_dst==HMX_CVT_FB_SCALE), rs.fb_limit, get_scale(bias), feedback );
    const int64_t bias32 = (int64_t)((int32_t)bias.fxp.bias32  & 0xFFFFFFF0);

   	uint32_t result = ARCH_FUNCTION(hmx_u8_cvt)(state_ptr, acc, bias32, bias.fxp.exp, get_zeroing(bias), poly_scale, poly_bias, (rs.relu==0), legacy);
    state_ptr->cvt_future_accum_fxp[spatial_idx][output_idx].uh[0] = result;
    DEBUG_PRINT(2,       "HMX_CVT_OUT: pktid=0x%08x CVT_STATE[%02d][%02d]=%02x", state_ptr->pktid, spatial_idx, output_idx, result );
    VERIF(ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_idx, output_idx, acc_lo_idx, result, rs);)
}

void ARCH_FUNCTION(hmx_8x8_convert_body)(hmx_state_t * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, hmx_bias_t bias, uint32_t subchannel,  uint32_t legacy, hmx_cvt_rs_reg_t rs, void * acc_select_fptr)
{
    // Indices
    const uint32_t acc_lo_idx = acc_idx + 0;
    const uint32_t acc_hi_idx = acc_idx + 2;

    // Accumulators
    hmx_acc_select_ptr_t fptr = (hmx_acc_select_ptr_t)acc_select_fptr;
    int32_t internal_bias = hmx_get_internal_bias(state_ptr, spatial_idx,output_idx, acc_lo_idx ) ;
    const int64_t acc = fptr(state_ptr,  state_ptr->accum_fxp[spatial_idx][output_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_idx][output_idx].w[acc_lo_idx], internal_bias);
    DEBUG_PRINT(2,       "HMX_CVT8X8_IN: pktid=0x%08x ACC[%02d][%02d][%02d].hi=%08x .lo=%08x combined=%09llx internal_bias=%x",  state_ptr->pktid, spatial_idx, output_idx, acc_idx, state_ptr->accum_fxp[spatial_idx][output_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_idx][output_idx].w[acc_lo_idx], (long long int)acc, internal_bias );

    // parameters from bias reg and feedback decision
    const uint16_t feedback = state_ptr->cvt_future_accum_fxp[spatial_idx][output_idx].uh[0];
    const uint32_t poly_bias  =  poly_feedback_select((rs.fb_dst==HMX_CVT_FB_OUTBIAS), rs.fb_limit, get_bias(bias), feedback );
    const uint32_t poly_scale =  poly_feedback_select((rs.fb_dst==HMX_CVT_FB_SCALE), rs.fb_limit, get_scale(bias), feedback );
    const int64_t bias32 = (int64_t)((int32_t)bias.fxp.bias32  & 0xFFFFFFFF);

   	uint16_t result = ARCH_FUNCTION(hmx_u8_cvt)(state_ptr, acc, bias32, bias.fxp.exp, get_zeroing(bias), poly_scale, poly_bias, (rs.relu==0), legacy);

    state_ptr->cvt_future_accum_fxp[spatial_idx][output_idx].uh[0] = result;
    DEBUG_PRINT(2,       "HMX_CVT_OUT: pktid=0x%08x CVT_STATE[%02d][%02d]=%02x",  state_ptr->pktid, spatial_idx, output_idx, result );
    VERIF(ARCH_FUNCTION(hmx_8x8_verif_callback)(state_ptr, spatial_idx, output_idx, acc_lo_idx, result, rs);)
}

void ARCH_FUNCTION(hmx_16x8_convert_body)(hmx_state_t * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, hmx_bias_t bias, uint32_t subchannel, uint32_t legacy, hmx_cvt_rs_reg_t rs, void * acc_select_fptr)
{
    // Indices
    const uint32_t acc_lo_idx = acc_idx + 0;
    const uint32_t acc_hi_idx = acc_idx + 2;
    const uint32_t spatial_lo_idx = spatial_idx + 0;
    const uint32_t spatial_hi_idx = spatial_idx + 1;



    // Accumulators
    hmx_acc_select_ptr_t fptr = (hmx_acc_select_ptr_t)acc_select_fptr;

    int32_t internal_bias = hmx_get_internal_bias(state_ptr, spatial_lo_idx,output_idx, acc_lo_idx ) ;
    const int64_t acc_ll = fptr(state_ptr,  state_ptr->accum_fxp[spatial_lo_idx][output_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_lo_idx][output_idx].w[acc_lo_idx], internal_bias);

    internal_bias = hmx_get_internal_bias(state_ptr, spatial_hi_idx,output_idx, acc_lo_idx ) ;
    const int64_t acc_hl = fptr(state_ptr,  state_ptr->accum_fxp[spatial_hi_idx][output_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_hi_idx][output_idx].w[acc_lo_idx], internal_bias);
    DEBUG_PRINT(2,       "HMX_CVT16X8_IN: pktid=0x%08x ACC_LL[%02d][%02d][%02d].hi=%08x .lo=%08x combined=%09llx",  state_ptr->pktid, spatial_lo_idx, output_idx, acc_idx, state_ptr->accum_fxp[spatial_lo_idx][output_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_lo_idx][output_idx].w[acc_lo_idx], (long long int)acc_ll );
    DEBUG_PRINT(2,       "HMX_CVT16X8_IN: pktid=0x%08x ACC_HL[%02d][%02d][%02d].hi=%08x .lo=%08x combined=%09llx",  state_ptr->pktid, spatial_hi_idx, output_idx, acc_idx, state_ptr->accum_fxp[spatial_hi_idx][output_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_hi_idx][output_idx].w[acc_lo_idx], (long long int)acc_hl );

    // parameters from bias reg and feedback decision
    const uint16_t poly_bias  =  bias.fxp.rnd_bit;
    const uint32_t poly_scale =  get_scale(bias);
    const int64_t bias32 =  (int64_t)bias.fxp.bias32;

    const uint32_t result = ARCH_FUNCTION(hmx_u16_cvt)(state_ptr, acc_hl, acc_ll, bias32, bias.fxp.exp,  get_zeroing(bias), poly_scale, poly_bias, (rs.relu==0), legacy) >> 4; // outputs 24-bits, take only top 20 for next step;
    const uint16_t result_lo = hmx_cvt_out_lo(result);
    const uint16_t result_hi = hmx_cvt_out_hi(result);

    state_ptr->cvt_future_accum_fxp[spatial_lo_idx][output_idx].uh[0] = result_lo;
    state_ptr->cvt_future_accum_fxp[spatial_hi_idx][output_idx].uh[0] = result_hi;
    DEBUG_PRINT(2,       "HMX_CVT_OUT: pktid=0x%08x CVT_STATE[%02d][%02d]=%02x CVT_STATE[%02d][%02d]=%02x HALF OUT=%04x",  state_ptr->pktid, spatial_lo_idx, output_idx,  result_lo, spatial_hi_idx, output_idx, result_hi, result  );
    VERIF(ARCH_FUNCTION(hmx_16x8_verif_callback)(state_ptr, spatial_hi_idx, spatial_lo_idx, output_idx, acc_idx, result_hi, result_lo, rs);)
}



void ARCH_FUNCTION(hmx_16x16_convert_body)(hmx_state_t * state_ptr, uint32_t spatial_idx, uint32_t output_idx, uint32_t acc_idx, hmx_bias_t bias, uint32_t subchannel, uint32_t legacy, hmx_cvt_rs_reg_t rs, void * acc_select_fptr)
{
    // Indices
    const uint32_t acc_lo_idx = acc_idx + 0;
    const uint32_t acc_hi_idx = acc_idx + 2;
    const uint32_t spatial_lo_idx = spatial_idx + 0;
    const uint32_t spatial_hi_idx = spatial_idx + 1;
    const uint32_t output_lo_idx = output_idx + 0;
    const uint32_t output_hi_idx = output_idx + 1;



    // Accumulators
    hmx_acc_select_ptr_t fptr = (hmx_acc_select_ptr_t)acc_select_fptr;
    int32_t internal_bias = hmx_get_internal_bias(state_ptr, spatial_lo_idx,output_lo_idx, acc_lo_idx ) ;
    int64_t acc_ll = fptr(state_ptr,  state_ptr->accum_fxp[spatial_lo_idx][output_lo_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_lo_idx][output_lo_idx].w[acc_lo_idx], internal_bias);

    internal_bias = hmx_get_internal_bias(state_ptr, spatial_hi_idx,output_lo_idx, acc_lo_idx ) ;
    int64_t acc_hl = fptr(state_ptr,  state_ptr->accum_fxp[spatial_hi_idx][output_lo_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_hi_idx][output_lo_idx].w[acc_lo_idx], internal_bias);

    internal_bias = hmx_get_internal_bias(state_ptr, spatial_lo_idx,output_hi_idx, acc_lo_idx ) ;
    int64_t acc_lh = fptr(state_ptr,  state_ptr->accum_fxp[spatial_lo_idx][output_hi_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_lo_idx][output_hi_idx].w[acc_lo_idx], internal_bias);

    internal_bias = hmx_get_internal_bias(state_ptr, spatial_hi_idx,output_hi_idx, acc_lo_idx ) ;
    int64_t acc_hh = fptr(state_ptr,  state_ptr->accum_fxp[spatial_hi_idx][output_hi_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_hi_idx][output_hi_idx].w[acc_lo_idx], internal_bias);

    const uint32_t output_adjust = ((rs.fxp16_ch_sel == 0x2) && !legacy) ? 0 : 1;

    if((rs.fxp16_ch_sel != 0x3) && (!legacy)){
        if(rs.fxp16_ch_sel == 0x2){
        acc_lh = acc_ll;
        acc_hh = acc_hl;
    }
    acc_ll = 0;
    acc_hl = 0;
    }

    DEBUG_PRINT(2,       "HMX_CVT16X16_IN: pktid=0x%08x ACC_LL[%02d][%02d][%02d].hi=%08x .lo=%08x combined=%09llx",  state_ptr->pktid, spatial_lo_idx, output_lo_idx, acc_lo_idx, state_ptr->accum_fxp[spatial_lo_idx][output_lo_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_lo_idx][output_lo_idx].w[acc_lo_idx], (long long int)acc_ll );
    DEBUG_PRINT(2,       "HMX_CVT16X16_IN: pktid=0x%08x ACC_HL[%02d][%02d][%02d].hi=%08x .lo=%08x combined=%09llx",  state_ptr->pktid, spatial_hi_idx, output_lo_idx, acc_lo_idx, state_ptr->accum_fxp[spatial_hi_idx][output_lo_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_hi_idx][output_lo_idx].w[acc_lo_idx], (long long int)acc_hl );
    DEBUG_PRINT(2,       "HMX_CVT16X16_IN: pktid=0x%08x ACC_LL[%02d][%02d][%02d].hi=%08x .lo=%08x combined=%09llx",  state_ptr->pktid, spatial_lo_idx, output_hi_idx, acc_lo_idx, state_ptr->accum_fxp[spatial_lo_idx][output_hi_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_lo_idx][output_hi_idx].w[acc_lo_idx], (long long int)acc_lh );
    DEBUG_PRINT(2,       "HMX_CVT16X16_IN: pktid=0x%08x ACC_HL[%02d][%02d][%02d].hi=%08x .lo=%08x combined=%09llx",  state_ptr->pktid, spatial_hi_idx, output_hi_idx, acc_lo_idx, state_ptr->accum_fxp[spatial_hi_idx][output_hi_idx].w[acc_hi_idx], state_ptr->accum_fxp[spatial_hi_idx][output_hi_idx].w[acc_lo_idx], (long long int)acc_hh );


    // parameters from bias reg and feedback decision
    const hmx_bias_t bias_reg2 = state_ptr->bias[rs.bias_sel][output_hi_idx];    // Need second bias reg
    const uint32_t output_bias = ((bias_reg2.fxp.bias1 << 12) + get_bias(bias)) << 2;
    const uint32_t scale = (((((!bias_reg2.fxp.sigmsb << 10) + bias_reg2.fxp.sig) << 10) + bias.fxp.sig) << 1) + bias.fxp.siglsb;
    const uint32_t feedback = hmx_cvt_combine_feedback(state_ptr->cvt_future_accum_fxp[spatial_hi_idx][output_idx+output_adjust].uh[0], state_ptr->cvt_future_accum_fxp[spatial_lo_idx][output_idx+output_adjust].uh[0]) << 2;
    const uint32_t poly_bias  =  poly_feedback_select((rs.fb_dst==HMX_CVT_FB_OUTBIAS), rs.fb_limit, output_bias, feedback ) >> 2;
    const uint32_t poly_scale =  poly_feedback_select((rs.fb_dst==HMX_CVT_FB_SCALE), rs.fb_limit, scale, feedback );
    const int64_t bias48 = (int64_t)bias.fxp.bias32 << 16;

    const uint32_t result =  ARCH_FUNCTION(hmx_u16x16_cvt)(state_ptr, acc_hh, acc_hl, acc_lh, acc_ll, bias48, bias.fxp.exp, get_zeroing(bias), poly_scale, poly_bias, (rs.relu==0), legacy) >> 4; // outputs 24-bits, take only top 20 for next step
    const uint16_t result_lo = hmx_cvt_out_lo(result);
    const uint16_t result_hi = hmx_cvt_out_hi(result);


    state_ptr->cvt_future_accum_fxp[spatial_lo_idx][output_lo_idx + output_adjust].uh[0] = result_lo;
    state_ptr->cvt_future_accum_fxp[spatial_hi_idx][output_lo_idx + output_adjust].uh[0] = result_hi;
    if(!rs.fb_dst){
        state_ptr->cvt_future_accum_fxp[spatial_lo_idx][output_lo_idx + !output_adjust].uh[0] = 0;
        state_ptr->cvt_future_accum_fxp[spatial_hi_idx][output_lo_idx + !output_adjust].uh[0] = 0;
    }
    DEBUG_PRINT(2,       "HMX_CVT_OUT: pktid=0x%08x CVT_STATE[%02d][%02d]=%02x CVT_STATE[%02d][%02d]=%02x HALF OUT=%04x", state_ptr->pktid, spatial_lo_idx, output_lo_idx + output_adjust,  result_lo, spatial_hi_idx, output_lo_idx + output_adjust, result_hi, result );
    VERIF(ARCH_FUNCTION(hmx_16x16_verif_callback)(state_ptr, spatial_hi_idx, spatial_lo_idx, output_hi_idx, output_lo_idx, output_adjust, acc_lo_idx, result_hi, result_lo, rs);)
}

static inline int8_t hmx_sign_extend_to_byte(int8_t in, int32_t bit_select, int32_t mask, int32_t shift) {
    int8_t out = (in >> bit_select) & mask;
    out <<= shift;
    out >>= shift;
    return out;
}

int8_t ARCH_FUNCTION(hmx_unpack_none)(int8_t in, int32_t bit_select){
    return in;
}
int8_t ARCH_FUNCTION(hmx_unpack_byte_from_byte)(int8_t in, int32_t bit_select) {
    return in;
}
int8_t ARCH_FUNCTION(hmx_unpack_sm_from_byte)(int8_t in, int32_t bit_select) {
    return (((in >> 7)) & 0x7F) ^ in;
}
int8_t ARCH_FUNCTION(hmx_unpack_1sbit_from_byte)(int8_t in, int32_t bit_select) {
	return ((in >> bit_select) & 0x1) ? -1 : 1;
}
int8_t ARCH_FUNCTION(hmx_unpack_1bit_from_byte)(int8_t in, int32_t bit_select) {
    return (in >> bit_select) & 0x1;
}
int8_t ARCH_FUNCTION(hmx_unpack_nibble_from_byte)(int8_t in, int32_t bit_select) {
    return hmx_sign_extend_to_byte(in, bit_select , 0xF, 4);
}
int8_t ARCH_FUNCTION(hmx_unpack_crumb_from_byte)( int8_t in, int32_t bit_select) {
    return hmx_sign_extend_to_byte(in, bit_select, 0x3, 6);
}
int8_t ARCH_FUNCTION(hmx_unpack_scrumb_from_byte)(int8_t in, int32_t bit_select) {
    int8_t out = hmx_sign_extend_to_byte(in, bit_select, 0x3, 6);
    return (out >= 0) ? 2-out : out;
}
#ifndef DBG_TRACING

hmx_mult_body_ptr_t hmx_mult_body_ptr_table[HMX_MULT_TYPES] = {
    0, /* directly handle hmx_mult_fxp */
    0, /* directly handle hmx_mult_fxp_subbyte */
	&hmx_mult_xfp,
};

hmx_unpack_ptr_t hmx_unpack_ptr_table[HMX_UNPACK_TYPES] = {
	&hmx_unpack_byte_from_byte,
    &hmx_unpack_sm_from_byte,
	&hmx_unpack_nibble_from_byte,
	&hmx_unpack_crumb_from_byte,
	&hmx_unpack_scrumb_from_byte,
	&hmx_unpack_1bit_from_byte,
    &hmx_unpack_1sbit_from_byte,
    &hmx_unpack_none,
};
hmx_cvt_body_ptr_t hmx_cvt_body_ptr_table[HMX_OP_TYPES] = {
	&hmx_8x8_convert_body,             // HMX_UB
    &hmx_8x8_convert_body,              // HMX_B
	&hmx_8x4_convert_body,              // HMX_UB4
	&hmx_16x8_convert_body,              // HMX_UH
	&hmx_xfp_convert_body,              // HMX_FP16
	&hmx_16x16_convert_body,            // HMX_UH_UH
};

uint8_t hmx_unpack_bits_table[HMX_UNPACK_TYPES] = {
	8,
    8,
	4,
	2,
	2,
	1,
    1,
    0,
};

uint32_t output_stride_table[HMX_OP_TYPES] = {
	1,             // HMX_UB.
    1,             // HMX_B.
	1,              // HMX_UB4
	1,              // HMX_UH
	1,              // HMX_FP16
	2,            // HMX_UH_UH
};

uint32_t spatial_stride_table[HMX_OP_TYPES] = {
	1,             // HMX_UB.
    1,             // HMX_B.
	1,              // HMX_UB4
	2,              // HMX_UH
	2,              // HMX_FP16
	2,            // HMX_UH_UH
};
#else

extern hmx_mult_body_ptr_t hmx_mult_body_ptr_table[HMX_MULT_TYPES];
extern hmx_unpack_ptr_t hmx_unpack_ptr_table[HMX_UNPACK_TYPES];
extern hmx_cvt_body_ptr_t hmx_cvt_body_ptr_table[HMX_OP_TYPES];
extern uint8_t hmx_unpack_bits_table[HMX_UNPACK_TYPES];
extern uint32_t output_stride_table[HMX_OP_TYPES];
extern uint32_t spatial_stride_table[HMX_OP_TYPES];

#endif

void ARCH_FUNCTION(hmx_acc_convert)(hmx_state_t * state_ptr, hmx_cvt_rs_reg_t cvt_rs, uint32_t type,  uint32_t subchannel,  uint32_t legacy)
{
    const uint32_t spatial_size = state_ptr->QDSP6_MX_ROWS;
	const uint32_t output_depth = state_ptr->QDSP6_MX_COLS;
    const uint32_t output_stride = output_stride_table[type];
    const uint32_t spatial_stride = spatial_stride_table[type];
    const uint32_t acc_idx = state_ptr->acc_select;

    const hmx_cvt_body_ptr_t convert_body_fptr = hmx_cvt_body_ptr_table[type];
    void * acc_select_fptr = (void*)((state_ptr->redundant_acc) ? &hmx_combine_redundant_acc : &hmx_non_redundant_acc);

    for(uint32_t output_idx = 0; output_idx < output_depth; output_idx+=output_stride)
    {
        const hmx_bias_t bias = state_ptr->bias[cvt_rs.bias_sel][output_idx];
		for(uint32_t spatial_idx = 0; spatial_idx < spatial_size; spatial_idx+=spatial_stride)
        {
            convert_body_fptr(state_ptr, spatial_idx, output_idx, acc_idx, bias, subchannel, legacy, cvt_rs, acc_select_fptr);
		}
	}
    if (state_ptr->is_flt) {
        state_ptr->flt_commit_state.acc_clear = !cvt_rs.acc_clear;
    } else {
        state_ptr->fxp_commit_state.acc_clear = !cvt_rs.acc_clear;
    }
}

void ARCH_FUNCTION(hmx_act_ld_verif_callback)(hmx_state_t * state_ptr, uint16_t act, uint32_t cache_idx, uint32_t flt, uint32_t block_idx);
void ARCH_FUNCTION(hmx_act_ld_verif_callback)(hmx_state_t * state_ptr, uint16_t act, uint32_t cache_idx, uint32_t flt, uint32_t block_idx)
{
#ifdef VERIFICATION
    if (state_ptr->proc->options->sim_vtcm_memory_callback)
    {

        int32_t accessed  = (flt) ? state_ptr->act_cache_uh_accessed[cache_idx>>1] : state_ptr->act_cache_ub_accessed[cache_idx];
        if (!accessed)
        {
            int slot_tmp = state_ptr->thread->cur_slot;
            int stride = (flt) ? 2 : 1;
            state_ptr->thread->cur_slot = 1;

            paddr_t pa = state_ptr->act_addr + (cache_idx & 2047);
            if (block_idx) {
                pa += 2048*block_idx;
            } else {
                pa += (cache_idx >= 2048) * state_ptr->dY;	// Has verical block
            }
            DEBUG_PRINT(2, "CALLBACK ACT LOAD PA=%llx ACT=%02x cache_idx=%d base_pa=%llx dy=%x", (long long int)pa, act, cache_idx, state_ptr->act_addr, state_ptr->dY);
            state_ptr->proc->options->sim_vtcm_memory_callback(state_ptr->system, state_ptr->proc, state_ptr->threadId, 1, pa, stride, DREAD, act);


            state_ptr->thread->cur_slot = slot_tmp;
            if (flt) {
                state_ptr->act_cache_uh_accessed[cache_idx>>1] = 1;
            } else {
                state_ptr->act_cache_ub_accessed[cache_idx] = 1;
            }
        }
    }
#endif
}

int32_t
ARCH_FUNCTION(hmx_inc_with_spatial_mask)(int32_t in, int32_t inc, int32_t mask)
{
    return (((in | ~mask) + (inc & mask)) & mask) | (in & ~mask);
}

int32_t
ARCH_FUNCTION(hmx_inc_with_spatial_mask_ovf)(int32_t in, int32_t inc, int32_t mask, int32_t * overflow)
{
    int32_t out = ARCH_FUNCTION(hmx_inc_with_spatial_mask)(in, inc, mask);
	*overflow = out < in;
    return out;
}

int32_t
ARCH_FUNCTION(hmx_inc_tap_with_spatial_mask_dilate)(int32_t in, int32_t inc, int32_t mask, int32_t dilate)
{
    int32_t out = ARCH_FUNCTION(hmx_inc_with_spatial_mask)(in, inc, mask);
    if (inc == 0) {
        out  = -1;
    } else if (dilate && (out >= 0)) {
        out = ARCH_FUNCTION(hmx_inc_with_spatial_mask)(out, inc, mask);
    }
    return out;
}

int32_t
ARCH_FUNCTION(hmx_compute_filter_size)(int32_t size, int32_t start, int32_t mask,  int32_t inc)
{

    int32_t tmp = start & mask;
	int32_t done = 0;
	int32_t count = 0;
	while(!done && tmp) {
        count = ARCH_FUNCTION(hmx_inc_with_spatial_mask_ovf)(count, inc, mask, &done);
		done =  (tmp == count) ? 1 : done;
		size++;
	}
	size++;
    return size;
}

int32_t
ARCH_FUNCTION(hmx_compute_filter_size_above)(int32_t size, int32_t start, int32_t mask,  int32_t inc)
{
	int32_t done = 0;
	int32_t count = start & mask;
	while(!done) {
        count = ARCH_FUNCTION(hmx_inc_with_spatial_mask_ovf)(count, inc, mask, &done);
        if (!done)
            size++;
	}
	size++;
    return size;
}

static inline int32_t
ARCH_FUNCTION(hmx_wgt_range_check)(hmx_state_t * state_ptr, int32_t stream_idx, int32_t in, int32_t exceeded_val)
{
    if (!state_ptr->wgt_cache[stream_idx][0].valid) {
        state_ptr->limit_execeeded = 1;
        return exceeded_val;
    }
    return in;
}

static inline int compute_indices(int start_value, int stop_value, int increment, int msb, int dilation, int * array_pointer){
    int counter = 0;
    int inner_value = start_value;
    while(inner_value >= 0){
		array_pointer[counter] = inner_value;
		inner_value = ARCH_FUNCTION(hmx_inc_tap_with_spatial_mask_dilate)(inner_value, increment, msb, dilation);
        if(inner_value > stop_value) inner_value = -1;
        counter++;
	}
    return counter;
}

void ARCH_FUNCTION(hmx_multiply)(thread_t *env, hmx_state_t * state_ptr, uint32_t weights_per_byte_log, uint32_t wgt_per_word, uint32_t unpack, uint32_t type, uint32_t mult_type, uint32_t output_channel_scale)
{
    int32_t tile_y_mask = state_ptr->tile_y_mask;
	int32_t tile_x_mask = state_ptr->tile_x_mask;
	int32_t tile_x_mask_msb = tile_x_mask | (1<<31);
	int32_t tile_y_mask_msb = tile_y_mask | (1<<31);
	int32_t tile_y_inc = state_ptr->tile_y_inc;
	int32_t tile_x_inc = state_ptr->tile_x_inc;
	int32_t y_dilate = state_ptr->y_dilate;
	int32_t x_dilate = state_ptr->x_dilate;
	int32_t format = state_ptr->format;
	int32_t group_size = state_ptr->group_size;
	int32_t group_count = state_ptr->group_count;
	int32_t parallel_group_size = state_ptr->QDSP6_MX_COLS / state_ptr->QDSP6_MX_PARALLEL_GRPS;
	int32_t input_ch_end_last_block = state_ptr->ch_stop << format;
	int32_t input_ch_start_first_block = state_ptr->ch_start << format;
	int32_t input_ch_stride = 1 << format; /* stride by number of groups*/
	int32_t input_channels = (state_ptr->QDSP6_MX_COLS << format) / group_count;
	int32_t block_end = state_ptr->blocks;
	state_ptr->weight_count = weights_per_byte_log;
	int32_t deep_mode = state_ptr->deep;
	int32_t flt_mode = type==HMX_FP16;
	int32_t deep_block_end = deep_mode ? 2 : 1 ;
	int32_t current_acc = flt_mode ? state_ptr->current_acc_flt : state_ptr->current_acc_fxp;
	int32_t wgt_stream_idx = 0;
    int x_tap_array[MAX_ACCUMULATORS_SPATIAL];
    int y_tap_array[MAX_ACCUMULATORS_SPATIAL];
    int intra_tile_x_array[MAX_ACCUMULATORS_SPATIAL];
    int intra_tile_y_array[MAX_ACCUMULATORS_SPATIAL];
	int x_tap_idx = state_ptr->x_start;
	int y_tap_idx = state_ptr->y_start;
    int intra_tile_x = 0;
    int intra_tile_y = 0;
    int x_tap_count = compute_indices(x_tap_idx, state_ptr->x_stop, tile_x_inc, tile_x_mask_msb, x_dilate, x_tap_array);
    int y_tap_count = compute_indices(y_tap_idx, state_ptr->y_stop, tile_y_inc, tile_y_mask_msb, y_dilate, y_tap_array);
    int x_count = compute_indices(intra_tile_x, 0x7FFFFFFF, tile_x_inc, tile_x_mask_msb, 0, intra_tile_x_array);
    int y_count = compute_indices(intra_tile_y, 0x7FFFFFFF, tile_y_inc, tile_y_mask_msb, 0, intra_tile_y_array);

	paddr_t wgt_addr = state_ptr->wgt_addr;
	paddr_t wgt_len = state_ptr->max_weight_pa;
    int32_t mx_fp_rate = state_ptr->QDSP6_MX_FP_RATE;
    int32_t input_ch_fp_rate_stride = 4 * input_ch_stride;
    if((mx_fp_rate == 2) && (group_size <= parallel_group_size/2) && flt_mode) {
        input_ch_fp_rate_stride = mx_fp_rate * input_ch_stride;
    } else if ( (mx_fp_rate == 8) && flt_mode) {
		input_ch_fp_rate_stride = mx_fp_rate * input_ch_stride;
    }

    ARCH_FUNCTION(hmx_ld_wgt)(env, state_ptr, wgt_addr, wgt_len,  wgt_per_word, output_channel_scale, flt_mode, unpack );

	for(int32_t block_idx = 0; block_idx < block_end; block_idx++)
    {
		int32_t input_ch_end = (((block_idx + 1) >= block_end)) ? input_ch_end_last_block : input_channels;
		int32_t input_ch_start = (block_idx == 0) ? input_ch_start_first_block : 0;
		if ((mx_fp_rate == 8) && flt_mode) { input_ch_start &= (0xfff8 << format); input_ch_end = ((input_ch_end & 0x1f) > 0) ? ((input_ch_end + 32) & (0xfff8 << format)) : (input_ch_end & (0xfff8 << format));}

        DEBUG_PRINT(2, "MX MULT: Block %d Channel Range: [%d, %d] Deep Block Count=%d, fp_rate=%d flt_mode=%d", block_idx, input_ch_start >> format, input_ch_end >> format, deep_block_end, mx_fp_rate, flt_mode);

		ARCH_FUNCTION(hmx_ld_act)(env, state_ptr,  state_ptr->act_addr, block_idx);

        for(int32_t y_tap_decoded = 0; y_tap_decoded < y_tap_count; y_tap_decoded++)
        {
            y_tap_idx = y_tap_array[y_tap_decoded];
			for(int32_t deep_block_idx = 0; deep_block_idx < deep_block_end; deep_block_idx++)
            {
                for(int32_t x_tap_decoded = 0; x_tap_decoded < x_tap_count; x_tap_decoded++)
                {
                    x_tap_idx = x_tap_array[x_tap_decoded];
					for(int32_t input_ch_idx = input_ch_start; input_ch_idx < input_ch_end; input_ch_idx += input_ch_fp_rate_stride)
                    {
						int32_t wgt_stream_group_idx = wgt_stream_idx; /* save wgt stream pointer for grouped convolution*/
						int32_t group_idx_stride = 1;
						for(int32_t group_idx = 0; group_idx < group_count; group_idx += group_idx_stride)
                        { /* process all groups */
							DEBUG_PRINT(2, "MX GROUP MULT: saved wgt_stream_group_idx=%d, wgt_stream_idx=%d", wgt_stream_group_idx, wgt_stream_idx);
							wgt_stream_idx = wgt_stream_group_idx; /* restore wgt stream pointer, reset per group */
							int32_t input_ch_start_group =  input_ch_idx + (group_idx << format) *group_size; /* starting channel of group */
							int32_t input_ch_stop_group =  input_ch_start_group + 4*input_ch_stride; /* stop at next group */
							if ((mx_fp_rate == 2) && (group_size <= parallel_group_size/2) && flt_mode) {
                                input_ch_stop_group = input_ch_start_group + mx_fp_rate*input_ch_stride;
                            } else if ((mx_fp_rate == 8) && (group_size > 4) && flt_mode) {
								input_ch_stop_group = input_ch_start_group + mx_fp_rate*input_ch_stride;
							} else if ((mx_fp_rate == 8) && (group_size <= 4) && flt_mode) {
								input_ch_stop_group = (group_idx << format) *group_size + (((group_size << format) < input_ch_end) ? (group_size << format) : input_ch_end);
								//wgt_stream_idx = wgt_stream_group_idx + group_idx % ( 8 / group_size);
                            }
                            if(group_size < 4) {
                                input_ch_stop_group = (group_idx << format) *group_size + (((group_size << format) < input_ch_end) ? (group_size << format) : input_ch_end) ;
                            }
							for(int32_t input_ch_idx2 = input_ch_start_group; input_ch_idx2 < input_ch_stop_group; input_ch_idx2 += input_ch_stride)
                            {
								int32_t input_ch_idx_raw = (input_ch_idx2 >> format);
								uint32_t fp8_ch_start = input_ch_start >> format;
								uint32_t fp8_ch_stop = ((((block_idx + 1) >= block_end)) ? input_ch_end_last_block : input_ch_end ) >> format;
                                DEBUG_PRINT(2, "MX MULT: x_tap=%d y_tap=%d input_channel=%d deep_block=%d, group_idx=%d, input_ch_idx=%d, input_ch_idx2=%d, stop_ch=%d", x_tap_decoded, y_tap_decoded, input_ch_idx_raw,deep_block_idx, group_idx, input_ch_idx, input_ch_idx2, input_ch_stop_group);
                                for(int32_t intra_y_counter = 0; intra_y_counter < y_count; intra_y_counter++)
                                {
                                    intra_tile_y = intra_tile_y_array[intra_y_counter];
                                    int32_t y_next_tile = 0;
                                    int32_t act_y_offset = ARCH_FUNCTION(hmx_inc_with_spatial_mask_ovf)(y_tap_idx, intra_tile_y, tile_y_mask, &y_next_tile);

                                    //DEBUG_PRINT(2, "MX MULT: y_next_tile =%x act_y_offset=%x y_tap_idx=%x intra_tile_y=%x tile_y_mask=%x", y_next_tile, act_y_offset, y_tap_idx, intra_tile_y, tile_y_mask);
                                    act_y_offset = (act_y_offset & 0x7FFFFFFF) + (y_next_tile * 0x800); // Clean negative overflow bit
                                    //DEBUG_PRINT(2, "MX MULT: y_next_tile =%x act_y_offset=%x y_tap_idx=%x intra_tile_y=%x tile_y_mask=%x", y_next_tile, act_y_offset, y_tap_idx, intra_tile_y, tile_y_mask);
                                    for(int32_t intra_x_counter = 0; intra_x_counter < x_count; intra_x_counter++)
                                    {
                                        intra_tile_x = intra_tile_x_array[intra_x_counter];
                                        int32_t overflow = 0;
                                        int32_t output_idx = ARCH_FUNCTION(hmx_inc_with_spatial_mask_ovf)(x_tap_idx, intra_tile_x, tile_x_mask, &overflow);
                                        int32_t acc_select = (overflow) ? (current_acc ^ 0x1) & 0x1 : current_acc & 0x1;

                                        output_idx |= intra_tile_y;

                                        int32_t mask = (1 << format)-1;
                                        output_idx = ((output_idx >> 5) & ~mask) | (output_idx & mask);

                                        if (overflow && (state_ptr->drop || state_ptr->deep)) {
                                            output_idx = -1;
                                        }

                                        uint16_t act = 0;
                                        if (!state_ptr->limit_execeeded) {
                                            int32_t cache_idx = act_y_offset + intra_tile_x + input_ch_idx2;
                                            act = (flt_mode) ? state_ptr->act_cache_uh[cache_idx>>1] : state_ptr->act_cache_ub[cache_idx];
                                            VERIF(ARCH_FUNCTION(hmx_act_ld_verif_callback)(state_ptr, act, cache_idx, flt_mode, block_idx););
                                        }
                                        int32_t output_ch_group_start = group_idx*group_size; /* output channel group corresponds to input channel group */
										int32_t output_ch_group_end = output_ch_group_start + group_size; /* stop at next group */
										if((group_size <= parallel_group_size/2) && flt_mode)
                                        {
                                            output_ch_group_start = (group_idx/(parallel_group_size/group_size))*parallel_group_size; output_ch_group_end = output_ch_group_start + parallel_group_size;
                                        }

                                        ARCH_FUNCTION(hmx_mult_inner)(state_ptr, output_idx,acc_select,act,wgt_stream_idx,mult_type,input_ch_idx_raw,x_tap_decoded,y_tap_decoded,block_idx,deep_block_idx, output_channel_scale, flt_mode, group_idx, output_ch_group_start, output_ch_group_end, group_size, fp8_ch_start, fp8_ch_stop);

									}
								}
								if(flt_mode && (mx_fp_rate == 8) && (input_ch_idx_raw - group_idx * group_size >= fp8_ch_stop) && (group_size > 4)){
									DEBUG_PRINT(2, "MX MULT not increase wgt_stream_idx: input_ch_idx_raw=%d, group_idx=%d, fp8_ch_stop=%d", input_ch_idx_raw, group_idx, fp8_ch_stop);
								}
								else
								wgt_stream_idx++;
							}
						}
						if(--state_ptr->mac_cycle_limit == 0 )
                        {
                            x_tap_count = x_tap_decoded;
                            block_idx = block_end;
                            input_ch_idx = input_ch_end;
                            deep_block_idx = deep_block_end;
                            // Set rest of cache to invalid
                            for(; wgt_stream_idx < WGT_CACHE_MAX_SIZE; wgt_stream_idx++ )
                            {
                                const int32_t output_ch_wgt_end = state_ptr->QDSP6_MX_COLS;
                                for(int32_t output_ch_wgt_idx = 0; output_ch_wgt_idx < output_ch_wgt_end; output_ch_wgt_idx++) {
                                    state_ptr->wgt_cache[wgt_stream_idx][output_ch_wgt_idx].valid = 0;
                                }
                            }
                        }
                    }
					ARCH_FUNCTION(hmx_mac_pmu)(state_ptr,flt_mode,state_ptr->blocks);
				}
				current_acc = (deep_mode) ? current_acc ^ 0x1 : current_acc;
			}

		}
		block_idx = ARCH_FUNCTION(hmx_wgt_range_check)(state_ptr, wgt_stream_idx, block_idx, block_end);
	}
	fDEBUG_VERIF_ACC_PRINT(flt_mode);
    if (flt_mode) {
		state_ptr->flt_commit_state.acc_update = 1;
	} else {
		state_ptr->fxp_commit_state.acc_update = 1;
	}
}


void ARCH_FUNCTION(hmx_mult_inner)(hmx_state_t * state_ptr, int32_t row,uint32_t acc_select,uint32_t act,uint32_t wgt_stream_idx,uint32_t mult_type,uint32_t input_channel,uint32_t x_tap,uint32_t y_tap,uint32_t block,uint32_t deep_block,uint32_t output2x,uint32_t is_flt,uint32_t grp_idx,uint32_t grp_start,uint32_t grp_end,uint32_t grp_size, uint32_t fp8_ch_start, uint32_t fp8_ch_stop){
    uint8_t valid = state_ptr->wgt_cache[wgt_stream_idx][0].valid;	// Should be valid for all output channels
	if(input_channel == (input_channel & ~(state_ptr->QDSP6_MX_FP_RATE - 1)))
		state_ptr->fp8_batch_ch_start_wgt_idx = wgt_stream_idx;
	uint8_t valid_fp8_batch = state_ptr->wgt_cache[state_ptr->fp8_batch_ch_start_wgt_idx][0].valid;
	DEBUG_PRINT(2,"HMX_MULT_INNER: limit_exceeded=%d, valid=%d, row=%d, fp8_batch_ch_start_wgt_idx=%d, valid_fp8_batch=%d, fp8_ch_stop=%d", state_ptr->limit_execeeded, valid, row, state_ptr->fp8_batch_ch_start_wgt_idx, valid_fp8_batch, fp8_ch_stop);
	uint32_t fp_rate_8_positive_0_insert = (state_ptr->QDSP6_MX_FP_RATE == 8) && is_flt && (input_channel - grp_idx * grp_size >= fp8_ch_stop) && (grp_size > 4) && valid_fp8_batch;
	uint32_t fp_rate_8_insert_0_small_grp_size = (state_ptr->QDSP6_MX_FP_RATE == 8) && is_flt && (grp_size <= 4) && ((input_channel / grp_size) != grp_idx);
	if(fp_rate_8_positive_0_insert)
		valid = 1;

    const hmx_mult_body_ptr_t mult_fptr = hmx_mult_body_ptr_table[mult_type];
    if ( (!state_ptr->limit_execeeded) && (valid || valid_fp8_batch) && (row>=0) )
    {
        for(uint32_t output_2x_channels = 0; output_2x_channels < output2x; output_2x_channels++)
        {
            for(uint32_t output_ch_idx = grp_start; output_ch_idx < grp_end; output_ch_idx++)
            {
                uint16_t wgt = state_ptr->wgt_cache[wgt_stream_idx][output_ch_idx + (output_2x_channels*32)].wgt;

                DEBUG_PRINT(2,"WGT_SEL CACHE[%02d][%02d]=%02x", wgt_stream_idx, output_ch_idx, wgt);
				if(fp_rate_8_positive_0_insert | fp_rate_8_insert_0_small_grp_size)
				{
					wgt = 0;
					DEBUG_PRINT(2,"FOR FP RATE=8 pos 0 wgt input channel=%d, grp_idx=%d, grp_size=%d, fp8_ch_start=%d, fp8_ch_stop=%d wgt=%02x", input_channel, grp_idx, grp_size, fp8_ch_start, fp8_ch_stop, wgt);
				}
				if( !valid && valid_fp8_batch)
				{
					wgt = 0;
					DEBUG_PRINT(2,"FOR FP RATE=8 wgt, not enough but batch beginning is good, input channel=%d, grp_idx=%d, grp_size=%d, fp8_ch_start=%d, fp8_ch_stop=%d wgt=%02x", input_channel, grp_idx, grp_size, fp8_ch_start, fp8_ch_stop, wgt);
				}
                if (mult_type ==  HMX_MULT_FXP)
                    hmx_mult_fxp(state_ptr, row, output_ch_idx, acc_select, act, wgt, input_channel, x_tap, y_tap, block, output_2x_channels, deep_block, grp_idx, grp_size);
                else if (mult_type == HMX_MULT_FXP_SUBBYTE)
                    hmx_mult_fxp_subbyte(state_ptr, row, output_ch_idx, acc_select, act, wgt, input_channel, x_tap, y_tap, block, output_2x_channels, deep_block, grp_idx, grp_size);
                else
                    mult_fptr(state_ptr, row, output_ch_idx, acc_select, act, wgt, input_channel, x_tap, y_tap, block, output_2x_channels, deep_block, grp_idx, grp_size);

                if (state_ptr->proc->arch_proc_options->pmu_enable)
                {
                    state_ptr->mpy_matrix[row][output_ch_idx][input_channel] = !( (act == 0x0000) || (wgt == 0x0000) ); // Both not-zero
                    if (is_flt) {
                        state_ptr->mpy_matrix[row][output_ch_idx][input_channel] &=  !( (act == 0x8000) || (wgt == 0x8000) ); // Negative 0 in flt
                    }
                }
            }
        }
    }
}




void ARCH_FUNCTION(hmx_unpack_bytes)(hmx_state_t * state_ptr, uint32_t raw_wgt, uint32_t output_ch_wgt_idx, uint32_t wgt_cache_idx, uint32_t wgt_per_word, uint32_t output_scale, uint32_t unpack_idx, paddr_t wgt_addr )
{
    if (wgt_per_word==2) {
        for(uint32_t bit_select = 0, wgt_cache_idx2 = wgt_cache_idx; bit_select < 32; bit_select += 16, wgt_cache_idx2++)
        {
            uint16_t unpacked_wgt = (raw_wgt >> bit_select) & 0xFFFF;
            state_ptr->wgt_cache[wgt_cache_idx2][output_ch_wgt_idx].wgt = unpacked_wgt;
            state_ptr->wgt_cache[wgt_cache_idx2][output_ch_wgt_idx].wgt ^= state_ptr->wgt_negate;
            state_ptr->wgt_cache[wgt_cache_idx2][output_ch_wgt_idx].valid = 1;
            DEBUG_PRINT(2, "WGT_LOAD[%02d][%02d]=0x%04x raw_wgt=0x%08x PA=%llx unpack from bit=%d", wgt_cache_idx2, output_ch_wgt_idx, (uint16_t)unpacked_wgt, raw_wgt, wgt_addr, bit_select);
        }
    }
    else
    {
        const int32_t output_ch_wgt_end = state_ptr->QDSP6_MX_COLS;
        const uint32_t bit_stride = 32 / wgt_per_word;
        hmx_unpack_ptr_t unpack_fptr = hmx_unpack_ptr_table[unpack_idx];
        for(int32_t bit_select = 0, wgt_cache_idx2 = wgt_cache_idx, wgt_cache_idx3 = wgt_cache_idx; bit_select < 8; bit_select+=bit_stride)
        {
            for(int32_t byte_select = 0; byte_select < 32; byte_select += 8)
            {
                int8_t packed_wgt = (int8_t)(raw_wgt >> byte_select) & 0xFF;
                int8_t unpacked_wgt = unpack_fptr(packed_wgt, bit_select);

                if ((output_scale == 2) && (bit_select == 4))
                {
                    DEBUG_PRINT(2, "WGT_LOAD[%02d][%02d]=0x%02x raw_wgt=0x%08x packed_wgt=0x%02x PA=%llx unpack from bit=%d byte=%d", wgt_cache_idx3, output_ch_wgt_idx + output_ch_wgt_end, (uint8_t)unpacked_wgt, raw_wgt, (uint8_t)packed_wgt, wgt_addr, bit_select, byte_select);
                    state_ptr->wgt_cache[wgt_cache_idx3][output_ch_wgt_idx + output_ch_wgt_end].wgt = unpacked_wgt;
                    state_ptr->wgt_cache[wgt_cache_idx3][output_ch_wgt_idx + output_ch_wgt_end].valid = 1;
                    wgt_cache_idx3++;

                } else {
                    DEBUG_PRINT(2, "WGT_LOAD[%02d][%02d]=0x%02x raw_wgt=0x%08x packed_wgt=0x%02x PA=%llx unpack from bit=%d byte=%d", wgt_cache_idx2, output_ch_wgt_idx, (uint8_t)unpacked_wgt, raw_wgt, (uint8_t)packed_wgt, wgt_addr, bit_select, byte_select);
                    state_ptr->wgt_cache[wgt_cache_idx2][output_ch_wgt_idx].wgt = unpacked_wgt;
                    state_ptr->wgt_cache[wgt_cache_idx2][output_ch_wgt_idx].valid = 1;
                    wgt_cache_idx2++;
                }
            }
        }
    }
}

int8_t ARCH_FUNCTION(hmx_wgt_ld_meta_data)(thread_t *env, hmx_state_t * state_ptr, uint32_t * metadata, paddr_t wgt_uc_metadata_addr,  paddr_t wgt_addr, paddr_t wgt_addr_end)
{
    int8_t meta_addr_valid = 1;
    uint32_t wgtc_transpose_metadata[128];
    const int32_t output_ch_wgt_lane_end = 4 * state_ptr->QDSP6_MX_COLS;
    for(int32_t output_ch_wgt_lane_idx = 0; output_ch_wgt_lane_idx < output_ch_wgt_lane_end; output_ch_wgt_lane_idx += 16)
    {
        for(int32_t output_ch_idx = 0; output_ch_idx < 16; output_ch_idx += 4)
        {
            paddr_t output_ch_wgt_metadata_addr = wgt_uc_metadata_addr + output_ch_wgt_lane_idx + output_ch_idx;
            uint32_t wgtc_metadata_output_ch_raw = sim_mem_read4(state_ptr->system, state_ptr->threadId, output_ch_wgt_metadata_addr);

            VERIF(ARCH_FUNCTION(hmx_wgt_verif_callback)(state_ptr, 1, output_ch_wgt_metadata_addr, wgt_addr_end, 4, wgtc_metadata_output_ch_raw, 0, 0);) // Metadata callback

            for(int32_t byte_idx = 0; byte_idx < 4; byte_idx++)
            {
                int32_t metadata_transpose_idx = output_ch_wgt_lane_idx + output_ch_idx + byte_idx;
                wgtc_transpose_metadata[metadata_transpose_idx] = (wgtc_metadata_output_ch_raw >> (byte_idx * 8)) & 0xFF;
            }
        }
    }

    for(int32_t byte_idx = 0; byte_idx < 128; byte_idx++)
    {
        for(int32_t vector_idx = 0; vector_idx < 8; vector_idx++)
        {
            int32_t bit_pos = byte_idx & 7;

            if(wgt_addr == (wgt_addr_end & (~0x7f)))
            {
                metadata[vector_idx*16 + byte_idx/8] |=  0x01 << bit_pos;
                meta_addr_valid = 0;
            }
            else
            {
                if((wgtc_transpose_metadata[byte_idx] >> vector_idx) & 0x1)
                    metadata[vector_idx*16 + byte_idx/8] |=  0x01 << bit_pos;
            }
        }
    }
    return meta_addr_valid;
}

void ARCH_FUNCTION(hmx_ld_wgt)(thread_t *env, hmx_state_t * state_ptr, paddr_t wgt_addr, paddr_t wgt_addr_end, uint32_t wgt_per_word, uint32_t output_scale, uint32_t is_flt, uint32_t unpack_idx )
{
	processor_t * proc __attribute__((unused)) = state_ptr->proc;

    DEBUG_PRINT(2, "Loading WGTs from pa range[%llx %llx]  WGTs per word=%d, output channel scale=%d wgtc_mode=%d", wgt_addr, wgt_addr_end, wgt_per_word, output_scale, state_ptr->wgtc_mode);

    const uint32_t output_ch_wgt_end = state_ptr->QDSP6_MX_COLS;
	//const uint32_t mx_fp_rate = state_ptr->QDSP6_MX_FP_RATE;

    int32_t wgt_cache_idx = 0;
    uint32_t block_idx = 0;
    while(wgt_addr <= wgt_addr_end)
    {
        if(state_ptr->wgtc_mode) //weight compression mode
        {
            // First Load metadata
            uint32_t wgtc_metadata[128] = {0};
            DEBUG_PRINT_VAR(paddr_t wgt_uc_metadata_addr = wgt_addr + (4 * output_ch_wgt_end);)
            INC_PSTATN(phmxwgtcomp_metadata_rd, 1);
            INC_PSTATN(phmxwgtdcomp_issue, 2);
#ifdef VERIFICATION
            int8_t meta_addr_valid = ARCH_FUNCTION(hmx_wgt_ld_meta_data)(env, state_ptr, wgtc_metadata, wgt_uc_metadata_addr,  wgt_addr, wgt_addr_end);
#endif
            const int32_t wgt_total_bytes = state_ptr->wgtc_total_bytes + 1;
            int32_t wgt_total_bytes_lane_cnt[8] = {0};
            int32_t wgt_local_total_bytes_lane[8] = {0};
            INC_PSTATN(phmxwgtcomp_compdata_rd, wgt_total_bytes/16);

            for(uint32_t vector_idx = 0; vector_idx < 8; vector_idx++)
            {
                const int32_t output_ch_wgt_lane_end = output_ch_wgt_end / 4;
                for(uint32_t output_ch_wgt_lane_idx = 0; output_ch_wgt_lane_idx < output_ch_wgt_lane_end; output_ch_wgt_lane_idx++)
                {
                    paddr_t wgt_data_lane_addr = wgt_addr + state_ptr->wgtc_start_offset + (16 * output_ch_wgt_lane_idx);

                    DEBUG_PRINT_VAR(paddr_t wgt_metadata_lane_addr = wgt_uc_metadata_addr + (16 * output_ch_wgt_lane_idx) + (2*vector_idx));
                    DEBUG_PRINT(2, "WGT DCOMPRESS: output_ch_wgt_lane_idx=%d, wgt_metadata_lane_addr=%llx, wgt_data_lane_addr=%llx, wgt_total_bytes=%d, vector_idx=%d", output_ch_wgt_lane_idx, wgt_metadata_lane_addr, wgt_data_lane_addr, wgt_local_total_bytes_lane[output_ch_wgt_lane_idx], vector_idx);

                    uint16_t meta_16bits = 0;
                    DEBUG_ONLY_VAR(meta_16bits);
                    uint64_t val_8_bytes[2] = {0};
                    for(uint32_t metadata_idx = 0; metadata_idx < 2; metadata_idx++)
                    {
                        uint32_t metadata_pos = (16 * vector_idx) + (2 * output_ch_wgt_lane_idx) + metadata_idx;
                        uint32_t metadata_val = wgtc_metadata[metadata_pos];
                        meta_16bits += metadata_val << (8 * metadata_idx);

                        DEBUG_PRINT(2, "WGT DCOMPRESS: metadata[%d,%d,%d]=%04x, metadata_idx=%d", vector_idx, output_ch_wgt_lane_idx, metadata_idx, metadata_val, metadata_pos);

                        uint32_t wgt_4byte_raw = 0;
                        for(uint32_t bit_idx = 0; bit_idx < 8; bit_idx++)
                        {
                            paddr_t wgt_data_addr = wgt_data_lane_addr + ((metadata_idx >> 1) * 16) + (wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx]);
                            DEBUG_PRINT(2, "WGT DCOMPRESS: wgt_data_addr=%llx, wgt_data_lane_addr=%llx wgt_addr_end=%llx metadata_idx=%d wgt_total_bytes_lane_cnt[%d]=%x ", (long long int)wgt_data_addr, (long long int)wgt_data_lane_addr, (long long int)wgt_addr_end, metadata_idx, output_ch_wgt_lane_idx, wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx]);
                            uint32_t wgtc_cache_c_lane_raw = (wgt_data_addr > wgt_addr_end) ?  0 : sim_mem_read4(state_ptr->system, state_ptr->threadId, ((wgt_data_addr>>2)<<2));


                            wgt_cache_idx = (block_idx * 8 * (wgt_per_word/output_scale)) + (vector_idx * (wgt_per_word / output_scale));
                            wgt_cache_idx = (wgt_cache_idx >= WGT_CACHE_MAX_SIZE) ? (WGT_CACHE_MAX_SIZE-1) :wgt_cache_idx; // maybe not needed?

                            uint8_t wgt_byte_raw = 0;
                            if( ((metadata_val >> bit_idx) & 0x1) && (wgt_local_total_bytes_lane[output_ch_wgt_lane_idx] < wgt_total_bytes) )
                            {
                                wgt_byte_raw = (wgtc_cache_c_lane_raw >> ((wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx] & 0x3) * 8)) & 0xFF;

                                VERIF(ARCH_FUNCTION(hmx_wgt_verif_callback)(state_ptr, 1, wgt_data_addr, wgt_addr_end, 4, wgt_byte_raw, 1, (wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx] & 0x3));)

                                wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx]++;
                                if( (wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx] & 0xF ) == 0)
                                {
                                    if(wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx] == 16)
                                    {
                                        wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx] += 128;
                                    }
                                    wgt_total_bytes_lane_cnt[output_ch_wgt_lane_idx] += 112;
                                }
                                wgt_local_total_bytes_lane[output_ch_wgt_lane_idx]++;
                            }


                            val_8_bytes[metadata_idx] += ((uint64_t)wgt_byte_raw) << ( bit_idx * 8 );

                            wgt_4byte_raw += ((uint64_t)wgt_byte_raw) << ( (bit_idx & 0x3) * 8 );
                            DEBUG_PRINT(2, "WGT DCOMPRESS: bit_idx=%d, wgt_4byte_raw=0x%08x, wgt_byte_raw=0x%02x", bit_idx, wgt_4byte_raw, wgt_byte_raw);

                            if( (bit_idx & 0x3) == 0x3)
                            {
                                uint32_t output_ch_c_idx = (output_ch_wgt_lane_idx * 4) + (metadata_idx * 2) + ( bit_idx / 4 );
                                ARCH_FUNCTION(hmx_unpack_bytes)(state_ptr, wgt_4byte_raw, output_ch_c_idx,  wgt_cache_idx, wgt_per_word, output_scale, unpack_idx, wgt_data_addr );
                                wgt_4byte_raw = 0;
                            }

                        }
                    }
                    DEBUG_PRINT(1, "WGT DCOMPRESS: block_idx=%d, lane_idx=%d vector_idx=%d, metadata_addr=%llx, meta_16bits=%x, val_hi_8_bytes=%llx, val_lo_8_bytes=%llx, wgt_addr=%llx, wgt_addr_end=%llx, wgt_total_bytes=%d", block_idx, output_ch_wgt_lane_idx, vector_idx, wgt_uc_metadata_addr, meta_16bits, (long long int)val_8_bytes[1], (long long int)val_8_bytes[0], wgt_addr, wgt_addr_end, wgt_total_bytes);
#ifdef VERIFICATION
                    if (state_ptr->proc->options->hmx_wgt_decomp_callback) {
			            state_ptr->proc->options->hmx_wgt_decomp_callback(state_ptr->system, state_ptr->proc, state_ptr->pktid, block_idx, output_ch_wgt_lane_idx, vector_idx, wgt_uc_metadata_addr, meta_addr_valid, meta_16bits, val_8_bytes[1], val_8_bytes[0]);
		            }
#endif

                }
            }
            block_idx++;
            wgt_addr = (paddr_t)(((wgt_addr + (wgt_total_bytes << 3)) & (~0x7f)) + 0x80);
        } else {
            // this is one vector worth of weights
            for(uint32_t output_ch_wgt_idx = 0; output_ch_wgt_idx < output_ch_wgt_end; output_ch_wgt_idx++)
            {
                uint32_t raw_wgt = sim_mem_read4(state_ptr->system, state_ptr->threadId, wgt_addr);  	 // Read 4 bytes per output channel
                VERIF(ARCH_FUNCTION(hmx_wgt_verif_callback)(state_ptr, 0, wgt_addr, wgt_addr_end, wgt_per_word, raw_wgt, 0, 0);)
                ARCH_FUNCTION(hmx_unpack_bytes)(state_ptr, raw_wgt, output_ch_wgt_idx,  wgt_cache_idx, wgt_per_word, output_scale,  unpack_idx, wgt_addr);

                wgt_addr += 4;
            }

        }
        wgt_cache_idx += wgt_per_word / output_scale;

    }

    if (wgt_per_word==2) {
        // Special case for floating point if a multiple of 128 bytes were provided instead of 256, fill the other 2 channels with zero
		uint32_t zero_fill_mask = 0x3;
		if(state_ptr->QDSP6_MX_FP_RATE == 8)
			zero_fill_mask = 0x1;

        while( (wgt_cache_idx & zero_fill_mask) )
        {
            for(int32_t output_ch_wgt_idx = 0; output_ch_wgt_idx < output_ch_wgt_end; output_ch_wgt_idx++)
            {
                state_ptr->wgt_cache[wgt_cache_idx][output_ch_wgt_idx].wgt = 0;
                state_ptr->wgt_cache[wgt_cache_idx][output_ch_wgt_idx].valid = 1;
                //state_ptr->wgt_cache[wgt_cache_idx][output_ch_wgt_idx].wgt  ^= state_ptr->wgt_negate;
                DEBUG_PRINT(2, "WGT_LOAD[%02d][%02d]=0x0 Set to zero not enough weights provided FP case", wgt_cache_idx, output_ch_wgt_idx);
            }
            wgt_cache_idx++;
        }

    }
    // Set rest of cache to invalid
    DEBUG_PRINT(2, "clearing rest of cache starting at %d", wgt_cache_idx);
    for(;wgt_cache_idx < WGT_CACHE_MAX_SIZE; wgt_cache_idx++ )
    {
        for(int32_t output_ch_wgt_idx = 0; output_ch_wgt_idx < output_ch_wgt_end * output_scale; output_ch_wgt_idx++)
        {
            state_ptr->wgt_cache[wgt_cache_idx][output_ch_wgt_idx].valid = 0;
        }
    }
}

// Populate information from the state to the hmx_maptr for the timing model
// maybe the state should just have a hmx_mem_access_info_t var instead, and just log that
void ARCH_FUNCTION(hmx_populate_hmx_maptr)(thread_t * thread, hmx_state_t * state_ptr) {

    mem_access_info_t *maptr = &thread->mem_access[0];
	hmx_mem_access_info_t * hmx_maptr = &maptr->hmx_ma;
    hmx_maptr->flt = state_ptr->is_flt;
	hmx_maptr->acc_select = state_ptr->acc_select;  // For dependency checking between mac and cvt
	hmx_maptr->x_offset = state_ptr->x_offset;
	hmx_maptr->y_offset = state_ptr->y_offset;
    hmx_maptr->dY = state_ptr->dY;
    hmx_maptr->format = state_ptr->format;
    hmx_maptr->tile_y_inc = state_ptr->tile_y_inc;
    hmx_maptr->cvt_wr_only = (state_ptr->cvt_type == HMX_CVT_BOTH); // If both direction, then it's only a convert write
}

void ARCH_FUNCTION(hmx_populate_hmx_mac_maptr)(thread_t * thread, hmx_state_t * state_ptr) {

    hmx_mem_access_info_t * hmx_wgt_maptr = &thread->mem_access[0].hmx_ma;
	hmx_mem_access_info_t * hmx_act_maptr = &thread->mem_access[1].hmx_ma;

	hmx_act_maptr->dY = hmx_wgt_maptr->dY  = state_ptr->dY;
	hmx_act_maptr->blocks = hmx_wgt_maptr->blocks  = state_ptr->blocks;
	hmx_act_maptr->fx = hmx_wgt_maptr->fx  = state_ptr->fx;
	hmx_act_maptr->fy = hmx_wgt_maptr->fy  = state_ptr->fy;
	hmx_act_maptr->x_offset = hmx_wgt_maptr->x_offset  = state_ptr->x_offset;
	hmx_act_maptr->y_offset = hmx_wgt_maptr->y_offset  = state_ptr->y_offset;
    hmx_act_maptr->tile_y_inc = hmx_wgt_maptr->tile_y_inc = state_ptr->tile_y_inc;
	hmx_act_maptr->y_start = hmx_wgt_maptr->y_start  = state_ptr->y_start;
	hmx_act_maptr->y_stop = hmx_wgt_maptr->y_stop  = state_ptr->y_stop;
	hmx_act_maptr->x_start = hmx_wgt_maptr->x_start  = state_ptr->x_start;
	hmx_act_maptr->x_stop = hmx_wgt_maptr->x_stop  = state_ptr->x_stop;
	hmx_act_maptr->y_tap = hmx_wgt_maptr->y_tap  = state_ptr->y_tap;
	hmx_act_maptr->x_tap = hmx_wgt_maptr->x_tap  = state_ptr->x_tap;
	hmx_act_maptr->ch_start = hmx_wgt_maptr->ch_start  = state_ptr->ch_start;
	hmx_act_maptr->ch_stop = hmx_wgt_maptr->ch_stop  = state_ptr->ch_stop;
	hmx_act_maptr->group_size = hmx_wgt_maptr->group_size  = state_ptr->group_size;
	hmx_act_maptr->group_count = hmx_wgt_maptr->group_count  = state_ptr->group_count;
	hmx_act_maptr->block_type = state_ptr->act_block_type;
    hmx_wgt_maptr->block_type  = state_ptr->wgt_block_type;
	hmx_act_maptr->format = hmx_wgt_maptr->format  = state_ptr->format;
	hmx_act_maptr->acc_select = hmx_wgt_maptr->acc_select = state_ptr->acc_select ;
	hmx_act_maptr->acc_range = hmx_wgt_maptr->acc_range = state_ptr->acc_range ;
	hmx_act_maptr->flt = hmx_wgt_maptr->flt = state_ptr->is_flt ;
	hmx_act_maptr->x_dilate = hmx_wgt_maptr->x_dilate = state_ptr->x_dilate;
	hmx_act_maptr->y_dilate = hmx_wgt_maptr->y_dilate  = state_ptr->y_dilate;
	hmx_act_maptr->deep = hmx_wgt_maptr->deep = state_ptr->deep;
	hmx_act_maptr->wgt_deep = hmx_wgt_maptr->wgt_deep = state_ptr->wgt_deep;
	hmx_act_maptr->drop = hmx_wgt_maptr->drop  = state_ptr->drop;
	hmx_act_maptr->batch = hmx_wgt_maptr->batch = state_ptr->batch;
	hmx_act_maptr->weight_count = hmx_wgt_maptr->weight_count  = state_ptr->weight_count;
	hmx_act_maptr->bias_32bit = hmx_wgt_maptr->bias_32bit  = state_ptr->bias_32bit;
	hmx_act_maptr->weight_bits = hmx_wgt_maptr->weight_bits = state_ptr->weight_bits;
	hmx_act_maptr->enable16x16 = hmx_wgt_maptr->enable16x16  = state_ptr->enable16x16;
	hmx_act_maptr->outputselect16x16 = hmx_wgt_maptr->outputselect16x16 = state_ptr->outputselect16x16;
    hmx_act_maptr->tile_x_mask = state_ptr->tile_x_mask;
    hmx_act_maptr->tile_y_mask = state_ptr->tile_y_mask;
	hmx_act_maptr->wgt_size = hmx_wgt_maptr->wgt_size = (int32_t)((state_ptr->max_weight_pa & ~0x7F) - (state_ptr->wgt_addr & ~0x7F) + 128) / 128;
	hmx_act_maptr->wgtc_mode = hmx_wgt_maptr->wgtc_mode = state_ptr->wgtc_mode;
	hmx_act_maptr->wgtc_global_density = hmx_wgt_maptr->wgtc_global_density = state_ptr->wgtc_total_bytes;
    thread->mem_access[0].lddata = state_ptr->max_weight_pa;
}

static inline uint32_t hmx_get_masked_inc(uint32_t mask) {
    return mask & (~mask+1);
}

void ARCH_FUNCTION(hmx_cvt_mem_parameters)(hmx_state_t * state_ptr, uint32_t start, uint32_t range, uint32_t type, uint32_t format, uint32_t direction)
{

	state_ptr->dY =  range & ~2047;
	state_ptr->enable16x16 = (type == HMX_UH_UH);
	state_ptr->outputselect16x16 = (start & 0x40) > 0; //using channel bits to select 16x16 upper or lower
    state_ptr->format = format;
	state_ptr->cvt_type = direction;
    state_ptr->is_flt = (type == HMX_FP16) ;

	if (state_ptr->is_flt) {
		state_ptr->flt_commit_state.cvt_write = 1;
	} else {
		state_ptr->fxp_commit_state.cvt_write = 1;
	}

    state_ptr->tile_x_mask = hmx_get_spatial_mask(~range, format);
    state_ptr->tile_y_mask = hmx_get_spatial_mask( range, format);
	if(state_ptr->is_flt) { // clear LSB of either direction for the mask
		state_ptr->tile_x_mask &= ~1;
		state_ptr->tile_y_mask &= ~1;
	}

	state_ptr->tile_x_inc = hmx_get_masked_inc(state_ptr->tile_x_mask);
	state_ptr->tile_y_inc = hmx_get_masked_inc(state_ptr->tile_y_mask);

	state_ptr->x_offset = start & state_ptr->tile_x_mask;
	state_ptr->y_offset = start & state_ptr->tile_y_mask;
	state_ptr->x_acc_offset = 0;
	if(((state_ptr->cvt_type == HMX_CVT_BEFORE) || (state_ptr->cvt_type == HMX_CVT_BOTH))&& (state_ptr->tile_x_mask)){
		int32_t x_count = state_ptr->x_offset;
		int32_t x_acc_offset = 0;
		int32_t mask = state_ptr->tile_x_mask | (1 << 31);
		while (x_count>=0) {
			if (x_count>=0) {
				x_acc_offset =ARCH_FUNCTION(hmx_inc_with_spatial_mask)(x_acc_offset, state_ptr->tile_x_inc, mask);
			}
			x_count = ARCH_FUNCTION(hmx_inc_with_spatial_mask)(x_count, state_ptr->tile_x_inc, mask);
		}
		state_ptr->x_acc_offset = x_acc_offset;
	}
	DEBUG_PRINT(0, "HMX_%s_CVT MXMEM Parameters: start=0x%08x range: 0x%08x (%s major)", (state_ptr->is_flt) ? "FLT" : "FXP", start, range, (format) ? "spatial" : "channel");
	DEBUG_PRINT(0, "HMX_%s_CVT MXMEM Parameters: x_mask: 0x%x x_inc: 0x%x x_offset: 0x%x y_mask: 0x%x y_inc: 0x%x y_offset: 0x%x ", (state_ptr->is_flt) ? "FLT" : "FXP",
    state_ptr->tile_x_mask, state_ptr->tile_x_inc, state_ptr->x_offset, state_ptr->tile_y_mask, state_ptr->tile_y_inc, state_ptr->y_offset );

}


void ARCH_FUNCTION(hmx_cvt_tx_parameters)(hmx_state_t * state_ptr, uint32_t usr, uint32_t type, uint32_t feedback)
{

    state_ptr->data_type = type;
    state_ptr->is_flt  = (type == HMX_FP16) ;
	state_ptr->acc_select = state_ptr->is_flt  ? state_ptr->current_acc_flt : state_ptr->current_acc_fxp;

	uint32_t rmw = (feedback && (type != HMX_UH)) ? 1 : 0 ;

    if (state_ptr->is_flt) {
		state_ptr->flt_commit_state.cvt_update = 1;
		state_ptr->flt_commit_state.cvt_advance = rmw;
		state_ptr->flt_commit_state.acc_clear = 0;
	} else {
		state_ptr->fxp_commit_state.cvt_update = 1;
		state_ptr->fxp_commit_state.cvt_advance = rmw;
		state_ptr->fxp_commit_state.acc_clear = 0;
	}
	state_ptr->usr_fp.raw = usr;
    DEBUG_PRINT(0, "HMX_%s_CVT TX Parameters: usr inf_nan_enable=%d nan_propagate=%d", (state_ptr->is_flt) ? "FLT" : "FXP", state_ptr->usr_fp.inf_nan_enable, state_ptr->usr_fp.nan_propagate);
}





void ARCH_FUNCTION(hmx_act_paramaters)(hmx_state_t * state_ptr, vaddr_t start, vaddr_t range, uint32_t slot, uint32_t type, uint32_t format, uint32_t block_type)
{
    uint32_t ch_mask = (state_ptr->QDSP6_MX_COLS-1) << format;

	state_ptr->dY = range & ~2047;
	state_ptr->is_flt  = (type == HMX_FP16) ;
	state_ptr->format = format;
	state_ptr->act_block_type = block_type;

	state_ptr->ch_start = ((start & ch_mask) >> format);
	state_ptr->ch_stop  = ((range & ch_mask) >> format);

    state_ptr->tile_x_mask = hmx_get_spatial_mask(~range, format);
    state_ptr->tile_y_mask = hmx_get_spatial_mask( range, format);

	if(state_ptr->is_flt) {
		state_ptr->tile_x_mask &= ~1;
		state_ptr->tile_y_mask &= ~1;
	}

	state_ptr->tile_x_inc = hmx_get_masked_inc(state_ptr->tile_x_mask);
	state_ptr->tile_y_inc = hmx_get_masked_inc(state_ptr->tile_y_mask);

	state_ptr->blocks = 1;
	state_ptr->fx = start & state_ptr->tile_x_mask;
	state_ptr->fy = start & state_ptr->tile_y_mask;
	state_ptr->acc_select = (state_ptr->is_flt) ? state_ptr->current_acc_flt : state_ptr->current_acc_fxp;

    state_ptr->x_tap = 0;
    state_ptr->x_start  = 0;
    state_ptr->x_stop  = 0;
    state_ptr->y_tap = 0;
    state_ptr->y_stop = 0;
    state_ptr->y_start = 0;
    state_ptr->batch = 0;

	state_ptr->y_dilate = (block_type == HMX_ACT_DILATE);
	state_ptr->batch = (block_type == HMX_ACT_BATCH);

	if (block_type == HMX_ACT_DEEP) {
        state_ptr->blocks = (state_ptr->dY < 0) ? 0 : (((range & ~2047) >> 11) + 1);
		state_ptr->y_tap = 1;
	} else if ((block_type == HMX_ACT_BLOCK) || (block_type == HMX_ACT_DILATE) || (block_type == HMX_ACT_BATCH)) {
		state_ptr->y_stop = state_ptr->fy;
		if (state_ptr->tile_y_mask!=0) {
			state_ptr->y_tap = ARCH_FUNCTION(hmx_compute_filter_size)(state_ptr->y_tap, state_ptr->fy, state_ptr->tile_y_mask, state_ptr->tile_y_inc);
		} else {
			state_ptr->y_tap = 1;
		}
		if (state_ptr->batch) {
			state_ptr->y_tap = 1;
		}
	} else if (block_type == HMX_ACT_ABOVE) {
		state_ptr->y_start = state_ptr->fy;
		state_ptr->y_stop  = state_ptr->tile_y_mask;
		if (state_ptr->tile_y_mask != 0) {
			state_ptr->y_tap = ARCH_FUNCTION(hmx_compute_filter_size_above)(state_ptr->y_tap, state_ptr->fy, state_ptr->tile_y_mask, state_ptr->tile_y_inc);
            if (state_ptr->y_tap == 0)
				state_ptr->y_tap = 1;
		} else {
			state_ptr->y_tap = 1;
		}
	} else if (block_type == HMX_ACT_SINGLE) {
		state_ptr->y_start = state_ptr->fy;
		state_ptr->y_stop  = state_ptr->fy;
		state_ptr->y_tap   = 1;
	}
    DEBUG_PRINT(2, "MX SETUP: state_ptr->y_start =%x state_ptr->y_stop =%x state_ptr->y_tap=%x fy=%x mask=%x", state_ptr->y_start, state_ptr->y_stop , state_ptr->y_tap, state_ptr->fy, state_ptr->tile_y_mask);

	// if start > stop, we are in grouped convolution mode. we will process all 32 input channels so overwrite the start/stop values
	// leading 1s  | group size
	// 0 | 32
	// 1 | 16
	// 2 | 8
	// 3 | 4
	// no group conv when in deep mode

	state_ptr->group_convolution = (state_ptr->ch_start > state_ptr->ch_stop) && (block_type != HMX_ACT_DEEP);
	size4u_t cl1 = fCL1_1(((state_ptr->ch_stop - state_ptr->ch_start) ^ state_ptr->ch_start ^ state_ptr->ch_stop) << 2); //count of leading ones

	state_ptr->group_size =  state_ptr->group_convolution ? (32 >> cl1) : 32; // maybe just 32>>clz
	state_ptr->group_count = 32 / state_ptr->group_size;

	int group_ch_start = state_ptr->ch_start & (0x1F >> cl1);
	int group_ch_stop = state_ptr->ch_stop & (0x1F >> cl1);

	if(state_ptr->group_count == 16)
	{
		group_ch_start = (state_ptr->ch_start & ~0x1) & (0x1F >> cl1);
		group_ch_stop = (state_ptr->ch_stop | 0x1) & (0x1F >> cl1);
	}
	else if(state_ptr->group_count <= 8)
	{
		group_ch_start = (state_ptr->ch_start & ~0x3) & (0x1F >> cl1);
		group_ch_stop = (state_ptr->ch_stop | 0x3) & (0x1F >> cl1);
	}
	state_ptr->ch_start = state_ptr->group_convolution ? group_ch_start : (state_ptr->ch_start & ~0x3);
	state_ptr->ch_stop = state_ptr->group_convolution ? (group_ch_stop + 1) : ((state_ptr->ch_stop | 0x3)+1);

	if ((state_ptr->ch_start >= state_ptr->ch_stop) && (state_ptr->blocks==1))  {
		state_ptr->blocks = 0;
	}

	if (state_ptr->tile_x_mask!=0) {
        state_ptr->x_tap = ARCH_FUNCTION(hmx_compute_filter_size)(state_ptr->x_tap, state_ptr->fx, state_ptr->tile_x_mask, state_ptr->tile_x_inc) ;
	} else {
		state_ptr->x_tap = 1;
	}
	if (block_type == HMX_ACT_DEEP) {
		int limit = (state_ptr->is_flt) ? state_ptr->QDSP6_MX_FP_ROWS : state_ptr->QDSP6_MX_ROWS;
		if (state_ptr->blocks > limit) {
			//warn(":deep block count=%d exceeded a max of %d. Limiting to max", state_ptr->blocks, limit);
			state_ptr->blocks = limit;
		}
	}

	state_ptr->fxp_commit_state.acc_update = 0;
	state_ptr->flt_commit_state.acc_update = 0;
    state_ptr->mac_cycle_limit = (state_ptr->is_flt) ? (1024 / state_ptr->QDSP6_MX_FP_RATE) : 512;
    state_ptr->operand_ready |= (state_ptr->blocks>0)<<slot;
	if(state_ptr->is_flt && !state_ptr->support_fp16)
	{
		state_ptr->blocks = 0;
		DEBUG_PRINT(0, "HMX FLT: set block to 0 for hmx without fp16 support, pktid=0x%08x", state_ptr->pktid);
	}

}


void ARCH_FUNCTION(hmx_wgt_paramaters)(hmx_state_t * state_ptr, vaddr_t start, vaddr_t range, uint32_t slot, uint32_t type, uint32_t block_type, uint32_t weight_count, uint32_t output_ch_scale, uint32_t unpack, uint32_t usr)
{

	state_ptr->is_bf16 = (start >> 6) & 0x1;
	state_ptr->is_bf16 &= (state_ptr->QDSP6_MX_FP_ACC_EXP >= 8);
    state_ptr->is_flt = (type == HMX_FP16);
	state_ptr->wgtc_mode = (start & 0x10) >> 4;

    state_ptr->wgtc_total_bytes = 0;

    if(state_ptr->wgtc_mode != 0)
	{
		state_ptr->wgtc_start_offset = start & 0xF;
		state_ptr->wgtc_total_bytes = range & 0x7F;
		//global density only falls on the compression lane boundary
		state_ptr->wgtc_total_bytes |= 0xF;
		if(state_ptr->wgtc_total_bytes >= 0x7F)
			state_ptr->wgtc_total_bytes = 0x7F;
	}
	state_ptr->wgtc_start_offset = 0; //temporarily ignore weight decompression offset
	state_ptr->limit_execeeded = 0;

    state_ptr->weight_bits = hmx_unpack_bits_table[unpack];
	if(!state_ptr->is_flt && (state_ptr->weight_bits < 8) && !((state_ptr->weight_bits == 4) & (output_ch_scale == 2)))
	{
		if(state_ptr->group_convolution)
		{
			state_ptr->ch_start = 0;
    		state_ptr->ch_stop = state_ptr->group_size;
		}
		else
		{
			state_ptr->ch_start = 0;
			state_ptr->ch_stop = 32;
		}
		DEBUG_PRINT(2, "Fix start stop channel at the boundary of groups: weight_bits = %x group_conv: %x, group_size: %d, ch_start: %d, ch_stop: %d", state_ptr->weight_bits, state_ptr->group_convolution, state_ptr->group_size, state_ptr->ch_start, state_ptr->ch_stop);
	}

	state_ptr->wgt_negate = (state_ptr->is_flt) ? (((start & (0x20)) > 0) << 15) : 0;

	state_ptr->wgt_block_type = block_type;
	state_ptr->acc_range = (state_ptr->fx > 0);
	state_ptr->wgt_deep  = 0;


    state_ptr->deep = 0;
    state_ptr->x_dilate = (block_type == HMX_WEI_DILATE);
    state_ptr->drop = (block_type == HMX_WEI_DROP);

	if ((block_type == HMX_WEI_NORMAL) || (block_type == HMX_WEI_DILATE) || (block_type == HMX_WEI_DROP)) {
		state_ptr->x_stop   = state_ptr->fx;
		if (state_ptr->drop) {
			state_ptr->acc_range = 0;
		}
	} else if (block_type == HMX_WEI_DEEP) {
		state_ptr->deep = 1;
		state_ptr->wgt_deep = 1;
		state_ptr->acc_range =  0;
		state_ptr->x_stop   =  state_ptr->fx = 0;
		state_ptr->x_tap   =  1;		// Force to single Tap
	} else if (block_type == HMX_WEI_AFTER) {
		state_ptr->x_start = state_ptr->fx;
		state_ptr->x_stop  = state_ptr->tile_x_mask;
		state_ptr->x_tap = 0;
		if (state_ptr->tile_x_mask!=0) {
            state_ptr->x_tap = ARCH_FUNCTION(hmx_compute_filter_size_above)(state_ptr->x_tap, state_ptr->fx, state_ptr->tile_x_mask, state_ptr->tile_x_inc);
		} else {
			state_ptr->x_tap = 1;
		}
	} else if (block_type == HMX_WEI_SINGLE) {
		state_ptr->x_tap   = 1;		// Force to single Tap
		state_ptr->x_start = state_ptr->fx;
		state_ptr->x_stop  = state_ptr->fx;

	}
    DEBUG_PRINT(2, "MX SETUP: state_ptr->x_start  =%x  state_ptr->x_stop=%x state_ptr->x_tap=%x state_ptr->fx=%x state_ptr->tile_x_mask=%x", state_ptr->x_start , state_ptr->x_stop, state_ptr->x_tap, state_ptr->fx, state_ptr->tile_x_mask);
	state_ptr->usr_fp.raw = usr;
	state_ptr->fp8_batch_ch_start_wgt_idx = 0;
}

void ARCH_FUNCTION(hmx_update_verif_trace)(thread_t * thread) {
#ifdef VERIFICATION
    processor_t * proc = thread->processor_ptr;
    if (thread->processor_ptr->options->testgen_mode)
       return;
    THREAD2HMXSTRUCT->fp_hmx_debug = proc->fp_hmx_debug;
    THREAD2HMXSTRUCT->hmx_debug_lvl = proc->ver_hmx_debug_print_level;

#endif
}
void ARCH_FUNCTION(hmx_configure_state)(thread_t * thread)
{
    ARCH_FUNCTION(hmx_tmp_set_state)(thread, THREAD2HMXSTRUCT);
}
// This is stuff that should only be set once at the init
void ARCH_FUNCTION(hmx_tmp_set_state)(thread_t * thread, hmx_state_t * state_ptr)
{
    processor_t * proc = thread->processor_ptr;

    state_ptr->thread = thread; // For callbacks
    state_ptr->proc   = proc; // For callbacks
    state_ptr->system = thread->system_ptr; // For callbacks

#ifdef VERIFICATION
    if (thread->processor_ptr->options->testgen_mode) {
        state_ptr->fp_hmx_debug = proc->arch_proc_options->hmxdebugfile;
        state_ptr->hmx_debug_lvl = proc->arch_proc_options->hmxdebuglvl;
    }
    else {
        state_ptr->fp_hmx_debug = proc->fp_hmx_debug;
        state_ptr->hmx_debug_lvl = proc->ver_hmx_debug_print_level;
    }
#else
    state_ptr->fp_hmx_debug  = proc->arch_proc_options->hmxdebugfile;
	state_ptr->hmx_debug_lvl = proc->arch_proc_options->hmxdebuglvl;
#endif
    state_ptr->lo_msb = 1ll << (proc->arch_proc_options->QDSP6_MX_ACCUM_WIDTH-1);
	state_ptr->lo_mask = state_ptr->lo_msb - 1;

#if 0
    // Switch function pointers to debug if we're debugging to trace files
    if (state_ptr->fp_hmx_debug)
    {
        hmx_mult_body_ptr_table[0] = &hmx_mult_fxp_debug;
        hmx_mult_body_ptr_table[1] = &hmx_mult_fxp_subbyte_debug;
        hmx_mult_body_ptr_table[2] = &hmx_mult_xfp_debug;

        hmx_cvt_body_ptr_table[0] = &hmx_8x8_convert_body_debug;
        hmx_cvt_body_ptr_table[1] = &hmx_8x8_convert_body_debug;
        hmx_cvt_body_ptr_table[2] = &hmx_8x4_convert_body_debug;
        hmx_cvt_body_ptr_table[3] = &hmx_16x8_convert_body_debug;
        hmx_cvt_body_ptr_table[4] = &hmx_xfp_convert_body_debug;
        hmx_cvt_body_ptr_table[5] = &hmx_16x16_convert_body_debug;
    }
#endif

    // FXP Parameters
    state_ptr->QDSP6_MX_CVT_WIDTH    = proc->arch_proc_options->QDSP6_MX_CVT_WIDTH;
	state_ptr->redundant_acc =  (proc->arch_proc_options->QDSP6_MX_SUB_COLS > 1);
    state_ptr->QDSP6_MX_COLS  = proc->arch_proc_options->QDSP6_MX_COLS;
    state_ptr->QDSP6_MX_ROWS = proc->arch_proc_options->QDSP6_MX_ROWS;

    // Floating Point Stuff
    state_ptr->QDSP6_MX_FP_COLS      = proc->arch_proc_options->QDSP6_MX_FP_COLS;
    state_ptr->QDSP6_MX_FP_ROWS      = proc->arch_proc_options->QDSP6_MX_FP_ROWS;
	state_ptr->QDSP6_MX_FP_ACC_NORM  = proc->arch_proc_options->QDSP6_MX_FP_ACC_NORM;
	state_ptr->QDSP6_MX_FP_ACC_INT   = proc->arch_proc_options->QDSP6_MX_FP_ACC_INT;
	state_ptr->QDSP6_MX_FP_ACC_FRAC  = proc->arch_proc_options->QDSP6_MX_FP_ACC_FRAC;
	state_ptr->QDSP6_MX_FP_ACC_EXP   = proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP;
	state_ptr->QDSP6_MX_FP_RATE      = proc->arch_proc_options->QDSP6_MX_FP_RATE;
    state_ptr->QDSP6_MX_RATE         = proc->arch_proc_options->QDSP6_MX_RATE;
	state_ptr->support_bf16 = (proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP >=  8);
	state_ptr->support_fp16 = proc->arch_proc_options->QDSP6_MX_FP_PRESENT;
    state_ptr->QDSP6_MX_PARALLEL_GRPS = proc->arch_proc_options->QDSP6_MX_PARALLEL_GRPS;
    state_ptr->QDSP6_MX_SUB_COLS = proc->arch_proc_options->QDSP6_MX_SUB_COLS;
    state_ptr->pktid = thread->pktid;
}
