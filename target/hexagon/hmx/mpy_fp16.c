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
#include <math.h>
#include "mpy_fp16.h"


double hmx_fp16_float(size2u_t in) {
    double out = 0;
    size4s_t signif_in = get_signif_fp16(in);
    size4s_t sign_in = get_sign_fp16(in);
    size4s_t exp_in = (in & HF_EXP_MASK) >> 10;
    if (exp_in) {
        exp_in -= 15;
    } else {
        exp_in = -16;
    }

    out =  ((double)signif_in / 1024.0f) * pow(2.0f, exp_in);

    return (sign_in) ? -out : out;
}

double hmx_acc_double(size16s_t acc) {
    double out = 0;
    int sgn = 0;
    if (acc.hi < 0) {
        size16s_t lsb = { .hi= 0, .lo = 1};
        acc.lo = ~acc.lo;
        acc.hi = ~acc.hi;
        acc = add128(acc, lsb);   // Convert to 2's comp
        sgn = 1;
    }

    size8s_t integer = acc.hi;
    size8u_t fractional = acc.lo >> 16;
    double d = 281474976710655.0;
    out =  ((double) integer) +  ((double)fractional / d );
    out = (sgn) ? -out : out;
    return out;
}


double hmx_acc_float(size16s_t acc, int sgn);
double hmx_acc_float(size16s_t acc, int sgn) {
    double out = 0;
    size8s_t integer = acc.hi;
    size8u_t fractional = acc.lo >> 16;
    //printf("%llx %llx\n", integer, fractional);
    double d = 281474976710655.0;
    out =  ((double) integer) +  ((double)fractional / d );
    out = (sgn) ? -out : out;
 //   printf("%f %f %f\n", (double)integer, (double)fractional / d, out);
    return out;
}




#ifdef STANDALONE
#define DEBUG_PRINT(...) \
    if (proc->arch_proc_options->hmx_mxmem_debug >= 2) {\
        printf(__VA_ARGS__);\
        printf("\n");\
    }
#define DEBUG_PRINT_ALWAYS(...) \
    if (proc->arch_proc_options->hmx_mxmem_debug >= 2) {\
        printf(__VA_ARGS__);\
        printf("\n");\
    }
#else
#define DEBUG_PRINT(...)
#endif



size16s_t hmx_rnd( processor_t *proc, size16s_t in, size8s_t lsb_bit);
size16s_t hmx_rnd( processor_t *proc, size16s_t in, size8s_t lsb_bit){
    size16s_t out = {0};

    if(lsb_bit)  {
        size16s_t sticky_add = { .hi= 0, .lo = 0 };
        size16s_t rnd_add = { .hi= 0, .lo = 0 };
        size8s_t rnd_bit = lsb_bit - 1;

        if (lsb_bit < 64) {
            sticky_add.lo = (1ll << (rnd_bit)) - 1;
            rnd_add.lo = (in.lo >> lsb_bit) & 0x1;
        } else {
            rnd_bit -= 64;
            sticky_add.hi = (1ll << rnd_bit) - 1;
            sticky_add.lo = 0xFFFFFFFFFFFFFFFFll;
            rnd_add.hi = (in.hi >> rnd_bit) & 0x1;
        }
        DEBUG_PRINT("in:          %016llx %016llx  ", in.hi, in.lo);
        DEBUG_PRINT("sticky_add:  %016llx %016llx  ", sticky_add.hi, sticky_add.lo);
        DEBUG_PRINT("rnd_add:     %016llx %016llx  ", rnd_add.hi, rnd_add.lo);
        out = add128(in, sticky_add);           // Add .499999
        DEBUG_PRINT("hmx_rnd:     %016llx %016llx  ", out.hi, out.lo);
        out = add128(out, rnd_add);             // Add .000001 if odd
        DEBUG_PRINT("hmx_rnd:     %016llx %016llx  ", out.hi, out.lo);
        out = shiftl128(shiftr128(out, lsb_bit), lsb_bit);
        DEBUG_PRINT("hmx_rnd:     %016llx %016llx  ", out.hi, out.lo);
    } else {
        out = in;
        DEBUG_PRINT("no hmx_rnd:  %016llx %016llx  ", out.hi, out.lo);
    }
    return out;
}

