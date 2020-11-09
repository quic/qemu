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

#ifndef _MPY_FP16_H_
#define _MPY_FP16_H_ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


#include "int16_emu.h"

#ifdef STANDALONE

typedef struct arch_proc_opt {
    int hmx_mxmem_debug;
	int	hmx_fp16_acc_width_int;
    int	hmx_fp16_acc_width_frac;
    int hmx_mxmem_debug_depth;
    int hmx_mxmem_debug_spatial;
    int QDSP6_MX_FP_ACC_FRAC;
} arch_proc_opt_t;

typedef struct th {
        int tmp;
} thread_t;

typedef struct ProcessorState {
    thread_t * thread[1];
    arch_proc_opt_t * arch_proc_options;
} processor_t;




#else
#include "arch.h"
#include "hmx.h"
#endif


#define BIAS_HF 15
#define E_MAX_HF 16
#define MAX_ACC_WIDTH 66

// Back of two bits on normaliztion
#define ULP_BIT 54
#define ACC_SIGN_BIT 17
#define ACC_TOP_BITS_MASK 0x3FFFF
#define ACC_LOW_BITS_MASK ~(0xFFFF)
#define NORM_MANISSA_SIZE 11
#define MANTISSA_MASK 1023
#define EXP_MASK (0x1F<<10)
#define SGN_MASK (1<<15)

#define MAX_ACC_FP16_POS 131071
#define MAX_ACC_FP16_NEG -131072


#define FP16_POS_OVF_BIT 0
#define FP16_NEG_OVF_BIT 1

#define FP16_POS_INF 0x7C00
#define FP16_NEG_INF 0xFC00
#define FP21_POS_INF 0x0F8000
#define FP21_NEG_INF 0x1F8000

#define FP16_POS_ZERO 0x0000
#define FP16_NEG_ZERO 0x8000

#define FP16_NAN 0xFFFF
#define MANTISSA_MASK_FP21 0x7FFF


#define USR_IEEE_INF_NAN(VAL) ((VAL>>0) & 0x1)
#define USR_IEEE_NAN_PROPAGATE(VAL) ((VAL>>1) & 0x1)

size16s_t hmx_fp16_mac(processor_t *proc, size16s_t acc, size1s_t * ovf, size1s_t fp_rnd_bits, size2u_t A, size2u_t B);
void hmx_fp16_acc_ovf_check(processor_t *proc, size16s_t acc, size1s_t * ovf, size1s_t fp_rnd_bits);


size2u_t hmx_u8_cvt( processor_t *proc, size4s_t acc,   size4s_t bias32, size2s_t exp, size2s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits);
size2u_t hmx_u16_cvt(processor_t *proc, size4s_t acc_hl, size4s_t acc_ll, size4s_t bias32, size2s_t exp, size2s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits);
size2u_t hmx_u16x16_cvt(processor_t *proc, size4s_t acc_hh, size4s_t acc_hl, size4s_t acc_lh, size4s_t acc_ll, size8s_t bias48, size2s_t exp, size4s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits);

#endif
