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
#include <math.h>
#include "mpy_hmx_fp_custom.h"


#ifndef HMX_XFP_FUNCTION
#define HMX_XFP_FUNCTION(func_name) func_name
#endif

#if defined (STANDALONE)

#define DEBUG_PRINT_XFP(...) \
    if (proc->arch_proc_options->hmx_mxmem_debug >= 2) {\
        printf(__VA_ARGS__);\
        printf("\n");\
    }

#define DEBUG_PRINT_XFP_VAL(NAME, V) \
DEBUG_PRINT_XFP("%s : stat {z:%x inf:%x neg:%x under:%x} exp=0x%04x sig raw=0x%016llx sig q=0x%03x.%08x Q%d.%ue%d exp_dec=%d", NAME, V.status.zero, V.status.inf, V.status.negative, V.status.under, (uint16_t)V.exp, (long long int)V.sig, 	(uint8_t)((V.sig >> V.FRAC) & 0xFF), (uint32_t)(((V.sig) & ((1<<V.FRAC)-1)) << (32 - V.FRAC)), V.INT, V.FRAC, V.EXP, (int32_t)V.exp);

#elif defined (VERIFICATION)

#define DEBUG_PRINT_XFP(...) \
    {\
		if (proc->ver_hmx_debug_print_level >= 2) {\
			fprintf(proc->fp_hmx_debug, __VA_ARGS__);\
            fprintf(proc->fp_hmx_debug, "\n");\
		}\
	}
#define DEBUG_PRINT_XFP_VAL(NAME, V) \
DEBUG_PRINT_XFP("%s : stat {z:%x inf:%x neg:%x under:%x} exp=0x%04x sig raw=0x%016llx sig q=0x%03x.%08x Q%d.%ue%d exp_dec=%d", NAME, V.status.zero, V.status.inf, V.status.negative, V.status.under, (uint16_t)V.exp, (long long int)V.sig, 	(uint8_t)((V.sig >> V.FRAC) & 0xFF), (uint32_t)(((V.sig) & ((1<<V.FRAC)-1)) << (32 - V.FRAC)), V.INT, V.FRAC, V.EXP, (int32_t)V.exp);


#elif defined (DBG_TRACING)

#define DEBUG_PRINT_XFP(...) \
    if (proc->arch_proc_options->hmxdebugfile && (proc->monotonic_pcycles > proc->arch_proc_options->hmxdebugfile_start_pcycle) && (proc->monotonic_pcycles < proc->arch_proc_options->hmxdebugfile_end_pcycle)) {\
		char buf[1024];\
		sprintf(buf, __VA_ARGS__); \
		fprintf(proc->arch_proc_options->hmxdebugfile, "%s\n", buf);\
	}\

#define DEBUG_PRINT_XFP_VAL(NAME, V) \
DEBUG_PRINT_XFP("%s : stat {z:%x inf:%x neg:%x under:%x} exp=0x%04x sig raw=0x%016llx sig q=0x%03x.%08x Q%d.%ue%d exp_dec=%d", NAME, V.status.zero, V.status.inf, V.status.negative, V.status.under, (uint16_t)V.exp, (long long int)V.sig, 	(uint8_t)((V.sig >> V.FRAC) & 0xFF), (uint32_t)(((V.sig) & ((1<<V.FRAC)-1)) << (32 - V.FRAC)), V.INT, V.FRAC, V.EXP, (int32_t)V.exp);


#else

#define DEBUG_PRINT_XFP(...)

#define DEBUG_PRINT_XFP_VAL(...)

#endif


static inline exp_range_t get_exp_range_unbiased(int32_t exp_bits) {
	exp_range_t out;
	out.min = -(1 << (exp_bits-1));
	out.max = -out.min-1;
	return out;
}


hmx_xfp_t HMX_XFP_FUNCTION(hmx_fp_xfp)(processor_t *proc,
		usr_fp_reg_t usr,
		uint32_t in,
		uint32_t frac_in,
		uint32_t exp_in,
		uint32_t int_out,
		uint32_t frac_out,
		uint32_t exp_out,
		uint32_t normalize)
{
	hmx_xfp_t out_xfp = {.INT = int_out, .EXP = exp_out, .FRAC = frac_out};

	const int32_t in_exp_min = 1;
	const int32_t in_exp_max = (1 << exp_in) - 1;
	const int32_t input_expo_bias = (1 << (exp_in-1));

	exp_range_t out_exp_range = get_exp_range_unbiased(exp_out);

	hmx_fp_t in_fp = fp_to_hmx_fp(proc, in, exp_in, frac_in);
	const int32_t in_denorm =  (in_fp.exp < in_exp_min);

	DEBUG_PRINT_XFP("hmx_fp_xfp in                  : %05x { sign=%u exp=%04x frac=%04x} in_denorm=%d normalize=%d", (uint32_t)in, in_fp.sign, (uint16_t)in_fp.exp, (uint16_t)in_fp.frac, in_denorm, normalize);

	int32_t exp_unbiased = in_fp.exp - (int32_t)input_expo_bias;	// Unbias exponent
	out_xfp.exp = exp_unbiased += (in_fp.exp == 0) ? 1 : 0; 		// denorm exponent is incremented by 1

	uint64_t int_bit = (in_denorm) ? 0 : 1; 						// denorm has no implied 1
	uint64_t mag_out = (int_bit << frac_in) | in_fp.frac; 			// insert implied one

	if (normalize && in_denorm)
	{ // Input normaliztion only for types that do input normalization
		uint64_t tmp = mag_out << (64-(frac_in+1)); // Shift up to MSB
		int32_t normalizing_shift = count_leading_ones_8(~tmp);

		DEBUG_PRINT_XFP("hmx_fp_xfp_input_normalize     : mantissa=0x%016llx normalizing_shift=%d",  (long long int)tmp, normalizing_shift);

		mag_out <<= normalizing_shift;	// normalize;
		out_xfp.exp -= normalizing_shift ;	// adjust expontn
	}

	int64_t  sig_out = (in_fp.sign) ? -(int32_t)mag_out : (int32_t)mag_out; // change from sign/mag to 2complement
	int32_t  shift = (int_out + frac_out - 2 - frac_in );
	shift = 	((shift < 0) ? 0 : shift);					// adjust for output fractional size
	sig_out = sig_out << shift;
	out_xfp.sig = sig_out;

	uint8_t inf = ((in_fp.exp==in_exp_max) & (in_fp.frac==0)) * (in_fp.sign ? 2 : 1); //0,1,2
	uint8_t nan = ((in_fp.exp==in_exp_max) & (in_fp.frac!=0)) ? 3 : 0; // 0, 3

	out_xfp.status.zero = ((in_fp.exp==0) & (in_fp.frac==0)); // 0, 1
	out_xfp.status.inf = usr.inf_nan_enable ? (inf | nan) : 0; 	// If INF/NAN is enabled by USR set the flag, otherwise it's zero
	out_xfp.status.negative = (out_xfp.status.zero) ? 0 : in_fp.sign;
	out_xfp.exp = 	(out_xfp.status.zero) ? out_exp_range.min : out_xfp.exp;	// True zero set the exponent to min

	DEBUG_PRINT_XFP_VAL("hmx_fp_xfp out                ", out_xfp);

	return out_xfp;
}