void hmx_fp16_acc_ovf_check(processor_t *proc, size16s_t in, size1s_t * ovf, size1s_t fp_rnd_bits)
{
   if( (in.hi > MAX_ACC_FP16_POS) || (in.hi < MAX_ACC_FP16_NEG ) ) {
        if (*ovf == 0) {    // No overflow has been detected yet
            *ovf |= (in.hi < 0) ? 2 : 1; // Negative or Positive overflow
            DEBUG_PRINT("Accumulator overflow top bits: %016llx ovf=%x", in.hi, *ovf);
        } else {
            DEBUG_PRINT("Accumulator overflow top bits, but ovf already recorded previously: %016llx previes ovf=%x new=%x (not recorded)", in.hi, *ovf, (in.hi < 0) ? 2 : 1);
        }
    }
}

size16s_t
convert_to_2c(size16s_t in, int sgn) {
    size16s_t lsb = { .hi= 0, .lo = 1 };
    if(sgn) {
        in.lo = ~in.lo;
        in.hi = ~in.hi;
        in = add128(in, lsb);   // Convert to 2's comp
    }
    return in;
}


size16s_t
convert_to_1c(size16s_t in, int sgn) {
    if(sgn) {
        in.lo = ~in.lo;
        in.hi = ~in.hi;
    }
    return in;
}

static inline size4s_t sat_to_pos(processor_t *proc, int sat, int propagate_nan, int result) {
    if (sat) {
        if (propagate_nan) {
            if (is_nan_fp16(result)) {
                DEBUG_PRINT("pos sat: propagating NaN result=%x", (int)result);
            } else if (get_sign_fp16(result)) {
                DEBUG_PRINT("pos sat: clipping negative to zero result=%x", (int)result);
                result = 0;
            }
        } else {
            if (get_sign_fp16(result)) {
                DEBUG_PRINT("pos sat: clipping negative to zero result=%x", (int)result);
                result = 0;
            }
        }
    }
    return result;
}


