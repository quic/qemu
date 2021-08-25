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

#ifndef _HMX_FP_SUPPORT_
#define _HMX_FP_SUPPORT_




#define BIAS_HF 15
#define BIAS_BF 127

#define E_MAX_HF 16
#define E_MAX_BF 127

#define MAX_ACC_WIDTH 66

// Back of two bits on normaliztion
#define ULP_BIT 54
#define ACC_SIGN_BIT 17
#define ACC_TOP_BITS_MASK 0x3FFFF
#define ACC_LOW_BITS_MASK ~(0xFFFF)
#define NORM_MANISSA_SIZE 11
#define MANTISSA_MASK 1023

#define HF_EXP_MASK (0x1F<<10)
#define BF_EXP_MASK (0xFF<<7)

#define SGN_MASK (1<<15)

#define MAX_ACC_FP16_POS 131071
#define MAX_ACC_FP16_NEG -131072

#define FP16_POS_OVF_BIT 0
#define FP16_NEG_OVF_BIT 1

#define FP16_POS_INF 0x7C000
#define FP16_NEG_INF 0xFC000

#define BF16_POS_INF 0x7F800
#define BF16_NEG_INF 0xFF800

#define FP21_POS_INF 0x0F8000
#define FP21_NEG_INF 0x1F8000

#define FP16_POS_ZERO 0x00000
#define FP16_NEG_ZERO 0x80000

// Bfloat Zeros are identical to HF16
#define BF16_POS_ZERO FP16_POS_ZERO
#define BF16_NEG_ZERO FP16_NEG_ZERO

#define FP20_MAX_POS_NOINF 0x7FFFF
#define FP20_MAX_NEG_NOINF 0xFFFFF


#define FP20_MAXNORM_POS 0x7BFFF
#define FP20_MAXNORM_NEG 0xFBFFF
#define BF20_MAXNORM_POS 0x7F7FF
#define BF20_MAXNORM_NEG 0xFF7FF


#define FP16_NAN 0xFFFFF
#define BF16_NAN 0xFFFFF
#define MANTISSA_MASK_FP21 0x7FFF


#define USR_IEEE_INF_NAN(VAL) ((VAL>>0) & 0x1)
#define USR_IEEE_NAN_PROPAGATE(VAL) ((VAL>>1) & 0x1)


typedef struct exp_range {
    int32_t min;
    int32_t max;
} exp_range_t;

typedef struct hmx_fp {
    uint32_t 	exp;               // Exponent
    uint64_t 	frac;              // Fraction
    uint8_t  	sign;              // Sign
    uint32_t 	FRAC;              // Number of fractional bits
    uint32_t 	EXP;               // Number of exponent bits
} hmx_fp_t;


static inline size4s_t is_denorm_fp16(size4u_t in)  { return ((in & HF_EXP_MASK) >> 10) ? 0 : 1 ; }
static inline size4s_t get_exp_fp16(  size4u_t in)  { return ((in & HF_EXP_MASK) >> 10) ? ((in & HF_EXP_MASK) >> 10)-BIAS_HF : 0 ; }
static inline size4s_t get_sign_fp16( size4u_t in)  { return (in & SGN_MASK) >> 15; }
static inline size4s_t get_signif_fp16(size4u_t in) { return (in & (MANTISSA_MASK)) | (( in & HF_EXP_MASK ) ?  (1<<10) : 0); }
static inline size4s_t is_pos_inf_fp21(size4s_t in) { return FP21_POS_INF == in; }
static inline size4s_t is_neg_inf_fp21(size4s_t in) { return FP21_NEG_INF == in; }
static inline size4s_t is_nan_fp21(size4s_t in)     { return 3*(((( (in>>15) & 0x1F)) == 0x1F) && ((in & (MANTISSA_MASK_FP21)) != 0 )); }

static inline size4s_t is_inf_fp16(size4u_t in )    { return (FP16_NEG_INF  == in) || (FP16_POS_INF  == in); }
static inline size4s_t is_zero_fp16(size4u_t in)    { return (FP16_NEG_ZERO == in) || (FP16_POS_ZERO == in); }
static inline size4s_t is_nan_fp16(size4u_t in )    { return (((in & HF_EXP_MASK) >> 10) == 0x1F) && ((in & (MANTISSA_MASK)) != 0 ); }





#endif