uint32_t HMX_XFP_FUNCTION(hmx_xfp_fp)(processor_t *proc,
		usr_fp_reg_t usr,
		hmx_xfp_t in,
		uint32_t fp_frac,
		uint32_t fp_exp,
		const hmx_cvt_rs_reg_t rs,
		usr_fp_reg_t usr_tmp_remove_me)
{

	uint32_t output = 0;
	uint16_t hf16_out = fp_exp == 5;
	DEBUG_PRINT_XFP_VAL("hmx_xfp_fp in                 ", in);

	// Normalize sig to 1.0
	int64_t tmp_sig = in.sig << (64-(in.INT+in.FRAC));
	uint64_t mag_in = (tmp_sig < 0) ? (uint64_t)-tmp_sig : (uint64_t)tmp_sig; 	// Convert to Unsigned
	int32_t exp_normalized = in.exp;
	uint8_t zero = 0;
	DEBUG_PRINT_XFP("hmx_xfp_fp magnitude normalization for fp to 1.x for in.sig=0x%016llx (%llx.%llx) exp=%d mag=0x%016llx", (long long int)in.sig, (long long int)in.sig>>in.FRAC, (long long int)in.sig << (64-in.FRAC), exp_normalized, (long long int)mag_in);
	if (mag_in != 0) {
		int32_t normalizing_shift = count_leading_ones_8(~mag_in);	// not including sign
		if(normalizing_shift) {
			mag_in <<= normalizing_shift;
			exp_normalized -= normalizing_shift;
		} else {
			normalizing_shift = 0;
		}
		exp_normalized += (in.INT - 2 ); // Difference between incoming and 1 int + sign bit
		DEBUG_PRINT_XFP("hmx_xfp_fp magnitude normalization to 1.x mag_out=%016llx exp_normalized=%d normalizing_shift=%d", (long long int)mag_in, exp_normalized, normalizing_shift);
	} else {
		zero = 1;
	}
	DEBUG_PRINT_XFP("hmx_xfp_fp mag_normalized=%016llx (%1llx.%016llx) exp=%d and adjusted. zero sig=%u", (long long int)mag_in, (long long int)(mag_in>>63) & 0x1, (long long int)mag_in << 1, exp_normalized, zero);


	exp_range_t fp_exp_range = get_exp_range_unbiased(fp_exp);
	uint8_t exp_overflow = (exp_normalized >= fp_exp_range.max);
	uint8_t exp_underflow = (exp_normalized <= fp_exp_range.min);

	uint8_t unrecoverable = (uint8_t)(((int32_t)exp_normalized < ((int32_t)fp_exp_range.min-(int32_t)fp_frac)));

	DEBUG_PRINT_XFP("hmx_xfp_fp exp overflow=%u underflow=%u (unrecoverable=%u) exp=%d range=[%d %d] fp_frac=%d", exp_overflow, exp_underflow, unrecoverable, exp_normalized, fp_exp_range.min, fp_exp_range.max, fp_frac);
	// Easy cases first
	uint16_t sign_in = (in.status.zero) ? in.status.negative : (in.sig < 0) || (in.status.negative); // -0
	if (in.status.inf == 0x3) {
		output = FP16_NAN;
		if (!rs.relu && !usr_tmp_remove_me.nan_propagate)
			output = FP16_POS_ZERO;
		DEBUG_PRINT_XFP("hmx_xfp_fp nan output %x relu=%d usr=%x", output, rs.relu, usr_tmp_remove_me.nan_propagate);
		return output;
	} else if (in.status.inf != 0)  {
		output = (in.status.inf==2) ? ( hf16_out ? FP16_NEG_INF : BF16_NEG_INF ) : ( hf16_out ? FP16_POS_INF : BF16_POS_INF );
		output = ((in.status.inf==2) && !rs.relu) ? FP16_POS_ZERO : output;
		DEBUG_PRINT_XFP("hmx_xfp_fp inf output=%x incoming inf=%x relu=%d", output, in.status.inf, rs.relu);
		return output;
	} else if (exp_overflow && !zero) {
		output = (sign_in) ? ( hf16_out ? FP16_NEG_INF : BF16_NEG_INF ) : ( hf16_out ? FP16_POS_INF : BF16_POS_INF );
		output = (sign_in && !rs.relu) ? FP16_POS_ZERO : output;

		DEBUG_PRINT_XFP("hmx_xfp_fp inf output=%x exponent overflow and not zero. incoming sign_in=%x relu=%d", output, sign_in, rs.relu);
		return output;
	} else if (in.status.zero || zero) {
		output = (sign_in ) ? FP16_NEG_ZERO :  FP16_POS_ZERO;
		output = (!rs.relu) ? FP16_POS_ZERO : output;
		DEBUG_PRINT_XFP("hmx_xfp_fp zero output %x", output);
		return output;
	} else if (in.status.under) {
		output = (sign_in) ? FP16_NEG_ZERO :  FP16_POS_ZERO;
		output = (!rs.relu) ? FP16_POS_ZERO : output;
		DEBUG_PRINT_XFP("hmx_xfp_fp underflow output %x", output);
		return output;
	}
	//mag_in >>= 1;	// Shift down by 1 to catch overflow, just throwing away 0 in lsb of uint64
	if (exp_underflow)
	{
		mag_in >>= (fp_exp_range.min-exp_normalized + 1);
		DEBUG_PRINT_XFP("hmx_xfp_fp underflow. adjusted mantissa to %016llx by %d", (long long int)mag_in, (fp_exp_range.min-exp_normalized));
	}

	// Roudning to nearest even

	// UGS - RND add at G
	// 000 - 0 - stay at even
	// 001 - 0 - stay at even
	// 010 - 0 - exactly .5 - stay at even
	// 011 - 1 - round up
	// 100 - 0 - stay at odd
	// 101 - 0 - stay at odd
	// 110 - 1 - exactly .5 - round up to even
	// 111 - 1 - round up

	int32_t ulp_bit = 64 - (1 + fp_frac);
	int32_t grd_bit = ulp_bit - 1;
	uint64_t sticky_mask = ((1ll << grd_bit)-1);
	uint64_t sticky = (sticky_mask & mag_in) != 0ll;
	uint64_t grd = 	(mag_in >> grd_bit) & 0x1;
	uint64_t ulp = 	(mag_in >> ulp_bit) & 0x1;
	uint64_t rnd = (ulp) ? ( grd ) : ( sticky & grd);
	uint16_t fp_mantissa = (uint16_t)((mag_in + (rnd << grd_bit)) >> ulp_bit);	// Add Rounding bit and truncate to 11/8-bit mantissa

	//printf("%d %d %d\n", ulp_bit, grd_bit, sticky_bit);
	DEBUG_PRINT_XFP("hmx_xfp_fp rne mag_in=%016llx rounded mag=%016llx ulp=%d guard=%d sticky=%d rnd=%d fp_mantissa=0x%03x sticky_mask=%016llx all_sticky_bits=%016llx", (long long int)mag_in,  (long long int)(mag_in + (rnd << grd_bit)), (int32_t)ulp, (int32_t)grd, (int32_t)sticky, (int32_t)rnd, fp_mantissa, (long long int)sticky_mask, (long long int)(sticky_mask & mag_in) );

	int32_t exp_bias = (1 << (fp_exp-1));
	int32_t fp_exponent = (exp_underflow) ? -exp_bias : exp_normalized;


	if (exp_underflow) {
		if ((fp_mantissa & (1<<fp_frac)) && !unrecoverable) {
			fp_exponent = -exp_bias + 1;
			DEBUG_PRINT_XFP("hmx_xfp_fp underflow. but rounded to normal range exp=%d fp_mantissa=0x%03x", fp_exponent, fp_mantissa);
		} else if (unrecoverable) {
			output = (sign_in ) ? FP16_NEG_ZERO :  FP16_POS_ZERO;
			output = (!rs.relu) ? FP16_POS_ZERO : output;
			DEBUG_PRINT_XFP("hmx_xfp_fp zero output due to unrecoverable underflow %x", output);
			return output;
		}
	} else if (fp_mantissa==0)  {
		fp_exponent++;
		DEBUG_PRINT_XFP("hmx_xfp_fp mantissa overflow on round incremented exponent by one to=%d", fp_exponent);
		if ( fp_exponent >= fp_exp_range.max) {
			DEBUG_PRINT_XFP("hmx_xfp_fp mantissa overflow to inf exponent=%d", fp_exponent);
			fp_exponent = fp_exp_range.max;
		}
	}


	fp_mantissa &= (1<<fp_frac)-1;
	fp_exponent += exp_bias;
	fp_exponent &= ((1<<fp_exp)-1);

	int32_t sign_bit = 15 + 4*rs.fp_rnd;
	output = (sign_in << sign_bit) | (fp_exponent << fp_frac) | fp_mantissa;
	output <<= (rs.fp_rnd) ? 0 : 4;
	if (sign_in && !rs.relu) {
		output = FP16_POS_ZERO;
	}
	DEBUG_PRINT_XFP("hmx_xfp_fp out:  out: %05x denorm = %u {%x %02x %05x} relu=%d rnd_bit=%d", output, exp_underflow, sign_in, fp_exponent, fp_mantissa, rs.relu, rs.fp_rnd);
	return output;
}