size2u_t hmx_fp16_cvt(processor_t *proc, size16s_t acc, size1s_t ovf,  size1s_t fp_rnd_bits, size4s_t bias_raw, size2s_t exp_scale, size2u_t exp_sat, size4s_t sat, size4s_t hi, size4s_t lo ) {

    size2u_t result = 0;
    if(proc->arch_proc_options->QDSP6_MX_FP_PRESENT == 0){
        return result;
    }
    size4s_t exponent = 0;
    size2u_t mantissa = 0;
    size4s_t sgn = 0;
    size16s_t bias = { .hi= 0, .lo = 0 };
    size4s_t bias_exp = (bias_raw >> 15) & 0x1F;
    size4s_t bias_sgn = (bias_raw >> 20) & 0x1;
    size16s_t epsilon_var = { .hi= 0, .lo = 1 };
    size16s_t epsilon_var_out = { .hi= 0, .lo = (1ll << ULP_BIT) };
    size2u_t use_1c = hi | lo | 0;

    size4s_t max_exp_is_inf = USR_IEEE_INF_NAN(fp_rnd_bits);  // Max exponent represents inf/nan
    size4s_t propagate_nan = USR_IEEE_NAN_PROPAGATE(fp_rnd_bits) && max_exp_is_inf;  // Max exponent represents inf/nan

    size1s_t bias_ovf = (max_exp_is_inf) ? ((is_nan_fp21(bias_raw)) | is_pos_inf_fp21(bias_raw) | (is_neg_inf_fp21(bias_raw)<<1)) : 0;

    if (ovf || bias_ovf) {

        size4s_t max_exponent = 31;
        size4s_t mantissa = 0x3FF;

        if ((max_exp_is_inf == 0) || (exp_sat != 0)) {        // Saturate to finite value
            max_exponent -= exp_sat;
            ovf |= bias_ovf;
        } else if (max_exp_is_inf) {
            ovf |= bias_ovf;
            mantissa = (ovf == 3) ? mantissa : 0;      // Simplest case. Only one is overflowing so saturate to that
        }

        if (propagate_nan && (ovf==0x3)) {
            max_exponent = 31;
            mantissa = 0x3FF;
        }

        size4s_t overflow_val =  mantissa | ((max_exponent << 10) & HF_EXP_MASK) | ((((ovf >> FP16_NEG_OVF_BIT) & 0x1) << 15) & SGN_MASK);
        overflow_val = sat_to_pos(proc, sat, propagate_nan, overflow_val);
        DEBUG_PRINT("CVT Overflow USR bits=%d: overflow_val=%x max_exponent=%d ovf=%x bias_ovf=%x max_exp_is_inf=%d propagate_nan=%d bias_raw=%x exp_sat=%x", fp_rnd_bits, (int)overflow_val, (int)max_exponent, (int)ovf, (int)bias_ovf, max_exp_is_inf, propagate_nan, bias_raw, exp_sat);
        return overflow_val;
    }

    // Sign Extension
    exp_scale <<= 10;
    exp_scale >>= 10;

    if (exp_scale == -32)
        exp_scale = -31;


    // Drop top bits
    if(acc.hi & ~ACC_TOP_BITS_MASK) {
        DEBUG_PRINT("Acc Overflow top bits: %016llx", acc.hi);
    }
    acc.hi &= ACC_TOP_BITS_MASK;


    DEBUG_PRINT("acc_in: %016llx.%016llx  exp_scale=%d exp_sat=%d hi=%d lo=%d bias_exp=%d" , acc.hi, acc.lo, exp_scale, exp_sat, hi, lo, bias_exp);

    bias.lo = bias_raw & MANTISSA_MASK_FP21;
    if (bias_exp) {
        bias_exp += (64-15-15) ;         // Align with bit 48 for Q17.~ format
        bias.lo |=  0x8000;
    } else {
       bias_exp += (64-15-14) ;         // Align with bit 48 for Q17.~ format - denorm case
    }


    bias = shiftl128(bias, bias_exp);
    DEBUG_PRINT("bias:  %016llx.%016llx %f bias_raw=%x bias_sgn=%d bias_exp=%d", bias.hi, bias.lo, hmx_acc_float(bias, bias_sgn), bias_raw, bias_sgn, bias_exp );

    bias = convert_to_2c(bias, bias_sgn);
    acc = add128(acc, bias);
    DEBUG_PRINT("biased: %016llx.%016llx  %f sgn=%d ", acc.hi, acc.lo, hmx_acc_float(acc, sgn), sgn);


    if(acc.hi & ~ACC_TOP_BITS_MASK) {
        DEBUG_PRINT("Acc Overflow top bits after bias: %016llx", acc.hi);
    }
    acc.hi &= ACC_TOP_BITS_MASK;



    sgn = ( (acc.hi >> ACC_SIGN_BIT) & 0x1 );   // Check sign bit (bit 18)

    if (use_1c) {
        acc = convert_to_1c(acc, sgn);
        DEBUG_PRINT("acc 1c: %016llx.%016llx sgn=%d", acc.hi, acc.lo, sgn);
    } else {
        acc = convert_to_2c(acc, sgn);
        DEBUG_PRINT("acc 2c: %016llx.%016llx sgn=%d", acc.hi, acc.lo, sgn);
    }
    acc.hi &= ACC_TOP_BITS_MASK;


    size4s_t clz = count_leading_zeros_16(acc) - (64-17);  // Max accumulator width
    size4s_t max_shift = 30 + exp_scale;
    size4s_t shift = (clz <= max_shift) ? clz : max_shift;
    size4s_t denorm = (clz > max_shift);
    size16s_t normalized  = { .hi= 0, .lo = 0 };

    if (shift < 0 ) {
        normalized  = shiftr128(shiftr128(acc, -shift+lo*11),16);
    } else {
        int norm_shift = shift+lo*11;

        if (use_1c) {
            unsigned long long int lo = acc.lo & 0x1;
            acc = shiftl128(acc, norm_shift);
            if (lo) {
                acc.lo = acc.lo | ((1 << norm_shift)-1);
            }
        } else {
            acc = shiftl128(acc, norm_shift);
        }
        //DEBUG_PRINT("shiftl: %016llx.%016llx sgn=%d norm_shift=%d", acc.hi, acc.lo, sgn, norm_shift);
        normalized  = shiftr128(acc,16);
    }
    normalized.hi &= 1; // Drop bits above 1.0

    DEBUG_PRINT("clz: clz=%d max_shift=%d shift=%d denorm=%d  ", clz, max_shift, shift, denorm);

    DEBUG_PRINT("normalized: %016llx.%016llx exp=%d ", normalized.hi, normalized.lo, exponent);

    if (lo && (normalized.hi == 0)) {
        size16s_t two = { .hi= 0, .lo = 2};
        normalized = sub128(two, normalized);
        normalized = sub128(normalized, epsilon_var);
        DEBUG_PRINT("lo: %016llx.%016llx ", normalized.hi, normalized.lo);
        sgn = !sgn;
    }



    size16s_t mantissa_64  = { .hi= 0, .lo = 0 };


    if (hi) {
        if (( (normalized.lo >> (ULP_BIT-1)) & 0x1 ) == 0) {
            mantissa_64 = add128(normalized, epsilon_var_out);
            DEBUG_PRINT("ulp added:  %016llx.%016llx  ", mantissa_64.hi, mantissa_64.lo);
        } else {
            mantissa_64 = normalized;
        }
    } else {
        if (sgn && use_1c) {
            normalized = add128(normalized, epsilon_var);
            DEBUG_PRINT("add eps :  %016llx.%016llx  ", normalized.hi, normalized.lo);
        }
       DEBUG_PRINT("rnd in :  %016llx %016llx  ", normalized.hi, normalized.lo);
       if (((normalized.lo >> ULP_BIT) & 0x1)==0) {
            normalized = sub128(normalized, epsilon_var);
            DEBUG_PRINT("eps in :  %016llx.%016llx  ", epsilon_var.hi, epsilon_var.lo);
            DEBUG_PRINT("nor in :  %016llx.%016llx  ", normalized.hi, normalized.lo);
       }
       epsilon_var_out.lo >>=1;
       mantissa_64 = add128(normalized, epsilon_var_out);
       DEBUG_PRINT("eps out :  %016llx.%016llx  ", epsilon_var_out.hi, epsilon_var_out.lo);
       DEBUG_PRINT("rnd out :  %016llx.%016llx  ", mantissa_64.hi, mantissa_64.lo);
    }


    mantissa_64 = shiftr128(mantissa_64, ULP_BIT);
    mantissa =  mantissa_64.lo & 0x3FF;

    DEBUG_PRINT("extracted mantissa %x %llx ULP_BIT=%d", (int)mantissa, mantissa_64.lo, ULP_BIT);

    exponent = (denorm) ? 0 : (31 - shift + exp_scale);

    if (mantissa_64.lo & (1ll << 11)) { // Overflow case
        exponent += 1;
        DEBUG_PRINT("overflowed exponent=%d ", exponent);
    } else if (denorm && (mantissa_64.lo & (1ll << 10))  ) {
        exponent = 1;
        DEBUG_PRINT("overflowed exponent for denorm case=%d ", exponent);
    }

    size4s_t max_exponent = 31 - exp_sat;
    if ( exponent > max_exponent) {
        exponent = max_exponent;
        //mantissa = 0x3FF;
        mantissa = (max_exp_is_inf && (exp_sat==0)) ? 0 : 0x3FF;
        DEBUG_PRINT("overflowed exponent=%d mantissa=%x usr=%d", exponent, mantissa, fp_rnd_bits);
    }

    if (max_exp_is_inf && (exp_sat==0) && (exponent==31)) {
        mantissa = 0;
        DEBUG_PRINT("result is infinity exponent=%d mantissa=%x usr=%d", exponent, mantissa, fp_rnd_bits);
    }

    result = mantissa | ((exponent << 10) & HF_EXP_MASK) | ((sgn << 15) & SGN_MASK);

    result = sat_to_pos(proc, sat, 0, result);  // Cannot propagate a NaN here, because a NaN cannot get here.

    DEBUG_PRINT("result: result=%x exponent=%d mantissa=%x", (int)result, (int)exponent, (int)mantissa);

    return result;
}


