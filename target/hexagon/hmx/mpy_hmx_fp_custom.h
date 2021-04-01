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

#define FULL "fully"
#define NORM "normal"
#define SUBN "subnormal"
#define OVER "infnan"
#define TWOP "exact"
#define OPPO "opposite"
#define BF "bf16"
#define FP "fp16"

#define HMX_FUNCTION_DECL(rval, fname, ...) \
rval fname(__VA_ARGS__);



// Internal XFP Functions
HMX_FUNCTION_DECL(hmx_fp_t,  fp_to_hmx_fp,            processor_t *proc,                   uint32_t in,    uint32_t exp,      uint32_t frac);
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_fp_xfp,              processor_t *proc, usr_fp_reg_t usr, uint32_t in,    uint32_t frac_in,  uint32_t exp_in,   uint32_t int_out, uint32_t frac_out, uint32_t exp_out, uint32_t normalize);
HMX_FUNCTION_DECL(uint32_t,  hmx_xfp_fp,              processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t in,   uint32_t fp_frac,  uint32_t fp_exp, const hmx_cvt_rs_reg_t rs, usr_fp_reg_t usr_tmp_remove_me);
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_xfp_mult,            processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t in_a, hmx_xfp_t in_b,    uint32_t exp_out);
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_xfp_add,             processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t in_a, hmx_xfp_t in_b);
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_xfp_cvt_normalize,   processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t in,   uint32_t int_out,  uint32_t frac_out, int32_t exp_out );
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_xfp_normalize,       processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t in,   uint32_t int_out,  uint32_t frac_out, int32_t exp_out, uint32_t usa_lza);
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_xfp_add_Nway_2stage, processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t *	in, hmx_xfp_t acc,     uint32_t N,        uint32_t extra_adder_bit);


// Simulator Interfaces
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_fp_xfp_mult,       processor_t *proc, usr_fp_reg_t usr, uint16_t A,  uint16_t B, uint32_t is_bf16);
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_xfp_mac_reduce,    processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t * A, hmx_xfp_t acc, uint32_t rate);
HMX_FUNCTION_DECL(uint32_t,  hmx_xfp_cvt,           processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t acc, hmx_bias_flt_poly_t bias_reg, uint32_t cvt_feedback, hmx_cvt_rs_reg_t rs);
HMX_FUNCTION_DECL(uint32_t,  hmx_xfp_cvt_debug,     processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t acc, hmx_bias_flt_poly_t bias_reg, uint32_t cvt_feedback, hmx_cvt_rs_reg_t rs);
HMX_FUNCTION_DECL(hmx_xfp_t, hmx_xfp_zero,          processor_t *proc);
HMX_FUNCTION_DECL(size16s_t, hmx_xfp_to_tb_callback,processor_t *proc, int32_t * exponent, uint32_t * ovf, hmx_xfp_t acc);


#endif