static inline int64_t right_shift_with_inexact(processor_t * proc, int64_t in, int32_t shift)
{
	int64_t inexact_mask = proc->arch_proc_options->xfp_inexact_enable ? ((1ll<<shift)-1) : 0;
	int64_t inexact = (inexact_mask & in) != 0;
	DEBUG_PRINT_XFP("hmx_xfp_shift_with inexact     : in=0x%016llx shift=%d inexact=%llx, inexact_enable=%d", (long long int) in, shift, (long long int)inexact, proc->arch_proc_options->xfp_inexact_enable);
	return (in >> shift) | inexact;
}


hmx_xfp_t HMX_XFP_FUNCTION(hmx_xfp_normalize)(
		processor_t *proc,
		usr_fp_reg_t usr,
		hmx_xfp_t	in,
		uint32_t	int_out,
		uint32_t	frac_out,
		int32_t	    exp_out,
		uint32_t	use_lza)
{

	hmx_xfp_t out = {.EXP = exp_out, .FRAC = frac_out, .INT = int_out};
	exp_range_t out_exp_range = get_exp_range_unbiased(exp_out);

	DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize in          ", in);


	// First change from Q8.22 to Q7.22, it's just an exponent adjustment
	int32_t additional_exp = in.INT-out.INT;
	in.exp += additional_exp;
	in.INT  -=additional_exp;
	in.FRAC +=additional_exp;

	DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize in2         ", in);
	if (additional_exp) {
		in.sig = right_shift_with_inexact(proc, in.sig, additional_exp);
		in.FRAC -= additional_exp;
		DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize in3         ", in);
	}

	int32_t in_bit_count = in.FRAC + in.INT;
	int64_t temp_sig = in.sig << (64-in_bit_count); // Shift up to MSB
	int64_t temp_sig2 = (in.status.negative && (temp_sig!=0)) ? temp_sig: ~temp_sig;
	int32_t normalizing_shift = (use_lza) ? in.lza : count_leading_ones_8(temp_sig2)-1;	// not including sign
	uint8_t ovf = (normalizing_shift==0) && (out.exp == out_exp_range.max); // Maximum Exponent and already normalized will overflow
	int32_t max_shift = proc->arch_proc_options->QDSP6_MX_FP_ACC_NORM;
	int32_t normalizing_shift2 = 0;
	normalizing_shift2 = (normalizing_shift > max_shift)  ? max_shift : normalizing_shift;
	normalizing_shift2 = (normalizing_shift2 < 0) ? 0 : normalizing_shift2;


	// Adjust exponent
	int32_t at_min_exp_adjust = (in.exp - normalizing_shift2) - out_exp_range.min;
	//DEBUG_PRINT_XFP("hmx_xfp_normalize              : at_min_exp_adjust=%2d exp=%04x min_exp=%04x shift=%d adjusted_shift=%d temp_sig2=%016llx", at_min_exp_adjust, in.exp, out_exp_range.min, normalizing_shift2, normalizing_shift2 + at_min_exp_adjust, temp_sig2);
	if ( at_min_exp_adjust < 0 ) {
		DEBUG_PRINT_XFP("hmx_xfp_normalize  near min exp: at_min_exp_adjust=%2d exp=%04x min_exp=%04x shift=%d adjusted_shift=%d", at_min_exp_adjust, in.exp, out_exp_range.min, normalizing_shift2, normalizing_shift2 + at_min_exp_adjust);
		normalizing_shift2 += at_min_exp_adjust;
	}
	temp_sig2 = (in.status.negative && (temp_sig!=0)) ? temp_sig2: ~temp_sig2;

	int64_t normalized_sig = (temp_sig2 << normalizing_shift2);	// normalize;
	out.sig =  normalized_sig >> (64-in_bit_count);
	out.exp = in.exp - normalizing_shift2;

	DEBUG_PRINT_XFP("hmx_xfp_normalize              : in.sig=%016llx temp_sig=%016llx normalized_sig=%016llx shift_raw=%d clipped=%d final_sig=%016llx (use_lza=%d lza shift=%u)", (long long int)in.sig, (long long int)temp_sig, (long long int)normalized_sig, normalizing_shift, normalizing_shift2, (long long int)out.sig, use_lza, in.lza);

	// Flags won't change
	out.status.zero = in.status.zero;
	out.status.inf = in.status.inf;
	out.status.negative = in.status.negative;


	if ((out.exp > out_exp_range.max) || (ovf)) {
		if (out.status.inf == 0) {
			out.status.inf |= out.status.negative ? 2 : 1;
		}
		DEBUG_PRINT_XFP("hmx_xfp_normalize overflow!: exp=%x set to %x", out.exp, out_exp_range.max  );
		out.exp = out_exp_range.max;
	}

	DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize out         ", out);

	return out;
}

