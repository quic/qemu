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

#include <math.h>


#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#include "utils.h"
#include <stdio.h>
#include <assert.h>
#include "mpy_fp16.h"


static inline size4s_t
is_denorm_fp16(size2u_t in ) {
    // Return 30-0 for normal range and 0 for denorm case as well
    size2s_t exponent = (in & EXP_MASK) >> 10;
    return (exponent) ? 0 : 1 ;
}

static inline size4s_t
get_exp_fp16(size2u_t in ) {
    // Return 30-0 for normal range and 0 for denorm case as well
    size2s_t exponent = (in & EXP_MASK) >> 10;
    return (exponent) ? exponent-BIAS_HF : 0 ;
}

static inline size4s_t
get_sign_fp16(size2u_t in ) {
    return (in & SGN_MASK) >> 15;
}


static inline size4s_t
get_signif_fp16(size2u_t in ) {
    size4s_t signif = (in & (MANTISSA_MASK)) | (( in & EXP_MASK ) ?  (1<<10) : 0);
    return signif;
}

static inline  size4s_t
is_pos_inf_fp21(size4s_t in ) {
    return FP21_POS_INF == in ;
}
static inline  size4s_t
is_neg_inf_fp21(size4s_t in ) {
    return FP21_NEG_INF == in ;
}
static inline  size4s_t
is_nan_fp21(size4s_t in ) {
    return 3*(((( (in>>15) & 0x1F)) == 0x1F) && ((in & (MANTISSA_MASK_FP21)) != 0 ));
}






static inline size4s_t
is_inf(size2u_t in ) {
    return (FP16_NEG_INF == in) || (FP16_POS_INF == in ) ;
}

static inline  size4s_t
is_zero(size2u_t in ) {
    return (FP16_NEG_ZERO == in) || (FP16_POS_ZERO == in ) ;
}