// static size2u_t sat_to_max(processor_t *proc, size8s_t in, size4s_t element_size, size4s_t sat) {
//     size8s_t max_element = (1 << (element_size*8)) - 1;
//     size2u_t out = 0;
//     if (sat) {
//         if (in < 0) {
//             out = 0;
//         } else if (in > max_element) {
//             out = max_element;
//         } else {
//             out = (in & max_element);
//         }
//     } else {
//        out = (in & max_element);
//     }
//     DEBUG_PRINT("in = %llx out=%04x (sat=%d)", in, out, sat);
//     return out;
// }

static size4u_t sat_to_max_v2(processor_t *proc, size8s_t in, size4s_t element_size, size4s_t sat) {
    int convert_width = proc->arch_proc_options->QDSP6_MX_CVT_WIDTH;
    size8s_t max_element = (1 << (element_size*convert_width)) - 1;
    size4u_t out = 0;
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
    DEBUG_PRINT("in = %llx out=%02x.%01x (sat=%d)", in, (out>>4), (out & 0xF), sat);    // Might need to get fixed for elements bigger than byte
    return out;
}

static inline size8s_t acc_shift(processor_t *proc, size8s_t acc_biased, size4s_t exp, size4s_t sat, size4s_t frac_bits, size4s_t int_bits) {
    size4s_t shift_acc = (32-frac_bits);
    size8s_t acc_shifted = (acc_biased << exp);
    size8s_t mask =((long long int)((1llu << (frac_bits + int_bits)) - 1) ) << shift_acc;
    if (sat) {
        mask |= ((long long int)((1llu << 32) - 1) ) << 32;
    }
    acc_shifted &= mask ;  // Clip only fractional bits

    DEBUG_PRINT("mask:         %08x.%08x (Q32.32)", (size4s_t)(mask   >> 32), (size4u_t)mask);
    DEBUG_PRINT("shifted:      %08x.%08x (Q32.%d) exp=%d" , (size4u_t)(acc_shifted  >> 32), (size4u_t)acc_shifted, frac_bits, exp);

    return acc_shifted;
}