hmx_xfp_t HMX_XFP_FUNCTION(hmx_xfp_cvt_normalize)(
		processor_t *proc,
		usr_fp_reg_t usr,
		hmx_xfp_t	in,
		uint32_t	int_out,
		uint32_t	frac_out,
		int32_t	    exp_out)
{

	hmx_xfp_t tmp = {.EXP = in.EXP, .FRAC = in.FRAC, .INT = in.INT};
	hmx_xfp_t out = {.EXP = exp_out, .FRAC = frac_out, .INT = int_out};
	exp_range_t out_exp_range = get_exp_range_unbiased(exp_out);

	DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize in          ", in);

	int32_t out_bit_count = out.FRAC + out.INT;
	int32_t in_bit_count = in.FRAC + in.INT;



	// Normalize
	uint8_t neg_sig = (in.sig < 0) && (in.sig!=0);

	if (in.sig == 0)
	{
		// Infinite Zeros cases for normalization
		// Set the sig to zero like the input and exp to min
		// copy other flags.
		out.sig = 0;
		//out.exp = out_exp_range.min;
		tmp.exp = in.exp - (in.FRAC + in.INT - 1);
		out.exp = tmp.exp + (tmp.INT-out.INT);
		out.status.zero = in.status.zero;
		out.status.under = (in.status.inf == 0) ? 1 : 0;	// Only set underflow if not inf
		out.status.inf = in.status.inf;
		out.status.negative = in.status.negative;
		DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize out zero    ", out);
		return out;
	}


	tmp.sig = in.sig << (64-in_bit_count); // Shift up to MSB
	tmp.sig = (neg_sig) ? tmp.sig: ~tmp.sig;
	DEBUG_PRINT_XFP("hmx_xfp_normalize              : tmp.sig=0x%016llx",  (long long int)tmp.sig);
	int32_t normalizing_shift = count_leading_ones_8(tmp.sig)-1;	// not including sign
	uint8_t ovf = (normalizing_shift==0) && (out.exp == out_exp_range.max); // Maximum Exponent and already normalized will overflow
	normalizing_shift = (normalizing_shift < 0) ? 0 : normalizing_shift;
	DEBUG_PRINT_XFP("hmx_xfp_normalize              : normalizing_shift=%d",  normalizing_shift);

	tmp.sig = (neg_sig) ? tmp.sig: ~tmp.sig;
	tmp.sig = (tmp.sig << normalizing_shift);	// normalize;

	tmp.sig >>= (64-in_bit_count);
	tmp.exp = in.exp - normalizing_shift ;
	DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize tmp         ", tmp);
	out.sig =  right_shift_with_inexact(proc, tmp.sig, (in_bit_count-out_bit_count));
	out.exp = tmp.exp + (tmp.INT-out.INT);

	DEBUG_PRINT_XFP("hmx_xfp_normalize              : in.sig=0x%016llx normalized_sig=0x%016llx shift=%d final_sig=0x%016llx", (long long int)in.sig, (long long int)tmp.sig, normalizing_shift, (long long int)out.sig);

	// Flags won't change
	out.status.zero = in.status.zero;
	out.status.inf = in.status.inf;
	out.status.negative = in.status.negative;

	if ((out.exp > out_exp_range.max) || (ovf)) {
		if (out.status.inf == 0) {
			out.status.inf |= out.status.negative ? 2 : 1;
		}
		DEBUG_PRINT_XFP("hmx_xfp_normalize overflow!    : exp=%x set to %x", out.exp, out_exp_range.max  );
		out.exp = out_exp_range.max;
	} else if ((out.exp < out_exp_range.min)  && (out.status.inf==0)) {
		DEBUG_PRINT_XFP("hmx_xfp_normalize underflow!   : exp=%x set to %x", out.exp, out_exp_range.min  );
		out.exp = out_exp_range.min;
		out.status.under = 1;
	}

	DEBUG_PRINT_XFP_VAL("hmx_xfp_normalize out         ", out);
	return out;
}



// xfp addition/subtraction
hmx_xfp_t HMX_XFP_FUNCTION(hmx_xfp_add)(
		processor_t *proc,
		usr_fp_reg_t usr,
		hmx_xfp_t 	in_a,
		hmx_xfp_t 	in_b)
{

	hmx_xfp_t out = {.EXP = in_a.EXP, .FRAC = in_a.FRAC, .INT = in_a.INT+1 };

	uint8_t a_neg = (in_a.status.zero | in_a.status.inf) ? in_a.status.negative : in_a.sig < 0;
	uint8_t b_neg = (in_b.status.zero | in_b.status.inf) ? in_b.status.negative : in_b.sig < 0;
	uint8_t z_plus_z_sign = ((in_a.sig==0) && (in_b.sig==0)) ? (in_a.status.negative && in_b.status.negative) : 0;
	uint8_t inf = in_a.status.inf | in_b.status.inf; // Inf + Inf = Inf, -Inf + -Inf = -Inf, Inf + -Inf = NaN

	if (in_a.status.under) {
		in_a.exp = -(1 << (in_a.EXP-1));
	}
	if (in_b.status.under) {
		in_b.exp = -(1 << (in_b.EXP-1));
	}

	// Select max exp and the sigs
	int32_t delta_exp = in_a.exp - in_b.exp;
	uint8_t delta_exp_neg = (delta_exp <= 0) ;
	int64_t max_sig = delta_exp_neg ? in_b.sig : in_a.sig;
	int64_t min_sig = delta_exp_neg ? in_a.sig : in_b.sig;

	out.exp = delta_exp_neg ? in_b.exp : in_a.exp;

	delta_exp = delta_exp_neg ? -delta_exp : delta_exp;
	if(delta_exp > 63) delta_exp = 63;

	DEBUG_PRINT_XFP_VAL("hmx_xfp_add in0               ", in_a);
	DEBUG_PRINT_XFP_VAL("hmx_xfp_add in1               ", in_b);
	DEBUG_PRINT_XFP("hmx_xfp_add                    : delta_exp=%x exp_out=%04x", delta_exp, out.exp);

	min_sig =  right_shift_with_inexact(proc, min_sig, delta_exp);
	out.sig = (max_sig + min_sig) ;
	uint8_t neg_out = out.sig < 0;
	DEBUG_PRINT_XFP("hmx_xfp_add sig addition       : max_sig=0x%016llx min_sig=0x%016llx sig=0x%016llx sign=%u", (long long int)max_sig,(long long int)min_sig,(long long int)out.sig, neg_out);

	out.status.zero = 0; 	// Can't have true zero coming out of the adder, yet.
	out.status.inf =   inf; // Doesn't change the flag
	out.status.under = (in_a.status.under && in_b.status.under) && !inf;

	if (out.status.inf == 0) {	// Don't flip sign if already inf
		out.status.negative = (a_neg & b_neg) | z_plus_z_sign | neg_out;	// both negative, -0 + -0, or negative output
	} else {
		out.status.negative = (inf >> 1);
	}
	DEBUG_PRINT_XFP_VAL("hmx_xfp_add out               ", out);

	return out;
}

