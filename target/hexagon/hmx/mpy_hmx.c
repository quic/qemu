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
#include "mpy_hmx.h"



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