static inline size8s_t acc_rectify(processor_t *proc, size8s_t acc_shifted, size2s_t zeroing, size2s_t legacy, size8s_t acc_biased, size2u_t element_size) {
    size8s_t acc_rectified = 0;
    size8s_t summarize_output = 0;
    size8s_t sign_bit = 0;
    size2u_t frac_bits = element_size * 12;
    acc_shifted = acc_shifted;
    size8s_t summarization_bits = acc_shifted & 0xFFFFFFFE00000000ll;

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

    size8s_t maskbits = ((1ll << (frac_bits+1)) - 1) << (32 - frac_bits);
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
    DEBUG_PRINT("acc_shifted_v2:%08x.%08x (Q4.%d)" , (size4u_t)(acc_shifted  >> 32), (size4u_t)acc_shifted, frac_bits);


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

    DEBUG_PRINT("acc_rectified:%08x.%08x (Q4.%d) zeroing=%d" , (size4u_t)(acc_rectified  >> 32), (size4u_t)acc_rectified, frac_bits, zeroing);

    return acc_rectified;
}

static inline size16s_t acc_scale(processor_t *proc, size8s_t acc_rectified, size8s_t scale_cvt) {
    size16s_t acc_scaled = mult64_to_128(scale_cvt, acc_rectified);

    DEBUG_PRINT("scale:        %08x.%08x (Q32.10)", (size4s_t)(scale_cvt   >> 32), (size4u_t)scale_cvt);
    DEBUG_PRINT("acc_scaled:   %016llx.%016llx (Q64.64)", (size8u_t)(acc_scaled.hi), (size8u_t)acc_scaled.lo);

    return acc_scaled;
}

static inline size8s_t acc_v3_bias(processor_t *proc, size16s_t acc_scaled, size4s_t element_size, size4u_t rnd_bit, size4s_t frac_bits){
    size8s_t ulp_bit = 64 - 8 * element_size - 3 - 1;
    size16s_t ulp;
    size16s_t acc_rnd;

    ulp = cast8s_to_16s( (size8s_t)rnd_bit << ulp_bit);
    ulp.hi = 0;
    acc_scaled = add128(acc_scaled, acc_scaled);
    acc_rnd = add128(acc_scaled, ulp); // + ULP
    ulp_bit += 3;
    int convert_width = proc->arch_proc_options->QDSP6_MX_CVT_WIDTH;

    size8s_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1-((convert_width - 8)*element_size)))); // Q31.16

    DEBUG_PRINT("ulp:          %016llx.%016llx (Q64.64) ulp_bit=%lld vrnd_bit=%x", (size8u_t)(ulp.hi), (size8u_t)ulp.lo, ulp_bit, rnd_bit);
    DEBUG_PRINT("acc_rnd:      %016llx.%016llx (Q64.64)", (size8u_t)(acc_rnd.hi), (size8u_t)acc_rnd.lo);
    DEBUG_PRINT("acc_final:    %08x.%08x (Q32.%d)", (size4u_t)(acc_final    >> (element_size*convert_width)), (size4u_t)acc_final<<(32-(element_size*convert_width)), frac_bits );

    return acc_final;
}

static inline size8s_t acc_v2_bias(processor_t *proc, size16s_t acc_scaled, size4s_t element_size, size4u_t rnd_bit, size4s_t frac_bits){
    size8s_t ulp_bit = 64 - 8 * element_size - 3 - 1;
    size16s_t ulp;
    size16s_t acc_rnd;

    ulp = cast8s_to_16s( (size8s_t)rnd_bit << ulp_bit);
    ulp.hi = 0;
    acc_scaled = add128(acc_scaled, acc_scaled);
    acc_rnd = add128(acc_scaled, ulp); // + ULP
    ulp_bit += 3;
    size8s_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1))); // Q31.16

    DEBUG_PRINT("ulp:          %016llx.%016llx (Q64.64) ulp_bit=%lld vrnd_bit=%x", (size8u_t)(ulp.hi), (size8u_t)ulp.lo, ulp_bit, rnd_bit);
    DEBUG_PRINT("acc_rnd:      %016llx.%016llx (Q64.64)", (size8u_t)(acc_rnd.hi), (size8u_t)acc_rnd.lo);
    DEBUG_PRINT("acc_final:    %08x.%08x (Q32.%d)", (size4u_t)(acc_final    >> (ulp_bit+1)), (size4u_t)acc_final<<(ulp_bit+1), frac_bits );

    return acc_final;
}