static inline uint32_t compute_shift(int32_t base_exp, int32_t relative_exp, int32_t extra)
{
	int32_t shift = base_exp-relative_exp - extra;
	if(shift < 0 ) shift = -shift;	// I think these all should be positive since base_exp will always be the max
	if(shift > 63 ) shift = 63;
	return (uint32_t) shift;
}

#if 0
void print_bin(processor_t *proc, int64_t a, const char * v, int32_t msb_bit)
{
	DEBUG_PRINT_XFP2("%s = ", v);
	int32_t r = 4-(msb_bit+1) % 4;
	for(int32_t i = msb_bit+r; i >= 0; i-=4 ) {
		for(int32_t j = 0; j < 4; j++ ) {
			int32_t bit = i - j;
			if(bit <= msb_bit) {
				DEBUG_PRINT_XFP2("%d", (int32_t)((a>>bit) & 0x1));
			}
		}
		if (i > 4)
			DEBUG_PRINT_XFP2("_");
	}
	DEBUG_PRINT_XFP2("\n");
}
#endif
static inline uint8_t cl1(uint32_t src, const uint32_t bit)
{
	uint8_t ret = 0;
	const uint64_t msb = 1<<bit;
	for (ret = 0; src & msb; src <<= 1) {
		ret++;
	}
	return ret;
}
#if 0
void print_sequence(processor_t *proc, int64_t z, int64_t p, int64_t g,  int32_t msb_bit)
{
	DEBUG_PRINT_XFP2("seq = ");

	int32_t r = 4-(msb_bit+1) % 4;
	for(int32_t i = msb_bit+r; i >= 0; i-=4 ) {

		for(int32_t j = 0; j < 4; j++ ) {
			int32_t bit = i - j;
			if(bit <= msb_bit) {
				if ((int32_t)(z>>bit) & 0x1) {
					DEBUG_PRINT_XFP2("z");
				} else if ((int32_t)(g>>bit) & 0x1) {
					DEBUG_PRINT_XFP2("g");
				} else if ((int32_t)(p>>bit) & 0x1) {
					DEBUG_PRINT_XFP2("p") ;
				} else {
					DEBUG_PRINT_XFP2(".");
				}
			}
		}
		if (i > 4)
			DEBUG_PRINT_XFP2("_");
	}
	DEBUG_PRINT_XFP2("\n");
}
#endif


static inline uint8_t hmx_xfp_compute_lza(processor_t *proc, int64_t a, int64_t b, int32_t msb_bit)
{
	const int64_t p =  (a ^ b);
	const int64_t g =  (a & b);
	const int64_t z = ~(a | b);
	//print_bin(proc,a, "in0", msb_bit);
	//print_bin(proc,b, "in1", msb_bit);
	//print_bin(proc,z, "z  ", msb_bit);
	//print_bin(proc,p, "p  ", msb_bit);
	//print_bin(proc,g, "g  ", msb_bit);
	//print_sequence(proc,z, p, g, msb_bit);
	const int64_t msb = 1ll << msb_bit;
	const int64_t zeros = (p ^ ~(z<<1));
	const int64_t ones  = (p ^ ~(g<<1));
	const int64_t z_msb = (msb & zeros) >> msb_bit;
	const int64_t o_msb = (msb &  ones) >> msb_bit;
	const int32_t lza_z = cl1(z_msb ? zeros : ~zeros, msb_bit)-1;
	const int32_t lza_o = cl1(o_msb ? ones  : ~ones,  msb_bit)-1;

	uint8_t lza = 0;
	if (msb & z) {
		lza = lza_z;
		//DEBUG_PRINT_XFP("hmx_xfp_compute_lza            : lza=%u from zeros %d and %d", lza, lza_z, lza_o);
	} else if (msb & g) {
		lza = lza_o;
		//DEBUG_PRINT_XFP("hmx_xfp_compute_lza            : lza=%u from ones %d and %d", lza, lza_z, lza_o );
	} else {
		lza = lza_z > lza_o ? lza_z : lza_o;
		//DEBUG_PRINT_XFP("hmx_xfp_compute_lza            : lza=%u max of %d and %d", lza, lza_z, lza_o );
	}
	//print_bin(proc,zeros, "zeros ", msb_bit);
	//print_bin(proc,ones, "ones  ", msb_bit);
	return lza;
}


static inline hmx_xfp_t xfp_adder( processor_t *proc,
		hmx_xfp_t *	in,
		uint32_t N,
		uint32_t extra_adder_bit,
		uint32_t INT,
		uint32_t FRAC,
		uint32_t EXP,
		uint32_t compute_lza)
{
	hmx_xfp_t out   = {.EXP = EXP, .FRAC = FRAC, .INT = INT };

	uint8_t all_zeros = 1;
	uint8_t all_true_zeros = 1;
	uint8_t z_plus_z_sign = 0;
	uint8_t inf = 0;
//	uint8_t max_exp_idx = 0;
	out.exp = -(1 << (out.EXP-1));	// min exponent
	for(uint32_t i = 0; i < N; i++ ) {
		all_zeros &= (in[i].sig==0);
		all_true_zeros &= in[i].status.zero;
		z_plus_z_sign |= in[i].status.negative;
		inf |= in[i].status.inf;
		if (out.exp < in[i].exp) {
			out.exp = in[i].exp;
//			max_exp_idx = i;
		}
		DEBUG_PRINT_XFP_VAL("hmx_xfp_add_in                ", in[i]);
	}
	DEBUG_PRINT_XFP("hmx_xfp_add_max_exp            : in[%d] exp = %04x acc_aligned_sig=%016llx", max_exp_idx, (uint16_t)in[max_exp_idx].exp, (long long int)in[max_exp_idx].sig);
	z_plus_z_sign = all_zeros & z_plus_z_sign;
	out.sig  = 0;
	int64_t aligned_sig[4] = {0};
	for(uint32_t i = 0; i < N; i++ ) {
		int32_t delta_exp = compute_shift(out.exp, in[i].exp, 0);
		aligned_sig[i] =  right_shift_with_inexact(proc, in[i].sig, delta_exp);
		out.sig += aligned_sig[i];
		DEBUG_PRINT_XFP("hmx_xfp_add_sig addition       : in[%d].sig=%016llx aligned_sig=%016llx shift=%3d out_sig=%016llx", i, (long long int)in[i].sig, (long long int)aligned_sig[i], delta_exp, (long long int)out.sig );
	}

	if (compute_lza) {
		DEBUG_PRINT_XFP("hmx_xfp_add_lza                : start computation");
		int32_t msb_bit = FRAC + INT - 1;
		out.lza = hmx_xfp_compute_lza(proc, aligned_sig[0], aligned_sig[1], msb_bit);
		//print_bin(proc, out.sig,          "raw sig           : ", msb_bit);
		//print_bin(proc, out.sig<<out.lza, "sig shifted by lza: ", msb_bit);
		DEBUG_PRINT_XFP("hmx_xfp_add_lza                : lza shift=%u ", out.lza );
	}

	out.sig = (out.sig >> extra_adder_bit) | (out.sig & 0x1); // Remove extra bit
	out.status.zero = all_true_zeros; 	// Can't have true zero coming out of the adder, yet.
	out.status.inf =  inf;	// Doesn't change from input
	out.status.negative =  (out.status.inf == 0) ? (z_plus_z_sign | (out.sig < 0)) : (inf >> 1);	// 0 + (-) 0, or negative output
	DEBUG_PRINT_XFP_VAL("hmx_xfp_add_out               ", out);
	return out;
}



