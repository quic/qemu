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

#ifndef _MPY_FP16_H_
#define _MPY_FP16_H_

#include "int16_emu.h"
#include "mpy_hmx_support.h"

void hmx_fp16_acc_ovf_check(processor_t *proc, size16s_t acc, size1s_t * ovf, size1s_t fp_rnd_bits);


size2u_t hmx_fp16_cvt(processor_t *proc,  size16s_t acc, size1s_t  ovf, size1s_t fp_rnd_bits, size4s_t bias, size2s_t exp_scale, size2u_t exp_sat, size4s_t sat, size4s_t hi, size4s_t lo);

size4u_t hmx_u8_cvt( processor_t *proc, size4s_t acc,   size4s_t bias32, size2s_t exp, size2s_t zeroing, size2s_t sig, size2u_t out_bias, size4s_t sat, size4s_t frac_bits, size2s_t legacy);
size4u_t hmx_u16_cvt(processor_t *proc, size4s_t acc_hl, size4s_t acc_ll, size4s_t bias32, size2s_t exp, size2s_t zeroing, size2s_t sig, size2s_t rnd_bit, size4s_t sat, size4s_t frac_bits, size2s_t legacy);
size4u_t hmx_u16x16_cvt(processor_t *proc, size4s_t acc_hh, size4s_t acc_hl, size4s_t acc_lh, size4s_t acc_ll, size8s_t bias48, size2s_t exp, size2s_t zeroing, size4s_t sig, size4u_t out_bias, size4s_t sat, size4s_t frac_bits, size2s_t legacy);

size16s_t convert_to_2c(size16s_t in, int sgn);
size16s_t convert_to_1c(size16s_t in, int sgn);

double hmx_fp16_float(size2u_t in);

double hmx_acc_double(size16s_t acc);

#endif