static inline size8s_t acc_rnd(processor_t *proc, size16s_t acc_scaled, size4s_t element_size, size4s_t rnd_bit, size4s_t frac_bits){
    int convert_width = proc->arch_proc_options->QDSP6_MX_CVT_WIDTH;
    size8s_t ulp_bit = 64 - 8 * element_size - 1;
    size16s_t ulp;
    size16s_t acc_rnd;


    ulp = cast8s_to_16s( (size8s_t)rnd_bit << ulp_bit);
    acc_scaled = add128(acc_scaled, acc_scaled);
    acc_rnd = add128(acc_scaled, ulp); // + ULP
    size8s_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1)-((convert_width - 8)*element_size))); // Q31.24

    DEBUG_PRINT("acc_rnd:      %016llx.%016llx (Q64.64) ulp_bit=%lld vrnd_bit=%x", (size8u_t)(acc_rnd.hi), (size8u_t)acc_rnd.lo, ulp_bit, rnd_bit);
    DEBUG_PRINT("acc_final:    %08x.%08x (Q32.%d)", (size4u_t)(acc_final    >> (ulp_bit+1)), (size4u_t)acc_final<<(ulp_bit+1), frac_bits );

    return acc_final;
}

static inline size8s_t acc_v1_bias(processor_t *proc, size16s_t acc_scaled, size4s_t element_size, size4s_t rnd_bit, size4s_t frac_bits){
    size8s_t ulp_bit = 64 - 8 * element_size - 1;
    size16s_t ulp;
    size16s_t acc_rnd;

        ulp_bit = 64 - 16 + 64;
        rnd_bit =(size4s_t) ((size2s_t)rnd_bit);
        ulp = shiftr128(shiftl128(cast8s_to_16s((size8s_t)rnd_bit), ulp_bit), 63);
        acc_rnd = add128(acc_scaled, ulp); // + ULP

        ulp_bit = 64 -8 -1;
    size8s_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1))); // Q31.16

    DEBUG_PRINT("acc_rnd:      %016llx.%016llx (Q64.64) ulp_bit=%lld vrnd_bit=%x", (size8u_t)(acc_rnd.hi), (size8u_t)acc_rnd.lo, ulp_bit, rnd_bit);
    DEBUG_PRINT("acc_final:    %08x.%08x (Q32.%d)", (size4u_t)(acc_final    >> (ulp_bit+1)), (size4u_t)acc_final<<(ulp_bit+1), frac_bits );

    return acc_final;
    }

// static size8s_t acc_shift_mult(processor_t *proc, size8s_t acc_biased, size4s_t exp, size4s_t sig, size4s_t sat, size4s_t element_size,size4s_t rnd_bit, size4s_t frac_bits) {

//     size8s_t scale_cvt = ((size8s_t)sig | 0x400) << 22;                   // Q1.10
//     size4s_t shift_acc = (32-frac_bits);
//     size8s_t acc_shifted = (acc_biased << exp);        // shift to Q31.32 and then to Q31.frac


//     size8s_t mask =((long long int)((1llu << frac_bits) - 1) ) << shift_acc;
//     if (sat) {
//         mask |= ((long long int)((1llu << 32) - 1) ) << 32;
//     }
//     acc_shifted &= mask ;  // Clip only fractional bits

//     size16s_t acc_scaled = mult64_to_128(scale_cvt, acc_shifted);
//     size8s_t ulp_bit = 64 - 8*element_size - 1 ;

//     size16s_t ulp;
//     size16s_t acc_rnd;

//     if (frac_bits == 10) {

//         ulp_bit = 64 - 16 + 64;
//         rnd_bit =(size4s_t) ((size2s_t)rnd_bit);
//         ulp = shiftr128(shiftl128(cast8s_to_16s((size8s_t)rnd_bit), ulp_bit), 63);
//         acc_rnd = add128(acc_scaled, ulp); // + ULP

//         ulp_bit = 64 -8 -1;
//     }

//     else {
//         ulp = cast8s_to_16s( (size8s_t)rnd_bit << ulp_bit);
//         acc_rnd = add128(acc_scaled, ulp); // + ULP
//     }



//     size8s_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1))); // Q31.16