hmx_xfp_t HMX_XFP_FUNCTION(hmx_xfp_add_Nway_2stage)(
		processor_t *proc,
		usr_fp_reg_t usr,
		hmx_xfp_t *	in,
		hmx_xfp_t 	acc,
		uint32_t N,
		uint32_t extra_adder_bit)
{
	const uint32_t use_lza_norm = 1;
	const uint32_t no_lza_norm = 0;
	const uint32_t no_extra_adder_bit = 0;
	uint32_t acc_alignment = acc.FRAC - in[0].FRAC;
	for(int32_t i = 0; i < N; i++) {
		DEBUG_PRINT_XFP_VAL("hmx_xfp_add_val_in_aligned    ", in[i]);
		in[i].sig <<= acc_alignment;
		in[i].FRAC += acc_alignment;

	}
	DEBUG_PRINT_XFP("hmx_xfp_add                    : first stage - 4 way add");
	hmx_xfp_t intermediate = xfp_adder( proc, in, N, no_extra_adder_bit, acc.INT, acc.FRAC, acc.EXP, no_lza_norm);
    hmx_xfp_t in_2way[2] = {acc, intermediate};
	DEBUG_PRINT_XFP("hmx_xfp_add                    : second stage - 2 way add");
	hmx_xfp_t adder_out = xfp_adder(  proc,  in_2way, 2, no_extra_adder_bit, acc.INT+1, acc.FRAC, acc.EXP, use_lza_norm);
	hmx_xfp_t out = hmx_xfp_normalize(proc, usr, adder_out, acc.INT, acc.FRAC, acc.EXP, use_lza_norm);

	return out;
}

hmx_xfp_t HMX_XFP_FUNCTION(hmx_xfp_mult)(
		processor_t *proc,
		usr_fp_reg_t usr,
		hmx_xfp_t in_a,
		hmx_xfp_t in_b,
		uint32_t exp_out)
{

	hmx_xfp_t out = {.EXP = exp_out, .FRAC = 2*in_a.FRAC, .INT = 2*in_a.INT-1 };

	DEBUG_PRINT_XFP_VAL("hmx_xfp_mult in0              ", in_a);
	DEBUG_PRINT_XFP_VAL("hmx_xfp_mult in1              ", in_b);

	out.sig = (int64_t)in_a.sig * (int64_t)in_b.sig; //
	out.exp =  in_a.exp + in_b.exp;
	out.status.zero = in_a.status.zero || in_b.status.zero;	// propagate true zero
	out.status.negative = (in_a.status.negative && !in_b.status.negative) || (!in_a.status.negative && in_b.status.negative);
	out.status.under = in_a.status.under | in_b.status.under;

	uint8_t inf_a = ((in_a.status.inf == 1) || (in_a.status.inf == 2));
	uint8_t inf_b = ((in_b.status.inf == 1) || (in_b.status.inf == 2));

	uint8_t z_a = in_a.sig == 0;
	uint8_t z_b = in_b.sig == 0;

	out.status.in0_zero = z_a || in_a.status.zero;
	out.status.in1_zero = z_b || in_b.status.zero;

	if ((in_a.status.inf == 3) || (in_b.status.inf == 3)) {	// NaN in, Nan out
		out.status.inf = usr.nan_propagate ? 3: 0;
	} else if (out.status.under && (inf_a || inf_b) ) {
		out.status.inf = usr.nan_propagate ? 3 : 0;	// Inf * 0
	} else if (inf_a && inf_b) { // Inf * Inf case
		out.status.inf = (out.status.negative) ? 2 : 1;
	} else if ( (inf_a && z_b ) || (inf_b && z_a) ) {
		out.status.inf  = usr.nan_propagate ? 3 : 0;	// Inf * 0
		out.status.zero = usr.nan_propagate ? 0 : 1;
	} else if (inf_a || inf_b) {
		out.status.inf = (out.status.negative) ? 2 : 1; // Inf * x, 0 < x < Inf = Inf
	}

	if (out.status.zero) {	// True Zero Output
		out.exp = -(1 << (out.EXP-1));
		out.sig = 0;
	} else if (z_a || z_b) { // Cancelleation Zero
		out.exp = -(1 << (out.EXP-1));
		out.sig = 0;
		out.status.negative = 0;
	}

	DEBUG_PRINT_XFP_VAL("hmx_xfp_mult out              ", out);
	return out;
}

// convert type from uint16_t fp to hmx_fp_t
hmx_fp_t HMX_XFP_FUNCTION(fp_to_hmx_fp)(
	processor_t *proc,
	uint32_t 	in,
	uint32_t	exp,
	uint32_t	frac)
{

	hmx_fp_t hmx_fp_in = {0};
	// mask
	uint32_t exp_mask = ((1 << exp) - 1) << frac;
	uint32_t sign_mask = 1 << (exp + frac);
	uint32_t frac_mask = (1 << frac) - 1;
	// expo part for in
	uint32_t in_exp = (in & exp_mask) >> frac;
	uint32_t in_sign = (in & sign_mask) >> (exp + frac);
	uint64_t in_frac = in & frac_mask;

	// assign value
	hmx_fp_in.EXP = exp;
	hmx_fp_in.FRAC = frac;
	hmx_fp_in.exp = in_exp;
	hmx_fp_in.frac = in_frac;
	hmx_fp_in.sign = in_sign;

	return hmx_fp_in;
}

hmx_xfp_t HMX_XFP_FUNCTION(hmx_xfp_zero)(processor_t *proc)
{
	usr_fp_reg_t usr = {0};
	return hmx_fp_xfp(proc, usr, 0x0000, 10, 5, proc->arch_proc_options->QDSP6_MX_FP_ACC_INT, proc->arch_proc_options->QDSP6_MX_FP_ACC_FRAC, proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP, 0);
}

