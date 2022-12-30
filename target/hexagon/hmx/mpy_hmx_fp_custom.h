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

#ifndef _MPY_HMX_FP_CUSTOM_H_
#define _MPY_HMX_FP_CUSTOM_H_

#include "int16_emu.h"
#include "mpy_hmx_support.h"

#ifndef ARCH_FUNCTION_DECL
#define ARCH_FUNCTION_DECL(rval, fname, ...) \
rval fname(__VA_ARGS__);\
rval fname##_##debug(__VA_ARGS__);
#endif

#include "mpy_hmx_support.h"

#define FULL "fully"
#define NORM "normal"
#define SUBN "subnormal"
#define OVER "infnan"
#define TWOP "exact"
#define OPPO "opposite"
#define BF "bf16"
#define FP "fp16"

#ifdef STANDALONE

// Data structures stubbed from simulator so we can compile standalone without simulator

typedef struct arch_proc_opt {
    int hmx_mxmem_debug;
    int QDSP6_MX_FP_ACC_FRAC;
    int QDSP6_MX_FP_ACC_INT;
    int QDSP6_MX_FP_PRESENT;
    int QDSP6_MX_FP_ACC_EXP;
    int QDSP6_MX_FP_RATE;
    int QDSP6_MX_FP_ACC_NORM;
    int xfp_inexact_enable;
    int xfp_cvt_frac;
    int xfp_cvt_int;

} arch_proc_opt_t;

typedef struct th {
        int tmp;
} thread_t;

typedef struct ProcessorState {
    thread_t * thread[1];
    arch_proc_opt_t * arch_proc_options;
} processor_t;


typedef struct ProcessorState {
    thread_t * thread[1];
    arch_proc_opt_t * arch_proc_options;
} hmx_state_t;


#else
// This is included in the simulator buil


struct ThreadState;
struct HMX_State;
union hmx_bias;
union hmx_cvt_rs_reg;
union usr_fp_reg;
struct hmx_xfp;
struct hmx_bias_flt_poly;

#ifndef ARCH_FUNCTION_DECL
#define ARCH_FUNCTION_DECL(rval, fname, ...) \
rval fname(__VA_ARGS__);
rval fname##_##debug(__VA_ARGS__);
#endif


#endif



// Internal XFP Functions
ARCH_FUNCTION_DECL(hmx_fp_t,       fp_to_hmx_fp,            struct HMX_State * state_ptr,                       uint32_t in,         uint32_t exp,      uint32_t frac);
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_fp_xfp,              struct HMX_State * state_ptr, union usr_fp_reg usr, uint32_t in,         uint32_t frac_in,  uint32_t exp_in,   uint32_t int_out, uint32_t frac_out, uint32_t exp_out, uint32_t normalize);
ARCH_FUNCTION_DECL(uint32_t,       hmx_xfp_fp,              struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp in,   uint32_t fp_frac,  uint32_t fp_exp, const union hmx_cvt_rs_reg rs);
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_xfp_mult,            struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp in_a, struct hmx_xfp in_b,    uint32_t exp_out);
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_xfp_add,             struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp in_a, struct hmx_xfp in_b);
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_xfp_cvt_normalize,   struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp in,   uint32_t int_out,  uint32_t frac_out, int32_t exp_out );
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_xfp_normalize,       struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp in,   uint32_t int_out,  uint32_t frac_out, int32_t exp_out, uint32_t usa_lza);
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_xfp_add_Nway_2stage, struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp * in, struct hmx_xfp acc,     uint32_t N,        uint32_t extra_adder_bit);



// Simulator Interfaces
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_fp_xfp_mult,         struct HMX_State * state_ptr, union usr_fp_reg usr, uint16_t A,  uint16_t B, uint32_t is_bf16);
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_xfp_mac_reduce,      struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp * A, struct hmx_xfp acc, uint32_t rate);
ARCH_FUNCTION_DECL(uint32_t,       hmx_xfp_cvt,             struct HMX_State * state_ptr, union usr_fp_reg usr, struct hmx_xfp acc, struct hmx_bias_flt_poly bias_reg, uint32_t cvt_feedback, union hmx_cvt_rs_reg rs);
ARCH_FUNCTION_DECL(struct hmx_xfp, hmx_xfp_zero,            struct HMX_State * state_ptr);
ARCH_FUNCTION_DECL(size16s_t,      hmx_xfp_to_tb_callback,  struct HMX_State * state_ptr, int32_t * exponent, uint32_t * ovf, struct hmx_xfp acc);


#endif