//     DEBUG_PRINT("mask:         %08x.%08x (Q32.32)", (size4s_t)(mask   >> 32), (size4u_t)mask);
//     DEBUG_PRINT("shifted:      %08x.%08x (Q32.%d) exp=%d" , (size4u_t)(acc_shifted  >> 32), (size4u_t)acc_shifted, frac_bits, exp);
//     DEBUG_PRINT("scale:        %08x.%08x (Q32.10)", (size4s_t)(scale_cvt   >> 32), (size4u_t)scale_cvt);
//     DEBUG_PRINT("acc_scaled:   %016llx.%016llx (Q64.64)", (size8u_t)(acc_scaled.hi), (size8u_t)acc_scaled.lo);
//     DEBUG_PRINT("acc_rnd:      %016llx.%016llx (Q64.64) ulp_bit=%lld vrnd_bit=%x", (size8u_t)(acc_rnd.hi), (size8u_t)acc_rnd.lo, ulp_bit, rnd_bit);
//     DEBUG_PRINT("acc_final:    %08x.%08x (Q32.%d)", (size4u_t)(acc_final    >> (ulp_bit+1)), (size4u_t)acc_final<<(ulp_bit+1), frac_bits );
//     //printf("acc_final %llx\n", acc_final);
//     return acc_final;
// }

size4u_t hmx_u8_cvt(processor_t *proc, size4s_t acc, size4s_t bias32, size2s_t exp, size2s_t zeroing, size2s_t sig, size2u_t out_bias, size4s_t sat, size4s_t frac_bits, size2s_t legacy){

    size4s_t element_size = 1;

    frac_bits = (frac_bits == 0) ? 12 : 10; // V1 or V2 behavior

    size8s_t bias32_add = (size8s_t)bias32;
    size8s_t acc_cvt =    (size8s_t)acc;
    size8s_t acc_biased  = (acc_cvt + bias32_add);

    DEBUG_PRINT("bias32:       %08x.%08x (Q32.32)", (size4s_t)(bias32_add   >> 32), (size4u_t)bias32_add);
    DEBUG_PRINT("acc_cvt:      %08x.%08x (Q32.32)", (size4u_t)(acc_cvt      >> 32), (size4u_t)acc_cvt);
    DEBUG_PRINT("acc_biased:   %08x.%08x (Q32.31)", (size4u_t)(acc_biased   >> 32), (size4u_t)acc_biased);


    size8s_t scale_cvt = ((size8s_t)sig) << 20;                   // Q0.12

    size4s_t int_bits = 32;
    size8s_t acc_shifted = acc_shift(proc, acc_biased, exp, sat, frac_bits, int_bits);
    size8s_t acc_rectified = acc_rectify(proc, acc_shifted, zeroing, legacy, acc_biased, element_size);
    size16s_t acc_scaled = acc_scale(proc, acc_rectified, scale_cvt);
    size8s_t acc_final;
    if(frac_bits == 10){
        acc_final = acc_v1_bias(proc, acc_scaled, element_size, out_bias, frac_bits);
    }
    else{
        acc_final = acc_v3_bias(proc, acc_scaled, element_size, out_bias, frac_bits); //left shift to get proper decimal location
    }

    // size8s_t acc_final = acc_shift_mult(proc, acc_biased, exp, sig, sat, element_size, out_bias, frac_bits);

    return sat_to_max_v2(proc, acc_final,  element_size, sat);
}



size4u_t hmx_u16_cvt(processor_t *proc, size4s_t acc_hl, size4s_t acc_ll, size4s_t bias32, size2s_t exp, size2s_t zeroing, size2s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits, size2s_t legacy) {
    size4s_t element_size = 2;
    frac_bits = element_size*12;    // Force to 12

    size8s_t bias32_add = (size8s_t)bias32; // sign extend from Q0.31 -> Q31.32
    size8s_t acc_ll_cvt = (size8s_t)acc_ll; // sign extend from Q0.31 -> Q31.32
    size8s_t acc_hl_cvt = (size8s_t)acc_hl; // sign extend from Q0.31 -> Q31.32
    size8s_t acc_combined = acc_hl_cvt + (acc_ll_cvt >> 8);
    size8s_t acc_biased = acc_combined + bias32_add;

    DEBUG_PRINT("bias32:       %08x.%08x (Q32.32)", (size4s_t)(bias32_add   >> 32), (size4u_t)bias32_add);
    DEBUG_PRINT("acc_ll_cvt:   %08x.%08x (Q32.32)", (size4u_t)(acc_ll_cvt   >> 32), (size4u_t)acc_ll_cvt);
    DEBUG_PRINT("acc_hl_cvt:   %08x.%08x (Q32.32)", (size4u_t)(acc_hl_cvt   >> 32), (size4u_t)acc_hl_cvt);
    DEBUG_PRINT("acc_combined: %08x.%08x (Q32.32)", (size4u_t)(acc_combined >> 32), (size4u_t)acc_combined);
    DEBUG_PRINT("acc_biased:   %08x.%08x (Q32.32)", (size4u_t)(acc_biased   >> 32), (size4u_t)acc_biased);

    size8s_t scale_cvt = ((size8s_t)sig) << 20;                   // Q0.12

    size4s_t int_bits = 32;
    size8s_t acc_shifted = acc_shift(proc, acc_biased, exp, sat, frac_bits, int_bits);
    size8s_t acc_rectified = acc_rectify(proc, acc_shifted, zeroing, legacy, acc_biased, element_size);
    size16s_t acc_scaled = acc_scale(proc, acc_rectified, scale_cvt);
    size8s_t acc_final = acc_rnd(proc, acc_scaled, element_size, rnd_bit, frac_bits);


    // size8s_t acc_final = acc_shift_mult(proc, acc_biased, exp, sig, sat, element_size, rnd_bit, frac_bits);

    return sat_to_max_v2(proc, acc_final,  element_size, sat);
}