#define XFP_MIN 0
#define XFP_MAX 1

hmx_xfp_t HMX_XFP_FUNCTION(xfp_cmp)(processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t a, hmx_xfp_t b, int32_t min_max);
hmx_xfp_t HMX_XFP_FUNCTION(xfp_cmp)(processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t a, hmx_xfp_t b, int32_t min_max)
{
	DEBUG_PRINT_XFP_VAL("xfp_cmp                       ", a);
	DEBUG_PRINT_XFP_VAL("xfp_cmp                       ", b);

	hmx_xfp_t x = (min_max) ? b : a;
	hmx_xfp_t y = (min_max) ? a : b;

	if (a.status.inf  | b.status.inf)
	{
		DEBUG_PRINT_XFP("xfp_%s infinity/nan case      : nan_propagate=%d inf_nan_enable=%d", min_max ? "max" : "min", usr.nan_propagate, usr.inf_nan_enable);
		// Nan cases, Nan result wins or loses based on USR bit
		if (a.status.inf == 3)  {
			return usr.nan_propagate ? a : b;
		} else if (b.status.inf == 3)  {
			return usr.nan_propagate ? b : a;
		}
		// Negative Infinity always wins
		if (a.status.inf == 2)   {
			return x;
		} else if (b.status.inf == 2) {
			return y;
		}
		// Positive Inf always loses
		return (a.status.inf == 1) ? y : x;
	}

	// Negative number wins in case of mismatched signs
	uint8_t neg_a = (a.sig < 0);
	uint8_t neg_b = (b.sig < 0);
	if (neg_a ^ neg_b) {
		DEBUG_PRINT_XFP("xfp_%s mismatched signs       : ", min_max ? "max" : "min");
		return neg_a ? x : y;
	}

	// Different exponents. Assumption here is that both are fully normalized
	// as they are coming from an IEEE input
	if ( a.exp != b.exp) {
		DEBUG_PRINT_XFP("xfp_%s diff exponents         : ", min_max ? "max" : "min");
		return (neg_a) ? (( a.exp < b.exp) ? y : x) : (( a.exp < b.exp) ? x : y);
	}

	DEBUG_PRINT_XFP("xfp_%s equal exponents        : ", min_max ? "max" : "min");
	// Exponents are equal
	return (a.sig <= b.sig) ? x : y;
}

static inline uint32_t hmx_xfp_fp_cvt(processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t acc, hmx_bias_flt_poly_t bias_reg, uint32_t cvt_feedback, const hmx_cvt_rs_reg_t rs, const uint32_t fp_frac, const uint32_t fp_exp)  {

	const uint32_t exp_out = 8;
	const uint32_t frac_out = proc->arch_proc_options->xfp_cvt_frac;
	const uint32_t int_out = proc->arch_proc_options->xfp_cvt_int;
	const uint32_t input_norm = 0;

	usr_fp_reg_t usr_tmp_overwrite;
	usr_tmp_overwrite.raw = 0xFFFFFFFF; // Bits 21:20 forced to 1 elsewhere
	usr.inf_nan_enable = 0; // Only bit 21 used for now in compare
	// Convert bias parameters from FP16 to XFP
	uint32_t extra = 5;
	uint32_t val = ((uint32_t)bias_reg.acc_bias << extra) | (uint32_t)bias_reg.acc_bias_extra;

	DEBUG_PRINT_XFP("xfp_fp_cvt acc_bias from reg   : acc_bias=%04x extra=%02x", (uint16_t)bias_reg.acc_bias, (uint8_t)bias_reg.acc_bias_extra);

	hmx_xfp_t acc_bias = HMX_XFP_FUNCTION(hmx_fp_xfp)(proc, usr_tmp_overwrite, val, fp_frac+extra, fp_exp, int_out, proc->arch_proc_options->QDSP6_MX_FP_ACC_FRAC , proc->arch_proc_options->QDSP6_MX_FP_ACC_EXP, input_norm);

	extra = 4;

	DEBUG_PRINT_XFP("xfp_fp_cvt scale from reg      : scale=%04x extra=%02x ", (uint16_t)bias_reg.scale, (uint8_t)bias_reg.scale_extra);
	val = ((uint32_t)bias_reg.scale << extra) | (uint32_t)bias_reg.scale_extra;
	hmx_xfp_t scale    = HMX_XFP_FUNCTION(hmx_fp_xfp)(proc,  usr_tmp_overwrite, val, fp_frac+extra, fp_exp, int_out, frac_out+1, exp_out, input_norm);


	if ( rs.fb_dst == 0x2 )
	{
		DEBUG_PRINT_XFP("xfp_fp_cvt scale from feedback : cvt_feedback=%04x extra=%02x rs.fb_dst=%d rs.fb_limit=%s",  (uint16_t)(cvt_feedback>>4), (uint8_t)(cvt_feedback & 0xF), rs.fb_dst, rs.fb_limit ? "Max" : "Min");
		hmx_xfp_t feedback = HMX_XFP_FUNCTION(hmx_fp_xfp)(proc, usr_tmp_overwrite, cvt_feedback, fp_frac+extra, fp_exp, int_out, frac_out+1, exp_out, input_norm);
		scale = HMX_XFP_FUNCTION(xfp_cmp)(proc, usr, scale, feedback, rs.fb_limit);
		DEBUG_PRINT_XFP_VAL("xfp_fp_cvt cmp scale selected ", scale);
	}

	scale.FRAC--;
	scale.sig >>=1;
	DEBUG_PRINT_XFP_VAL("xfp_fp_cvt scale correction   ", scale);

	// Bias Accumulator and normalize
	hmx_xfp_t acc_biased     = HMX_XFP_FUNCTION(hmx_xfp_add)(proc,  usr_tmp_overwrite, acc, acc_bias); 															// Add the bias
	hmx_xfp_t acc_normalized = HMX_XFP_FUNCTION(hmx_xfp_cvt_normalize)(proc, usr_tmp_overwrite, acc_biased, int_out, frac_out, exp_out);									// Normalize bias addition

	// Scale Normalized Accumulator
	scale.EXP++;
	hmx_xfp_t acc_scaled 	 = HMX_XFP_FUNCTION(hmx_xfp_mult)(proc, usr_tmp_overwrite, acc_normalized, scale, scale.EXP); 															// Scale accumulator
	acc_scaled.EXP = scale.EXP;

	// Add Output bias
	DEBUG_PRINT_XFP("xfp_fp_cvt outbias from reg    : out_bias=%04x extra=%02x", (uint16_t)bias_reg.out_bias, (uint8_t)bias_reg.out_bias_extra);
	val = ((uint32_t)bias_reg.out_bias << extra) | (uint32_t)bias_reg.out_bias_extra;
	hmx_xfp_t out_bias = HMX_XFP_FUNCTION(hmx_fp_xfp)(proc, usr_tmp_overwrite, val, fp_frac+extra, fp_exp, int_out, frac_out, exp_out, input_norm);

	if ( rs.fb_dst == 0x1 )
	{
		DEBUG_PRINT_XFP("xfp_fp_cvt bias from feedback  : cvt_feedback=%04x extra=%02x rs.fb_dst=%d rs.fb_limit=%s",  (uint16_t)(cvt_feedback>>4), (uint8_t)(cvt_feedback & 0xF), rs.fb_dst, rs.fb_limit  ? "Max" : "Min");
		hmx_xfp_t feedback = HMX_XFP_FUNCTION(hmx_fp_xfp)(proc, usr_tmp_overwrite, cvt_feedback, fp_frac+extra, fp_exp, int_out, frac_out, exp_out, input_norm);
		out_bias = HMX_XFP_FUNCTION(xfp_cmp)(proc, usr, out_bias, feedback, rs.fb_limit);
		DEBUG_PRINT_XFP_VAL("xfp_fp_cvt cmp bias selected ", out_bias);
	}
	out_bias.sig <<= (acc_scaled.FRAC - out_bias.FRAC); // align

	// Final output in FP16/BF16
	hmx_xfp_t acc_final  = HMX_XFP_FUNCTION(hmx_xfp_add)(proc, usr_tmp_overwrite, acc_scaled, out_bias); 																	// Now add output bias
	return HMX_XFP_FUNCTION(hmx_xfp_fp)(proc, usr_tmp_overwrite, acc_final, (fp_frac + rs.fp_rnd*4), fp_exp, rs, usr);									// Round to FP16/BF16
}