static inline  size4s_t
is_nan(size2u_t in ) {
    return (((in & EXP_MASK) >> 10) == 0x1F) && ((in & (MANTISSA_MASK)) != 0 );
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
#ifdef VERIFICATION
#define DEBUG_PRINT(...)\
    {\
		if (proc->ver_hmx_debug_print_level >= 2) {\
			fprintf(proc->fp_hmx_debug, __VA_ARGS__);\
            fprintf(proc->fp_hmx_debug, "\n");\
		}\
	}
#else
#define DEBUG_PRINT(...)
#endif
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

size16s_t hmx_fp16_mac(processor_t *proc, size16s_t acc, size1s_t * ovf, size1s_t fp_rnd_bits, size2u_t A, size2u_t B)
{
    size16s_t result = {.hi = 0, .lo = 0};
    if(proc->arch_proc_options->QDSP6_MX_FP_PRESENT == 0){
        return result;
    }
    // Alternatiove half-precision format like ARM Spec:
    // IF 0<E<32:
    //    Value = (-1)Sx2(E-15)x(1+2-10T)

    // IF E==0:
    //    IF T==0: Value = Signed zero
    //    IF T!=0: Value = (-1)Sx2(-14)x(0+2-10T)
    size4s_t max_exp_is_inf = USR_IEEE_INF_NAN(fp_rnd_bits);
    size4s_t signif_a = get_signif_fp16(A);
    size4s_t signif_b = get_signif_fp16(B);
    size4s_t sign_ab = get_sign_fp16(A) ^ get_sign_fp16(B);
    size4s_t denorm_a = is_denorm_fp16(A);
    size4s_t denorm_b = is_denorm_fp16(B);
    size4s_t exp_ab  = get_exp_fp16(A) + get_exp_fp16(B) + 64 -20 - 14*denorm_a - 14*denorm_b;
    size8s_t lsb_bit = 64 - proc->arch_proc_options->QDSP6_MX_FP_ACC_FRAC;
    size16s_t product = cast8s_to_16s((size8s_t)signif_a * signif_b);
    size16s_t shifted = shiftl128(product, exp_ab); // Result is 83-bits 17 fractional in hi and 24-48 in lo
    size16s_t rounded = hmx_rnd(proc, shifted, lsb_bit);

    //DEBUG_PRINT("sign:    %x %x  lsb_bit=%lli ", signif_a, signif_b, lsb_bit);
    //DEBUG_PRINT("denorm: %x %x  exp_ab=%d ", denorm_a, denorm_b, exp_ab);

    if (sign_ab) {              // Convert Back to 2's complement
        size16s_t one = { .hi= 0, .lo = 1 };
        rounded.lo = ~rounded.lo;
        rounded.hi = ~rounded.hi;
        rounded = add128(rounded, one);
    }
    DEBUG_PRINT("mult:    %016llx.%016llx ", product.hi, product.lo);
    DEBUG_PRINT("shift:   %016llx.%016llx ", shifted.hi, shifted.lo);
    DEBUG_PRINT("rnd:     %016llx %016llx ", rounded.hi, rounded.lo);


    size4s_t inf_ovf = 0;
    size4s_t nan_ovf = 0;
    if (max_exp_is_inf) {

        size4s_t propagate_nan = USR_IEEE_NAN_PROPAGATE(fp_rnd_bits);
        size4s_t a_is_inf = is_inf(A);
        size4s_t b_is_inf = is_inf(B);
        size4s_t a_is_nan = is_nan(A);
        size4s_t b_is_nan = is_nan(B);
        size4s_t a_is_zero = is_zero(A);
        size4s_t b_is_zero = is_zero(B);

        inf_ovf |= propagate_nan && (a_is_inf||b_is_inf);
        inf_ovf |= (a_is_inf && !b_is_zero) || (b_is_inf && !a_is_zero);
        *ovf |= inf_ovf << sign_ab;    // Inf * Inf or Inf/Inf

        nan_ovf |= propagate_nan && (a_is_nan||b_is_nan);
        nan_ovf |= propagate_nan && ( ((a_is_nan||a_is_inf) && b_is_zero) || ( (b_is_nan||b_is_inf) && a_is_zero) );
        nan_ovf |= (a_is_nan && !b_is_zero) || (b_is_nan && !a_is_zero);
        *ovf |= 0x3*nan_ovf ;    // Inf * Inf or Inf/Inf
        DEBUG_PRINT("IEEE INF/NAN USR=%d check A=0x%04x inf=%d nan=%d zero=%d B=0x%04x inf=%d nan=%d zero=%d ovf=%d", fp_rnd_bits, A, a_is_inf, a_is_nan, a_is_zero, B, b_is_inf, b_is_nan, b_is_zero, *ovf);



    }

    if ((nan_ovf|inf_ovf) == 0) {
        if(rounded.hi > MAX_ACC_FP16_POS) {
            *ovf |= 1;
            DEBUG_PRINT("Postive overflow top 17 bits: %016llx ovf=%x", rounded.hi, *ovf);
        } else if ( rounded.hi < MAX_ACC_FP16_NEG ) {
            *ovf |= 2;
            DEBUG_PRINT("Negative overflow top 17 bits: %016llx ovf=%x", rounded.hi, *ovf);
        }
    }


    result = add128(acc, rounded);
    DEBUG_PRINT("result:  %016llx.%016llx\n", result.hi, result.lo);
    return result;
}



size16s_t convert_to_2c(size16s_t in, int sgn);
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


size16s_t convert_to_1c(size16s_t in, int sgn);
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
            if (is_nan(result)) {
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


static size2u_t sat_to_max(processor_t *proc, size8s_t in, size4s_t element_size, size4s_t sat) {
    size8s_t max_element = (1 << (element_size*8)) - 1;
    size2u_t out = 0;
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
    DEBUG_PRINT("in = %llx out=%04x (sat=%d)\n", in, out, sat);
    return out;
}

static inline size8s_t acc_shift(processor_t *proc, size8s_t acc_biased, size4s_t exp, size4s_t sat, size4s_t frac_bits) {
    size4s_t shift_acc = (32-frac_bits);
    size8s_t acc_shifted = (acc_biased << exp);
    size8s_t mask =((long long int)((1llu << frac_bits) - 1) ) << shift_acc;
    if (sat) {
        mask |= ((long long int)((1llu << 32) - 1) ) << 32;
    }
    acc_shifted &= mask ;  // Clip only fractional bits

    DEBUG_PRINT("mask:         %08x.%08x (Q32.32)", (size4s_t)(mask   >> 32), (size4u_t)mask);
    DEBUG_PRINT("shifted:      %08x.%08x (Q32.%d) exp=%d" , (size4u_t)(acc_shifted  >> 32), (size4u_t)acc_shifted, frac_bits, exp);

    return acc_shifted;
}

static inline size16s_t acc_scale(processor_t *proc, size8s_t acc_shifted, size8s_t scale_cvt) {
    size16s_t acc_scaled = mult64_to_128(scale_cvt, acc_shifted);

    DEBUG_PRINT("scale:        %08x.%08x (Q32.10)", (size4s_t)(scale_cvt   >> 32), (size4u_t)scale_cvt);
    DEBUG_PRINT("acc_scaled:   %016llx.%016llx (Q64.64)", (size8u_t)(acc_scaled.hi), (size8u_t)acc_scaled.lo);

    return acc_scaled;
}

static inline size8s_t acc_rnd(processor_t *proc, size16s_t acc_scaled, size4s_t element_size, size4s_t rnd_bit, size4s_t frac_bits){
    size8s_t ulp_bit = 64 - 8 * element_size - 1;
    size16s_t ulp;
    size16s_t acc_rnd;

    ulp = cast8s_to_16s( (size8s_t)rnd_bit << ulp_bit);
    acc_rnd = add128(acc_scaled, ulp); // + ULP
    size8s_t acc_final = cast16s_to_8s(shiftr128(acc_rnd, (ulp_bit+1))); // Q31.16

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

    ulp_bit = 64 - 8 - 1;
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

size2u_t hmx_u8_cvt(processor_t *proc, size4s_t acc, size4s_t bias32, size2s_t exp, size2s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits){

    size4s_t element_size = 1;

    frac_bits = (frac_bits == 0) ? 12 : 10; // V1 or V2 behavior

    size8s_t bias32_add = (size8s_t)bias32;
    size8s_t acc_cvt =    (size8s_t)acc;
    size8s_t acc_biased  = acc_cvt + bias32_add;

    DEBUG_PRINT("bias32:       %08x.%08x (Q32.32)", (size4s_t)(bias32_add   >> 32), (size4u_t)bias32_add);
    DEBUG_PRINT("acc_cvt:      %08x.%08x (Q32.32)", (size4u_t)(acc_cvt      >> 32), (size4u_t)acc_cvt);
    DEBUG_PRINT("acc_biased:   %08x.%08x (Q32.32)", (size4u_t)(acc_biased   >> 32), (size4u_t)acc_biased);

    size8s_t scale_cvt = ((size8s_t)sig | 0x400) << 22;                   // Q1.10

    size8s_t acc_shifted = acc_shift(proc, acc_biased, exp, sat, frac_bits);
    size16s_t acc_scaled = acc_scale(proc, acc_shifted, scale_cvt);
    size8s_t acc_final;
    if(frac_bits == 10){
        acc_final = acc_v1_bias(proc, acc_scaled, element_size, rnd_bit, frac_bits);
    }
    else{
        acc_final = acc_rnd(proc, acc_scaled, element_size, rnd_bit, frac_bits);
    }

    // size8s_t acc_final = acc_shift_mult(proc, acc_biased, exp, sig, sat, element_size, rnd_bit, frac_bits);

    return sat_to_max(proc, acc_final,  element_size, sat);
}



size2u_t hmx_u16_cvt(processor_t *proc, size4s_t acc_hl, size4s_t acc_ll, size4s_t bias32, size2s_t exp, size2s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits) {
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

    size8s_t scale_cvt = ((size8s_t)sig | 0x400) << 22;                   // Q1.10

    size8s_t acc_shifted = acc_shift(proc, acc_biased, exp, sat, frac_bits);
    size16s_t acc_scaled = acc_scale(proc, acc_shifted, scale_cvt);
    size8s_t acc_final = acc_rnd(proc, acc_scaled, element_size, rnd_bit, frac_bits);


    // size8s_t acc_final = acc_shift_mult(proc, acc_biased, exp, sig, sat, element_size, rnd_bit, frac_bits);

    return sat_to_max(proc, acc_final,  element_size, sat);
}

size2u_t hmx_u16x16_cvt(processor_t *proc, size4s_t acc_hh, size4s_t acc_hl,  size4s_t acc_lh, size4s_t acc_ll, size8s_t bias48, size2s_t exp, size4s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits) {
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

    size8s_t scale_cvt = ((size8s_t)sig | 0x100000) << 12;                   // Q1.20

    size8s_t acc_shifted = acc_shift(proc, acc_biased, exp, sat, frac_bits);
    size16s_t acc_scaled = acc_scale(proc, acc_shifted, scale_cvt);
    size8s_t acc_final = acc_rnd(proc, acc_scaled, element_size, rnd_bit, frac_bits);

    // size8s_t acc_final = acc_shift_mult(proc, acc_biased, exp, sig, sat, element_size, rnd_bit, frac_bits);

    return sat_to_max(proc, acc_final,  element_size, sat);
}