size4u_t hmx_u16x16_cvt(processor_t *proc, size4s_t acc_hh, size4s_t acc_hl,  size4s_t acc_lh, size4s_t acc_ll, size8s_t bias48, size2s_t exp, size2s_t zeroing, size4s_t sig, size4u_t out_bias, size4s_t sat, size4s_t frac_bits, size2s_t legacy) {
    size4s_t element_size = 2;
    frac_bits = element_size*12;    // Force to 12

    size8s_t acc_ll_cvt = (size8s_t)acc_ll; // sign extend from Q0.31 -> Q31.32
    size8s_t acc_lh_cvt = (size8s_t)acc_lh; // sign extend from Q0.31 -> Q31.32
    size8s_t acc_hl_cvt = (size8s_t)acc_hl; // sign extend from Q0.31 -> Q31.32
    size8s_t acc_hh_cvt = (size8s_t)acc_hh; // sign extend from Q0.31 -> Q31.32
    bias48 = (bias48 << 16) >> 16;          // sign extend from Q15.31 -> Q31.32

    size8s_t acc_combined = acc_ll_cvt + ((acc_lh_cvt + acc_hl_cvt + (acc_hh_cvt << 8) ) << 8);
    size8s_t acc_biased = (acc_combined + bias48) >> 16;

    DEBUG_PRINT("bias48:       %08x.%08x (Q32.32)", (size4s_t)(bias48       >> 32), (size4u_t)bias48);
    DEBUG_PRINT("acc_ll_cvt:   %08x.%08x (Q32.32)", (size4u_t)(acc_ll_cvt   >> 32), (size4u_t)acc_ll_cvt);
    DEBUG_PRINT("acc_lh_cvt:   %08x.%08x (Q32.32)", (size4u_t)(acc_lh_cvt   >> 32), (size4u_t)acc_lh_cvt);
    DEBUG_PRINT("acc_hl_cvt:   %08x.%08x (Q32.32)", (size4u_t)(acc_hl_cvt   >> 32), (size4u_t)acc_hl_cvt);
    DEBUG_PRINT("acc_hh_cvt:   %08x.%08x (Q32.32)", (size4u_t)(acc_hh_cvt   >> 32), (size4u_t)acc_hh_cvt);
    DEBUG_PRINT("acc_combined: %08x.%08x (Q32.32)", (size4u_t)(acc_combined >> 32), (size4u_t)acc_combined);
    DEBUG_PRINT("acc_biased:   %08x.%08x (Q32.32)", (size4s_t)(acc_biased   >> 32), (size4u_t)acc_biased);

    size8s_t scale_cvt = (size8s_t)sig << 10;                   // Q0.22

    size4s_t int_bits = 32;
    size8s_t acc_shifted = acc_shift(proc, acc_biased, exp, sat, frac_bits, int_bits);
    size8s_t acc_rectified = acc_rectify(proc, acc_shifted, zeroing, legacy, acc_biased, element_size);
    size16s_t acc_scaled = acc_scale(proc, acc_rectified, scale_cvt);
    size8s_t acc_final = acc_v3_bias(proc, acc_scaled, element_size, out_bias, frac_bits);

    // size8s_t acc_final = acc_shift_mult(proc, acc_biased, exp, sig, sat, element_size, rnd_bit, frac_bits);

    return sat_to_max_v2(proc, acc_final,  element_size, sat);
}