uint32_t HMX_XFP_FUNCTION(hmx_xfp_cvt)(processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t acc, hmx_bias_flt_poly_t bias_reg, uint32_t cvt_feedback, const hmx_cvt_rs_reg_t rs )  {
	if(proc->arch_proc_options->QDSP6_MX_FP_PRESENT == 0) return 0;
	const uint32_t mantissa_bits = (rs.fp_type ? 7 : 10) ;	// 4 additional bits depending on rounding location
	const uint32_t exp_bits = rs.fp_type ? 8 : 5 ;
	return  hmx_xfp_fp_cvt(proc, usr, acc, bias_reg, cvt_feedback, rs, mantissa_bits, exp_bits);
}
uint32_t HMX_XFP_FUNCTION(hmx_xfp_cvt_debug)(processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t acc, hmx_bias_flt_poly_t bias_reg, uint32_t cvt_feedback, const hmx_cvt_rs_reg_t rs )  {
  return HMX_XFP_FUNCTION(hmx_xfp_cvt)(proc, usr, acc, bias_reg, cvt_feedback, rs );
}

hmx_xfp_t HMX_XFP_FUNCTION(hmx_fp_xfp_mult)(processor_t *proc, usr_fp_reg_t usr, uint16_t A,  uint16_t B, uint32_t is_bf16 ) {
	if(proc->arch_proc_options->QDSP6_MX_FP_PRESENT == 0) return hmx_xfp_zero(proc);

	const uint32_t exp_hf = is_bf16 ? 8 : 5;
	const uint32_t frac_hf = is_bf16 ? 7 : 10 ;
	const uint32_t exp_xfp = 8;
	const uint32_t frac_xfp = 9;
	const uint32_t int_xfp = 3;
	const uint32_t input_norm = is_bf16 ? 0 : 1;
	usr.nan_propagate = 1;	// Mac side ignores nan-propagate flag, always propagate the nan
	hmx_xfp_t in_xfp_a = HMX_XFP_FUNCTION(hmx_fp_xfp)(  proc, usr, A, frac_hf, exp_hf, int_xfp, frac_xfp, exp_xfp, input_norm);
	hmx_xfp_t in_xfp_b = HMX_XFP_FUNCTION(hmx_fp_xfp)(  proc, usr, B, frac_hf, exp_hf, int_xfp, frac_xfp, exp_xfp, input_norm);
	hmx_xfp_t out_xfp  = HMX_XFP_FUNCTION(hmx_xfp_mult)(proc, usr, in_xfp_a, in_xfp_b, exp_xfp+1 );
	return out_xfp;
}




hmx_xfp_t HMX_XFP_FUNCTION(hmx_xfp_mac_reduce)(processor_t *proc, usr_fp_reg_t usr, hmx_xfp_t * A, hmx_xfp_t acc, uint32_t rate) {
	DEBUG_PRINT_XFP("-----------Accumulation-------");
	DEBUG_PRINT_XFP_VAL("hmx_xfp_mac_reduce accumulator", acc);

	const uint32_t extra_adder_bit = 1; //TODO: Parameter

	hmx_xfp_t acc_final = {0};
	if (usr.inf_nan_enable) {
		acc_final = HMX_XFP_FUNCTION(hmx_xfp_add_Nway_2stage)(proc, usr, A, acc, rate, extra_adder_bit);
	}
	else
	{
		int32_t all_zero0 = 1;
		int32_t all_zero1 = 1;
		for(int32_t prod_idx = 0; prod_idx < rate; prod_idx++) {
			all_zero0 &= A[prod_idx].status.in0_zero;
			all_zero1 &= A[prod_idx].status.in1_zero;
		}
		if (all_zero0 || all_zero1) {
			acc_final = acc;
			DEBUG_PRINT_XFP("hmx_xfp_mac_reduce             : all zeros on rows: %d cols: %d, skipping MAC operation", all_zero0, all_zero1);
		} else {
			acc_final = HMX_XFP_FUNCTION(hmx_xfp_add_Nway_2stage)(proc, usr, A, acc, rate, extra_adder_bit);
		}
	}

#if defined (STANDALONE)
	uint32_t ovf = 0;
	int32_t exponent = 0;
	size16s_t acc2 = hmx_xfp_to_tb_callback(proc, &exponent, &ovf, acc_final);
	DEBUG_PRINT_XFP("XFP Exponent: %2d, Significand: 0x%02x.%016llx, Overflow: %s", exponent, (uint8_t)(acc2.hi>>58) & 0xFF, (long long int)(acc2.hi<<6), (ovf==1) ? "+Inf" : ((ovf==2) ? "-Inf" : (ovf==0) ? "0" : "NaN"));
#endif
	return acc_final;
}

size16s_t HMX_XFP_FUNCTION(hmx_xfp_to_tb_callback)(processor_t *proc, int32_t * exponent, uint32_t * ovf, hmx_xfp_t acc) {
	size16s_t acc_legacy = {.hi = acc.sig << (64-(proc->arch_proc_options->QDSP6_MX_FP_ACC_FRAC+proc->arch_proc_options->QDSP6_MX_FP_ACC_INT)), .lo = 0};
	*exponent = acc.exp;
	*ovf = acc.status.inf;
	if (acc.status.inf) {
		acc_legacy.hi = 0;
		*exponent = 0;
		DEBUG_PRINT_XFP(    "xfp to legacy acc              : inf=%d reporting exp and sig as zeros", 	acc.status.inf);
	}
	return acc_legacy;
}
