/*
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

/*
 * Test instructions that might set bits in user status register (USR)
 */

#include <stdio.h>
#include <stdint.h>

int err;

static void __check(int line, uint32_t val, uint32_t expect)
{
    if (val != expect) {
        printf("ERROR at line %d: %d != %d\n", line, val, expect);
        err++;
    }
}

#define check(RES, EXP) __check(__LINE__, RES, EXP)

static void __check32(int line, uint32_t val, uint32_t expect)
{
    if (val != expect) {
        printf("ERROR at line %d: 0x%08x != 0x%08x\n", line, val, expect);
        err++;
    }
}

#define check32(RES, EXP) __check32(__LINE__, RES, EXP)

static void __check64(int line, uint64_t val, uint64_t expect)
{
    if (val != expect) {
        printf("ERROR at line %d: 0x%016llx != 0x%016llx\n", line, val, expect);
        err++;
    }
}

#define check64(RES, EXP) __check64(__LINE__, RES, EXP)

/*
 * Some of the instructions tested are only available on certain versions
 * of the Hexagon core
 */
#define CORE_HAS_AUDIO    (__HEXAGON_ARCH__ >= 67 && defined(__HEXAGON_AUDIO__))
#define CORE_IS_V67       (__HEXAGON_ARCH__ >= 67)

/* Define the bits in Hexagon USR register */
#define USR_OVF_BIT          0        /* Sticky saturation overflow */
#define USR_FPINVF_BIT       1        /* IEEE FP invalid sticky flag */
#define USR_FPDBZF_BIT       2        /* IEEE FP divide-by-zero sticky flag */
#define USR_FPOVFF_BIT       3        /* IEEE FP overflow sticky flag */
#define USR_FPUNFF_BIT       4        /* IEEE FP underflow sticky flag */
#define USR_FPINPF_BIT       5        /* IEEE FP inexact sticky flag */

/* Some useful floating point values */
const uint32_t SF_INF =              0x7f800000;
const uint32_t SF_QNaN =             0x7fc00000;
const uint32_t SF_SNaN =             0x7fb00000;
const uint32_t SF_QNaN_neg =         0xffc00000;
const uint32_t SF_SNaN_neg =         0xffb00000;
const uint32_t SF_HEX_NaN =          0xffffffff;
const uint32_t SF_zero =             0x00000000;
const uint32_t SF_one =              0x3f800000;
const uint32_t SF_one_recip =        0x3f7f0001;         /* 0.9960...  */
const uint32_t SF_one_invsqrta =     0x3f7f0000;         /* 0.99609375 */
const uint32_t SF_two =              0x40000000;
const uint32_t SF_four =             0x40800000;
const uint32_t SF_small_neg =        0xab98fba8;
const uint32_t SF_large_pos =        0x5afa572e;

const uint64_t DF_QNaN =             0x7ff8000000000000ULL;
const uint64_t DF_SNaN =             0x7ff7000000000000ULL;
const uint64_t DF_QNaN_neg =         0xfff8000000000000ULL;
const uint64_t DF_SNaN_neg =         0xfff7000000000000ULL;
const uint64_t DF_HEX_NaN =          0xffffffffffffffffULL;
const uint64_t DF_zero =             0x0000000000000000ULL;
const uint64_t DF_any =              0x3f80000000000000ULL;
const uint64_t DF_one =              0x3ff0000000000000ULL;
const uint64_t DF_one_hh =           0x3ff001ff80000000ULL;     /* 1.00048... */
const uint64_t DF_small_neg =        0xbd731f7500000000ULL;
const uint64_t DF_large_pos =        0x7f80000000000001ULL;

/*
 * Templates for functions to execute an instruction
 *
 * The templates vary by the number of arguments and the types of the args
 * and result.  We use one letter in the macro name for the result and each
 * argument:
 *     x             unknown (specified in a subsequent template) or don't care
 *     R             register (32 bits)
 *     P             pair (64 bits)
 *     p             predicate
 *     I             immediate
 *     Xx            read/write
 */

#define CLEAR_USRBITS \
    "r2 = usr\n\t" \
    "r2 = clrbit(r2, #0)\n\t" \
    "r2 = clrbit(r2, #1)\n\t" \
    "r2 = clrbit(r2, #2)\n\t" \
    "r2 = clrbit(r2, #3)\n\t" \
    "r2 = clrbit(r2, #4)\n\t" \
    "r2 = clrbit(r2, #5)\n\t" \
    "usr = r2\n\t"

/* Template for instructions with one register operand */
#define FUNC_x_OP_x(RESTYPE, SRCTYPE, NAME, INSN, BIT) \
static RESTYPE NAME(SRCTYPE src, uint32_t *usr_result) \
{ \
    RESTYPE result; \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        INSN  "\n\t" \
        "%1 = usr\n\t" \
        : "=r"(result), "=r"(usr) \
        : "r"(src) \
        : "r2", "usr"); \
      *usr_result = ((usr >> (BIT)) & 1); \
      return result; \
}

#define FUNC_R_OP_R(NAME, INSN, BIT) \
FUNC_x_OP_x(uint32_t, uint32_t, NAME, INSN, BIT)

#define FUNC_R_OP_P(NAME, INSN, BIT) \
FUNC_x_OP_x(uint32_t, uint64_t, NAME, INSN, BIT)

#define FUNC_P_OP_P(NAME, INSN, BIT) \
FUNC_x_OP_x(uint64_t, uint64_t, NAME, INSN, BIT)

#define FUNC_P_OP_R(NAME, INSN, BIT) \
FUNC_x_OP_x(uint64_t, uint32_t, NAME, INSN, BIT)

/*
 * Template for instructions with a register and predicate result
 * and one register operand
 */
#define FUNC_xp_OP_x(RESTYPE, SRCTYPE, NAME, INSN, BIT) \
static RESTYPE NAME(SRCTYPE src, uint8_t *pred_result, uint32_t *usr_result) \
{ \
    RESTYPE result; \
    uint8_t pred; \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        INSN  "\n\t" \
        "%1 = p2\n\t" \
        "%2 = usr\n\t" \
        : "=r"(result), "=r"(pred), "=r"(usr) \
        : "r"(src) \
        : "r2", "p2", "usr"); \
    *pred_result = pred; \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_Rp_OP_R(NAME, INSN, BIT) \
FUNC_xp_OP_x(uint32_t, uint32_t, NAME, INSN, BIT)

/* Template for instructions with two register operands */
#define FUNC_x_OP_xx(RESTYPE, SRC1TYPE, SRC2TYPE, NAME, INSN, BIT) \
static RESTYPE NAME(SRC1TYPE src1, SRC2TYPE src2, uint32_t *usr_result) \
{ \
    RESTYPE result; \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        INSN "\n\t" \
        "%1 = usr\n\t" \
        : "=r"(result), "=r"(usr) \
        : "r"(src1), "r"(src2) \
        : "r2", "usr"); \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_P_OP_PP(NAME, INSN, BIT) \
FUNC_x_OP_xx(uint64_t, uint64_t, uint64_t, NAME, INSN, BIT)

#define FUNC_R_OP_PP(NAME, INSN, BIT) \
FUNC_x_OP_xx(uint32_t, uint64_t, uint64_t, NAME, INSN, BIT)

#define FUNC_P_OP_RR(NAME, INSN, BIT) \
FUNC_x_OP_xx(uint64_t, uint32_t, uint32_t, NAME, INSN, BIT)

#define FUNC_R_OP_RR(NAME, INSN, BIT) \
FUNC_x_OP_xx(uint32_t, uint32_t, uint32_t, NAME, INSN, BIT)

#define FUNC_R_OP_PR(NAME, INSN, BIT) \
FUNC_x_OP_xx(uint32_t, uint64_t, uint32_t, NAME, INSN, BIT)

#define FUNC_P_OP_PR(NAME, INSN, BIT) \
FUNC_x_OP_xx(uint64_t, uint64_t, uint32_t, NAME, INSN, BIT)

/*
 * Template for instructions with a register and predicate result
 * and two register operands
 */
#define FUNC_xp_OP_xx(RESTYPE, SRC1TYPE, SRC2TYPE, NAME, INSN, BIT) \
static RESTYPE NAME(SRC1TYPE src1, SRC2TYPE src2, \
                    uint8_t *pred_result, uint32_t *usr_result) \
{ \
    RESTYPE result; \
    uint8_t pred; \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        INSN  "\n\t" \
        "%1 = p2\n\t" \
        "%2 = usr\n\t" \
        : "=r"(result), "=r"(pred), "=r"(usr) \
        : "r"(src1), "r"(src2) \
        : "r2", "p2", "usr"); \
    *pred_result = pred; \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_Rp_OP_RR(NAME, INSN, BIT) \
FUNC_xp_OP_xx(uint32_t, uint32_t, uint32_t, NAME, INSN, BIT)

/* Template for instructions with one register and one immediate */
#define FUNC_x_OP_xI(RESTYPE, SRC1TYPE, NAME, INSN, BIT) \
static RESTYPE NAME(SRC1TYPE src1, int32_t src2, uint32_t *usr_result) \
{ \
    RESTYPE result; \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        INSN "\n\t" \
        "%1 = usr\n\t" \
        : "=r"(result), "=r"(usr) \
        : "r"(src1), "i"(src2) \
        : "r2", "usr"); \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_R_OP_RI(NAME, INSN, BIT) \
FUNC_x_OP_xI(uint32_t, uint32_t, NAME, INSN, BIT)

#define FUNC_R_OP_PI(NAME, INSN, BIT) \
FUNC_x_OP_xI(uint32_t, uint64_t, NAME, INSN, BIT)

/*
 * Template for instructions with a read/write result
 * and two register operands
 */
#define FUNC_Xx_OP_xx(RESTYPE, SRC1TYPE, SRC2TYPE, NAME, INSN, BIT) \
static RESTYPE NAME(RESTYPE result, SRC1TYPE src1, SRC2TYPE src2, \
                    uint32_t *usr_result) \
{ \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        INSN "\n\t" \
        "%1 = usr\n\t" \
        : "+r"(result), "=r"(usr) \
        : "r"(src1), "r"(src2) \
        : "r2", "usr"); \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_XR_OP_RR(NAME, INSN, BIT) \
FUNC_Xx_OP_xx(uint32_t, uint32_t, uint32_t, NAME, INSN, BIT)

#define FUNC_XP_OP_PP(NAME, INSN, BIT) \
FUNC_Xx_OP_xx(uint64_t, uint64_t, uint64_t, NAME, INSN, BIT)

#define FUNC_XP_OP_RR(NAME, INSN, BIT) \
FUNC_Xx_OP_xx(uint64_t, uint32_t, uint32_t, NAME, INSN, BIT)

/*
 * Template for instructions with a read/write result
 * and two register operands
 */
#define FUNC_Xxp_OP_xx(RESTYPE, SRC1TYPE, SRC2TYPE, NAME, INSN, BIT) \
static RESTYPE NAME(RESTYPE result, SRC1TYPE src1, SRC2TYPE src2, \
                    uint8_t *pred_result, uint32_t *usr_result) \
{ \
    uint32_t usr; \
    uint8_t pred; \
    asm(CLEAR_USRBITS \
        INSN "\n\t" \
        "%1 = p2\n\t" \
        "%2 = usr\n\t" \
        : "+r"(result), "=r"(pred), "=r"(usr) \
        : "r"(src1), "r"(src2) \
        : "r2", "usr"); \
    *pred_result = pred; \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_XPp_OP_PP(NAME, INSN, BIT) \
FUNC_Xxp_OP_xx(uint64_t, uint64_t, uint64_t, NAME, INSN, BIT)

/*
 * Template for instructions with a read/write result and
 * two register and one predicate operands
 */
#define FUNC_Xx_OP_xxp(RESTYPE, SRC1TYPE, SRC2TYPE, NAME, INSN, BIT) \
static RESTYPE NAME(RESTYPE result, SRC1TYPE src1, SRC2TYPE src2, uint8_t pred,\
                    uint32_t *usr_result) \
{ \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        "p2 = %4\n\t" \
        INSN "\n\t" \
        "%1 = usr\n\t" \
        : "+r"(result), "=r"(usr) \
        : "r"(src1), "r"(src2), "r"(pred) \
        : "r2", "p2", "usr"); \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_XR_OP_RRp(NAME, INSN, BIT) \
FUNC_Xx_OP_xxp(uint32_t, uint32_t, uint32_t, NAME, INSN, BIT)

/* Template for compare instructions with two register operands */
#define FUNC_CMP_xx(SRC1TYPE, SRC2TYPE, NAME, INSN, BIT) \
static uint32_t NAME(SRC1TYPE src1, SRC2TYPE src2, uint32_t *usr_result) \
{ \
    uint32_t result; \
    uint32_t usr; \
    asm(CLEAR_USRBITS \
        INSN "\n\t" \
        "%0 = p1\n\t" \
        "%1 = usr\n\t" \
        : "=r"(result), "=r"(usr) \
        : "r"(src1), "r"(src2) \
        : "p1", "r2", "usr"); \
    *usr_result = ((usr >> (BIT)) & 1); \
    return result; \
}

#define FUNC_CMP_RR(NAME, INSN, BIT) \
FUNC_CMP_xx(uint32_t, uint32_t, NAME, INSN, BIT)

#define FUNC_CMP_PP(NAME, INSN, BIT) \
FUNC_CMP_xx(uint64_t, uint64_t, NAME, INSN, BIT)

/*
 * Function declarations using the templates
 */
FUNC_R_OP_R(satub,              "%0 = satub(%2)",                   USR_OVF_BIT)
FUNC_P_OP_PP(vaddubs,           "%0 = vaddub(%2, %3):sat",          USR_OVF_BIT)
FUNC_P_OP_PP(vadduhs,           "%0 = vadduh(%2, %3):sat",          USR_OVF_BIT)
FUNC_P_OP_PP(vsububs,           "%0 = vsubub(%2, %3):sat",          USR_OVF_BIT)
FUNC_P_OP_PP(vsubuhs,           "%0 = vsubuh(%2, %3):sat",          USR_OVF_BIT)

/* Add vector of half integers with saturation and pack to unsigned bytes */
FUNC_R_OP_PP(vaddhubs,          "%0 = vaddhub(%2, %3):sat",         USR_OVF_BIT)

/* Vector saturate half to unsigned byte */
FUNC_R_OP_P(vsathub,            "%0 = vsathub(%2)",                 USR_OVF_BIT)

/* Similar to above but takes a 32-bit argument */
FUNC_R_OP_R(svsathub,           "%0 = vsathub(%2)",                 USR_OVF_BIT)

/* Vector saturate word to unsigned half */
FUNC_P_OP_P(vsatwuh_nopack,     "%0 = vsatwuh(%2)",                 USR_OVF_BIT)

/* Similar to above but returns a 32-bit result */
FUNC_R_OP_P(vsatwuh,            "%0 = vsatwuh(%2)",                 USR_OVF_BIT)

/* Vector arithmetic shift halfwords with saturate and pack */
FUNC_R_OP_PI(asrhub_sat,        "%0 = vasrhub(%2, #%3):sat",        USR_OVF_BIT)

/* Vector arithmetic shift halfwords with round, saturate and pack */
FUNC_R_OP_PI(asrhub_rnd_sat,    "%0 = vasrhub(%2, #%3):raw",        USR_OVF_BIT)

FUNC_R_OP_RR(addsat,            "%0 = add(%2, %3):sat",             USR_OVF_BIT)
/* Similar to above but with register pairs */
FUNC_P_OP_PP(addpsat,           "%0 = add(%2, %3):sat",             USR_OVF_BIT)

FUNC_XR_OP_RR(mpy_acc_sat_hh_s0, "%0 += mpy(%2.H, %3.H):sat",       USR_OVF_BIT)
FUNC_R_OP_RR(mpy_sat_hh_s1,     "%0 = mpy(%2.H, %3.H):<<1:sat",     USR_OVF_BIT)
FUNC_R_OP_RR(mpy_sat_rnd_hh_s1, "%0 = mpy(%2.H, %3.H):<<1:rnd:sat", USR_OVF_BIT)
FUNC_R_OP_RR(mpy_up_s1_sat,     "%0 = mpy(%2, %3):<<1:sat",         USR_OVF_BIT)
FUNC_P_OP_RR(vmpy2s_s1,         "%0 = vmpyh(%2, %3):<<1:sat",       USR_OVF_BIT)
FUNC_P_OP_RR(vmpy2su_s1,        "%0 = vmpyhsu(%2, %3):<<1:sat",     USR_OVF_BIT)
FUNC_R_OP_RR(vmpy2s_s1pack,     "%0 = vmpyh(%2, %3):<<1:rnd:sat",   USR_OVF_BIT)
FUNC_P_OP_PP(vmpy2es_s1,        "%0 = vmpyeh(%2, %3):<<1:sat",      USR_OVF_BIT)
FUNC_R_OP_PP(vdmpyrs_s1,        "%0 = vdmpy(%2, %3):<<1:rnd:sat",   USR_OVF_BIT)
FUNC_XP_OP_PP(vdmacs_s0,        "%0 += vdmpy(%2, %3):sat",          USR_OVF_BIT)
FUNC_R_OP_RR(cmpyrs_s0,         "%0 = cmpy(%2, %3):rnd:sat",        USR_OVF_BIT)
FUNC_XP_OP_RR(cmacs_s0,         "%0 += cmpy(%2, %3):sat",           USR_OVF_BIT)
FUNC_XP_OP_RR(cnacs_s0,         "%0 -= cmpy(%2, %3):sat",           USR_OVF_BIT)
FUNC_P_OP_PP(vrcmpys_s1_h,      "%0 = vrcmpys(%2, %3):<<1:sat:raw:hi",
                                                                    USR_OVF_BIT)
FUNC_XP_OP_PP(mmacls_s0,        "%0 += vmpyweh(%2, %3):sat",        USR_OVF_BIT)
FUNC_R_OP_RR(hmmpyl_rs1,        "%0 = mpy(%2, %3.L):<<1:rnd:sat",   USR_OVF_BIT)
FUNC_XP_OP_PP(mmaculs_s0,       "%0 += vmpyweuh(%2, %3):sat",       USR_OVF_BIT)
FUNC_R_OP_PR(cmpyi_wh,          "%0 = cmpyiwh(%2, %3):<<1:rnd:sat", USR_OVF_BIT)
FUNC_P_OP_PP(vcmpy_s0_sat_i,    "%0 = vcmpyi(%2, %3):sat",          USR_OVF_BIT)
FUNC_P_OP_PR(vcrotate,          "%0 = vcrotate(%2, %3)",            USR_OVF_BIT)
FUNC_P_OP_PR(vcnegh,            "%0 = vcnegh(%2, %3)",              USR_OVF_BIT)

#if CORE_HAS_AUDIO
FUNC_R_OP_PP(wcmpyrw,           "%0 = cmpyrw(%2, %3):<<1:sat",      USR_OVF_BIT)
#endif

FUNC_R_OP_RR(addh_l16_sat_ll,   "%0 = add(%2.L, %3.L):sat",         USR_OVF_BIT)
FUNC_P_OP_P(vconj,              "%0 = vconj(%2):sat",               USR_OVF_BIT)
FUNC_P_OP_PP(vxaddsubw,         "%0 = vxaddsubw(%2, %3):sat",       USR_OVF_BIT)
FUNC_P_OP_P(vabshsat,           "%0 = vabsh(%2):sat",               USR_OVF_BIT)
FUNC_P_OP_PP(vnavgwr,           "%0 = vnavgw(%2, %3):rnd:sat",      USR_OVF_BIT)
FUNC_R_OP_RI(round_ri_sat,      "%0 = round(%2, #%3):sat",          USR_OVF_BIT)
FUNC_R_OP_RR(asr_r_r_sat,       "%0 = asr(%2, %3):sat",             USR_OVF_BIT)

FUNC_XPp_OP_PP(ACS,             "%0, p2 = vacsh(%3, %4)",           USR_OVF_BIT)

/* Floating point */
FUNC_R_OP_RR(sfmin,             "%0 = sfmin(%2, %3)",            USR_FPINVF_BIT)
FUNC_R_OP_RR(sfmax,             "%0 = sfmax(%2, %3)",            USR_FPINVF_BIT)
FUNC_R_OP_RR(sfadd,             "%0 = sfadd(%2, %3)",            USR_FPINVF_BIT)
FUNC_R_OP_RR(sfsub,             "%0 = sfsub(%2, %3)",            USR_FPINVF_BIT)
FUNC_R_OP_RR(sfmpy,             "%0 = sfmpy(%2, %3)",            USR_FPINVF_BIT)
FUNC_XR_OP_RR(sffma,            "%0 += sfmpy(%2, %3)",           USR_FPINVF_BIT)
FUNC_XR_OP_RR(sffms,            "%0 -= sfmpy(%2, %3)",           USR_FPINVF_BIT)
FUNC_CMP_RR(sfcmpuo,            "p1 = sfcmp.uo(%2, %3)",         USR_FPINVF_BIT)
FUNC_CMP_RR(sfcmpeq,            "p1 = sfcmp.eq(%2, %3)",         USR_FPINVF_BIT)
FUNC_CMP_RR(sfcmpgt,            "p1 = sfcmp.gt(%2, %3)",         USR_FPINVF_BIT)
FUNC_CMP_RR(sfcmpge,            "p1 = sfcmp.ge(%2, %3)",         USR_FPINVF_BIT)

FUNC_P_OP_PP(dfadd,             "%0 = dfadd(%2, %3)",            USR_FPINVF_BIT)
FUNC_P_OP_PP(dfsub,             "%0 = dfsub(%2, %3)",            USR_FPINVF_BIT)

#if CORE_IS_V67
FUNC_P_OP_PP(dfmin,             "%0 = dfmin(%2, %3)",            USR_FPINVF_BIT)
FUNC_P_OP_PP(dfmax,             "%0 = dfmax(%2, %3)",            USR_FPINVF_BIT)
FUNC_XP_OP_PP(dfmpyhh,          "%0 += dfmpyhh(%2, %3)",         USR_FPINVF_BIT)
#endif

FUNC_CMP_PP(dfcmpuo,            "p1 = dfcmp.uo(%2, %3)",         USR_FPINVF_BIT)
FUNC_CMP_PP(dfcmpeq,            "p1 = dfcmp.eq(%2, %3)",         USR_FPINVF_BIT)
FUNC_CMP_PP(dfcmpgt,            "p1 = dfcmp.gt(%2, %3)",         USR_FPINVF_BIT)
FUNC_CMP_PP(dfcmpge,            "p1 = dfcmp.ge(%2, %3)",         USR_FPINVF_BIT)

/* Conversions from sf */
FUNC_P_OP_R(conv_sf2df,         "%0 = convert_sf2df(%2)",        USR_FPINVF_BIT)
FUNC_R_OP_R(conv_sf2uw,         "%0 = convert_sf2uw(%2)",        USR_FPINVF_BIT)
FUNC_R_OP_R(conv_sf2w,          "%0 = convert_sf2w(%2)",         USR_FPINVF_BIT)
FUNC_P_OP_R(conv_sf2ud,         "%0 = convert_sf2ud(%2)",        USR_FPINVF_BIT)
FUNC_P_OP_R(conv_sf2d,          "%0 = convert_sf2d(%2)",         USR_FPINVF_BIT)
FUNC_R_OP_R(conv_sf2uw_chop,    "%0 = convert_sf2uw(%2)",        USR_FPINVF_BIT)
FUNC_R_OP_R(conv_sf2w_chop,     "%0 = convert_sf2w(%2)",         USR_FPINVF_BIT)
FUNC_P_OP_R(conv_sf2ud_chop,    "%0 = convert_sf2ud(%2)",        USR_FPINVF_BIT)
FUNC_P_OP_R(conv_sf2d_chop,     "%0 = convert_sf2d(%2)",         USR_FPINVF_BIT)

/* Conversions from df */
FUNC_R_OP_P(conv_df2sf,         "%0 = convert_df2sf(%2)",        USR_FPINVF_BIT)
FUNC_R_OP_P(conv_df2uw,         "%0 = convert_df2uw(%2)",        USR_FPINVF_BIT)
FUNC_R_OP_P(conv_df2w,          "%0 = convert_df2w(%2)",         USR_FPINVF_BIT)
FUNC_P_OP_P(conv_df2ud,         "%0 = convert_df2ud(%2)",        USR_FPINVF_BIT)
FUNC_P_OP_P(conv_df2d,          "%0 = convert_df2d(%2)",         USR_FPINVF_BIT)
FUNC_R_OP_P(conv_df2uw_chop,    "%0 = convert_df2uw(%2)",        USR_FPINVF_BIT)
FUNC_R_OP_P(conv_df2w_chop,     "%0 = convert_df2w(%2)",         USR_FPINVF_BIT)
FUNC_P_OP_P(conv_df2ud_chop,    "%0 = convert_df2ud(%2)",        USR_FPINVF_BIT)
FUNC_P_OP_P(conv_df2d_chop,     "%0 = convert_df2d(%2)",         USR_FPINVF_BIT)

/* Integer to float conversions */
FUNC_R_OP_R(conv_uw2sf,         "%0 = convert_uw2sf(%2)",        USR_FPINPF_BIT)
FUNC_R_OP_R(conv_w2sf,          "%0 = convert_w2sf(%2)",         USR_FPINPF_BIT)
FUNC_R_OP_P(conv_ud2sf,         "%0 = convert_ud2sf(%2)",        USR_FPINPF_BIT)
FUNC_R_OP_P(conv_d2sf,          "%0 = convert_d2sf(%2)",         USR_FPINPF_BIT)

/* Special purpose floating point instructions */
FUNC_XR_OP_RRp(sffma_sc,        "%0 += sfmpy(%2, %3, p2):scale", USR_FPINVF_BIT)
FUNC_Rp_OP_RR(sfrecipa,         "%0, p2 = sfrecipa(%3, %4)",     USR_FPINVF_BIT)
FUNC_R_OP_RR(sffixupn,          "%0 = sffixupn(%2, %3)",         USR_FPINVF_BIT)
FUNC_R_OP_RR(sffixupd,          "%0 = sffixupd(%2, %3)",         USR_FPINVF_BIT)
FUNC_R_OP_R(sffixupr,           "%0 = sffixupr(%2)",             USR_FPINVF_BIT)
FUNC_Rp_OP_R(sfinvsqrta,        "%0, p2 = sfinvsqrta(%3)",       USR_FPINVF_BIT)

/*
 * Templates for test cases
 *
 * Same naming convention as the function templates
 */
#define TEST_x_OP_x(RESTYPE, CHECKFN, SRCTYPE, FUNC, SRC, RES, USR_RES) \
    do { \
        RESTYPE result; \
        SRCTYPE src = SRC; \
        uint32_t usr_result; \
        result = FUNC(src, &usr_result); \
        CHECKFN(result, RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_R_OP_R(FUNC, SRC, RES, USR_RES) \
TEST_x_OP_x(uint32_t, check32, uint32_t, FUNC, SRC, RES, USR_RES)

#define TEST_R_OP_P(FUNC, SRC, RES, USR_RES) \
TEST_x_OP_x(uint32_t, check32, uint64_t, FUNC, SRC, RES, USR_RES)

#define TEST_P_OP_P(FUNC, SRC, RES, USR_RES) \
TEST_x_OP_x(uint64_t, check64, uint64_t, FUNC, SRC, RES, USR_RES)

#define TEST_P_OP_R(FUNC, SRC, RES, USR_RES) \
TEST_x_OP_x(uint64_t, check64, uint32_t, FUNC, SRC, RES, USR_RES)

#define TEST_xp_OP_x(RESTYPE, CHECKFN, SRCTYPE, FUNC, SRC, \
                     RES, PRED_RES, USR_RES) \
    do { \
        RESTYPE result; \
        SRCTYPE src = SRC; \
        uint8_t pred_result; \
        uint32_t usr_result; \
        result = FUNC(src, &pred_result, &usr_result); \
        CHECKFN(result, RES); \
        check(pred_result, PRED_RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_Rp_OP_R(FUNC, SRC, RES, PRED_RES, USR_RES) \
TEST_xp_OP_x(uint32_t, check32, uint32_t, FUNC, SRC, RES, PRED_RES, USR_RES)

#define TEST_x_OP_xx(RESTYPE, CHECKFN, SRC1TYPE, SRC2TYPE, \
                     FUNC, SRC1, SRC2, RES, USR_RES) \
    do { \
        RESTYPE result; \
        SRC1TYPE src1 = SRC1; \
        SRC2TYPE src2 = SRC2; \
        uint32_t usr_result; \
        result = FUNC(src1, src2, &usr_result); \
        CHECKFN(result, RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_P_OP_PP(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xx(uint64_t, check64, uint64_t, uint64_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_R_OP_PP(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xx(uint32_t, check32, uint64_t, uint64_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_P_OP_RR(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xx(uint64_t, check64, uint32_t, uint32_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_R_OP_RR(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xx(uint32_t, check32, uint32_t, uint32_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_R_OP_PR(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xx(uint32_t, check32, uint64_t, uint32_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_P_OP_PR(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xx(uint64_t, check64, uint64_t, uint32_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_xp_OP_xx(RESTYPE, CHECKFN, SRC1TYPE, SRC2TYPE, FUNC, SRC1, SRC2, \
                      RES, PRED_RES, USR_RES) \
    do { \
        RESTYPE result; \
        SRC1TYPE src1 = SRC1; \
        SRC2TYPE src2 = SRC2; \
        uint8_t pred_result; \
        uint32_t usr_result; \
        result = FUNC(src1, src2, &pred_result, &usr_result); \
        CHECKFN(result, RES); \
        check(pred_result, PRED_RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_Rp_OP_RR(FUNC, SRC1, SRC2, RES, PRED_RES, USR_RES) \
TEST_xp_OP_xx(uint32_t, check32, uint32_t, uint32_t, FUNC, SRC1, SRC2, \
              RES, PRED_RES, USR_RES)

#define TEST_x_OP_xI(RESTYPE, CHECKFN, SRC1TYPE, \
                     FUNC, SRC1, SRC2, RES, USR_RES) \
    do { \
        RESTYPE result; \
        SRC1TYPE src1 = SRC1; \
        uint32_t src2 = SRC2; \
        uint32_t usr_result; \
        result = FUNC(src1, src2, &usr_result); \
        CHECKFN(result, RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_R_OP_RI(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xI(uint32_t, check32, uint32_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_R_OP_PI(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_x_OP_xI(uint32_t, check64, uint64_t, \
             FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_Xx_OP_xx(RESTYPE, CHECKFN, SRC1TYPE, SRC2TYPE, \
                      FUNC, RESIN, SRC1, SRC2, RES, USR_RES) \
    do { \
        RESTYPE result = RESIN; \
        SRC1TYPE src1 = SRC1; \
        SRC2TYPE src2 = SRC2; \
        uint32_t usr_result; \
        result = FUNC(result, src1, src2, &usr_result); \
        CHECKFN(result, RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_XR_OP_RR(FUNC, RESIN, SRC1, SRC2, RES, USR_RES) \
TEST_Xx_OP_xx(uint32_t, check32, uint32_t, uint32_t, \
              FUNC, RESIN, SRC1, SRC2, RES, USR_RES)

#define TEST_XP_OP_PP(FUNC, RESIN, SRC1, SRC2, RES, USR_RES) \
TEST_Xx_OP_xx(uint64_t, check64, uint64_t, uint64_t, \
              FUNC, RESIN, SRC1, SRC2, RES, USR_RES)

#define TEST_XP_OP_RR(FUNC, RESIN, SRC1, SRC2, RES, USR_RES) \
TEST_Xx_OP_xx(uint64_t, check64, uint32_t, uint32_t, \
              FUNC, RESIN, SRC1, SRC2, RES, USR_RES)

#define TEST_Xxp_OP_xx(RESTYPE, CHECKFN, SRC1TYPE, SRC2TYPE, \
                       FUNC, RESIN, SRC1, SRC2, RES, PRED_RES, USR_RES) \
    do { \
        RESTYPE result = RESIN; \
        SRC1TYPE src1 = SRC1; \
        SRC2TYPE src2 = SRC2; \
        uint8_t pred_res; \
        uint32_t usr_result; \
        result = FUNC(result, src1, src2, &pred_res, &usr_result); \
        CHECKFN(result, RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_XPp_OP_PP(FUNC, RESIN, SRC1, SRC2, RES, PRED_RES, USR_RES) \
TEST_Xxp_OP_xx(uint64_t, check64, uint64_t, uint64_t, FUNC, RESIN, SRC1, SRC2, \
               RES, PRED_RES, USR_RES)

#define TEST_Xx_OP_xxp(RESTYPE, CHECKFN, SRC1TYPE, SRC2TYPE, \
                      FUNC, RESIN, SRC1, SRC2, PRED, RES, USR_RES) \
    do { \
        RESTYPE result = RESIN; \
        SRC1TYPE src1 = SRC1; \
        SRC2TYPE src2 = SRC2; \
        uint8_t pred = PRED; \
        uint32_t usr_result; \
        result = FUNC(result, src1, src2, pred, &usr_result); \
        CHECKFN(result, RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_XR_OP_RRp(FUNC, RESIN, SRC1, SRC2, PRED, RES, USR_RES) \
TEST_Xx_OP_xxp(uint32_t, check32, uint32_t, uint32_t, \
              FUNC, RESIN, SRC1, SRC2, PRED, RES, USR_RES)

#define TEST_CMP_xx(SRC1TYPE, SRC2TYPE, \
                    FUNC, SRC1, SRC2, RES, USR_RES) \
    do { \
        uint32_t result; \
        SRC1TYPE src1 = SRC1; \
        SRC2TYPE src2 = SRC2; \
        uint32_t usr_result; \
        result = FUNC(src1, src2, &usr_result); \
        check(result, RES); \
        check(usr_result, USR_RES); \
    } while (0)

#define TEST_CMP_RR(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_CMP_xx(uint32_t, uint32_t, FUNC, SRC1, SRC2, RES, USR_RES)

#define TEST_CMP_PP(FUNC, SRC1, SRC2, RES, USR_RES) \
TEST_CMP_xx(uint64_t, uint64_t, FUNC, SRC1, SRC2, RES, USR_RES)

int main()
{
    TEST_R_OP_R(satub,       0,         0,         0);
    TEST_R_OP_R(satub,       0xff,      0xff,      0);
    TEST_R_OP_R(satub,       0xfff,     0xff,      1);
    TEST_R_OP_R(satub,       -1,        0,         1);

    TEST_P_OP_PP(vaddubs,    0xfeLL,    0x01LL,    0xffLL,    0);
    TEST_P_OP_PP(vaddubs,    0xffLL,    0xffLL,    0xffLL,    1);

    TEST_P_OP_PP(vadduhs,    0xfffeLL,  0x1LL,     0xffffLL,  0);
    TEST_P_OP_PP(vadduhs,    0xffffLL,  0x1LL,     0xffffLL,  1);

    TEST_P_OP_PP(vsububs, 0x0807060504030201LL, 0x0101010101010101LL,
                 0x0706050403020100LL, 0);
    TEST_P_OP_PP(vsububs, 0x0807060504030201LL, 0x0202020202020202LL,
                 0x0605040302010000LL, 1);

    TEST_P_OP_PP(vsubuhs, 0x0004000300020001LL, 0x0001000100010001LL,
                 0x0003000200010000LL, 0);
    TEST_P_OP_PP(vsubuhs, 0x0004000300020001LL, 0x0002000200020002LL,
                 0x0002000100000000LL, 1);

    TEST_R_OP_PP(vaddhubs, 0x0004000300020001LL, 0x0001000100010001LL,
                 0x05040302, 0);
    TEST_R_OP_PP(vaddhubs, 0x7fff000300020001LL, 0x0002000200020002LL,
                 0xff050403, 1);

    TEST_R_OP_P(vsathub,         0x0001000300020001LL, 0x01030201,           0);
    TEST_R_OP_P(vsathub,         0x010000700080ffffLL, 0xff708000,           1);

    TEST_R_OP_P(vsatwuh,         0x0000ffff00000001LL, 0xffff0001,           0);
    TEST_R_OP_P(vsatwuh,         0x800000000000ffffLL, 0x0000ffff,           1);

    TEST_P_OP_P(vsatwuh_nopack,  0x0000ffff00000001LL, 0x0000ffff00000001LL, 0);
    TEST_P_OP_P(vsatwuh_nopack,  0x800000000000ffffLL, 0x000000000000ffffLL, 1);

    TEST_R_OP_R(svsathub,        0x00020001,           0x0201,               0);
    TEST_R_OP_R(svsathub,        0x0080ffff,           0x8000,               1);

    TEST_R_OP_PI(asrhub_sat,     0x004f003f002f001fLL, 3,    0x09070503,     0);
    TEST_R_OP_PI(asrhub_sat,     0x004fffff8fff001fLL, 3,    0x09000003,     1);

    TEST_R_OP_PI(asrhub_rnd_sat, 0x004f003f002f001fLL, 2,    0x0a080604,     0);
    TEST_R_OP_PI(asrhub_rnd_sat, 0x004fffff8fff001fLL, 2,    0x0a000004,     1);

    TEST_R_OP_RR(addsat,        1,              2,              3,           0);
    TEST_R_OP_RR(addsat,        0x7fffffff,     0x00000010,     0x7fffffff,  1);
    TEST_R_OP_RR(addsat,        0x80000000,     0x80000006,     0x80000000,  1);

    TEST_P_OP_PP(addpsat, 1LL, 2LL, 3LL, 0);
    /* overflow to max positive */
    TEST_P_OP_PP(addpsat, 0x7ffffffffffffff0LL, 0x0000000000000010LL,
                 0x7fffffffffffffffLL, 1);
    /* overflow to min negative */
    TEST_P_OP_PP(addpsat, 0x8000000000000003LL, 0x8000000000000006LL,
                 0x8000000000000000LL, 1);

    TEST_XR_OP_RR(mpy_acc_sat_hh_s0, 0x7fffffff, 0xffff0000, 0x11110000,
                  0x7fffeeee, 0);
    TEST_XR_OP_RR(mpy_acc_sat_hh_s0, 0x7fffffff, 0x7fff0000, 0x7fff0000,
                  0x7fffffff, 1);

    TEST_R_OP_RR(mpy_sat_hh_s1,        0xffff0000, 0x11110000, 0xffffddde, 0);
    TEST_R_OP_RR(mpy_sat_hh_s1,        0x7fff0000, 0x7fff0000, 0x7ffe0002, 0);
    TEST_R_OP_RR(mpy_sat_hh_s1,        0x80000000, 0x80000000, 0x7fffffff, 1);

    TEST_R_OP_RR(mpy_sat_rnd_hh_s1,    0xffff0000, 0x11110000, 0x00005dde, 0);
    TEST_R_OP_RR(mpy_sat_rnd_hh_s1,    0x7fff0000, 0x7fff0000, 0x7ffe8002, 0);
    TEST_R_OP_RR(mpy_sat_rnd_hh_s1,    0x80000000, 0x80000000, 0x7fffffff, 1);

    TEST_R_OP_RR(mpy_up_s1_sat,        0xffff0000, 0x11110000, 0xffffddde, 0);
    TEST_R_OP_RR(mpy_up_s1_sat,        0x7fff0000, 0x7fff0000, 0x7ffe0002, 0);
    TEST_R_OP_RR(mpy_up_s1_sat,        0x80000000, 0x80000000, 0x7fffffff, 1);

    TEST_P_OP_RR(vmpy2s_s1,  0x7fff0000, 0x7fff0000, 0x7ffe000200000000LL, 0);
    TEST_P_OP_RR(vmpy2s_s1,  0x80000000, 0x80000000, 0x7fffffff00000000LL, 1);

    TEST_P_OP_RR(vmpy2su_s1, 0x7fff0000, 0x7fff0000, 0x7ffe000200000000LL, 0);
    TEST_P_OP_RR(vmpy2su_s1, 0xffffbd97, 0xffffffff, 0xfffe000280000000LL, 1);

    TEST_R_OP_RR(vmpy2s_s1pack,        0x7fff0000, 0x7fff0000, 0x7ffe0000, 0);
    TEST_R_OP_RR(vmpy2s_s1pack,        0x80008000, 0x80008000, 0x7fff7fff, 1);

    TEST_P_OP_PP(vmpy2es_s1, 0x7fff7fff7fff7fffLL, 0x1fff1fff1fff1fffLL,
                 0x1ffec0021ffec002LL, 0);
    TEST_P_OP_PP(vmpy2es_s1, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fffffff7fffffffLL, 1);

    TEST_R_OP_PP(vdmpyrs_s1, 0x7fff7fff7fff7fffLL, 0x1fff1fff1fff1fffLL,
                 0x3ffe3ffe, 0);
    TEST_R_OP_PP(vdmpyrs_s1, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fff7fffLL, 1);

    TEST_XP_OP_PP(vdmacs_s0, 0x0fffffffULL, 0x00ff00ff00ff00ffLL,
                  0x00ff00ff00ff00ffLL, 0x0001fc021001fc01LL, 0);
    TEST_XP_OP_PP(vdmacs_s0, 0x01111111ULL, 0x8000800080001000LL,
                  0x8000800080008000LL, 0x7fffffff39111111LL, 1);

    TEST_R_OP_RR(cmpyrs_s0,            0x7fff0000, 0x7fff0000, 0x0000c001, 0);
    TEST_R_OP_RR(cmpyrs_s0,            0x80008000, 0x80008000, 0x7fff0000, 1);

    TEST_XP_OP_RR(cmacs_s0, 0x0fffffff, 0x7fff0000, 0x7fff0000,
                  0x00000000d000fffeLL, 0);
    TEST_XP_OP_RR(cmacs_s0, 0x0fff1111, 0x80008000, 0x80008000,
                  0x7fffffff0fff1111LL, 1);

    TEST_XP_OP_RR(cnacs_s0, 0x000000108fffffffULL, 0x7fff0000, 0x7fff0000,
                  0x00000010cfff0000ULL, 0);
    TEST_XP_OP_RR(cnacs_s0, 0x000000108ff1111fULL, 0x00002001, 0x00007ffd,
                  0x0000001080000000ULL, 1);

    TEST_P_OP_PP(vrcmpys_s1_h, 0x00ff00ff00ff00ffLL, 0x00ff00ff00ff00ffLL,
                 0x0003f8040003f804LL, 0);
    TEST_P_OP_PP(vrcmpys_s1_h, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fffffff7fffffffLL, 1);

    TEST_XP_OP_PP(mmacls_s0, 0x6fffffff, 0x00ff00ff00ff00ffLL,
                  0x00ff00ff00ff00ffLL, 0x0000fe017000fe00LL, 0);
    TEST_XP_OP_PP(mmacls_s0, 0x6f1111ff, 0x8000800080008000LL,
                  0x1000100080008000LL, 0xf80008007fffffffLL, 1);

    TEST_R_OP_RR(hmmpyl_rs1,           0x7fff0000, 0x7fff0001, 0x0000fffe, 0);
    TEST_R_OP_RR(hmmpyl_rs1,           0x80000000, 0x80008000, 0x7fffffff, 1);

    TEST_XP_OP_PP(mmaculs_s0, 0x000000007fffffffULL, 0xffff800080008000LL,
                  0xffff800080008000LL, 0xffffc00040003fffLL, 0);
    TEST_XP_OP_PP(mmaculs_s0, 0x000011107fffffffULL, 0x00ff00ff00ff00ffLL,
                  0x00ff00ff001100ffLL, 0x00010f117fffffffLL, 1);

    TEST_R_OP_PR(cmpyi_wh, 0x7fff000000000000LL, 0x7fff0001, 0x0000fffe, 0);
    TEST_R_OP_PR(cmpyi_wh, 0x8000000000000000LL, 0x80008000, 0x7fffffff, 1);

    TEST_P_OP_PP(vcmpy_s0_sat_i, 0x00ff00ff00ff00ffLL, 0x00ff00ff00ff00ffLL,
                 0x0001fc020001fc02LL, 0);
    TEST_P_OP_PP(vcmpy_s0_sat_i, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fffffff7fffffffLL, 1);

    TEST_P_OP_PR(vcrotate, 0x8000000000000000LL, 0x00000002,
                 0x8000000000000000LL, 0);
    TEST_P_OP_PR(vcrotate, 0x7fff80007fff8000LL, 0x00000001,
                 0x7fff80007fff7fffLL, 1);

    TEST_P_OP_PR(vcnegh, 0x8000000000000000LL, 0x00000002,
                 0x8000000000000000LL, 0);
    TEST_P_OP_PR(vcnegh, 0x7fff80007fff8000LL, 0x00000001,
                 0x7fff80007fff7fffLL, 1);

#if CORE_HAS_AUDIO
    TEST_R_OP_PP(wcmpyrw, 0x8765432101234567LL, 0x00000002ffffffffLL,
                 0x00000001, 0);
    TEST_R_OP_PP(wcmpyrw, 0x800000007fffffffLL, 0x000000ff7fffffffLL,
                 0x7fffffff, 1);
    TEST_R_OP_PP(wcmpyrw, 0x7fffffff80000000LL, 0x7fffffff000000ffLL,
                 0x80000000, 1);
#else
    printf("Audio instructions skipped\n");
#endif

    TEST_R_OP_RR(addh_l16_sat_ll,      0x0000ffff, 0x00000002, 0x00000001, 0);
    TEST_R_OP_RR(addh_l16_sat_ll,      0x00007fff, 0x00000005, 0x00007fff, 1);
    TEST_R_OP_RR(addh_l16_sat_ll,      0x00008000, 0x00008000, 0xffff8000, 1);

    TEST_P_OP_P(vconj, 0x0000ffff00000001LL, 0x0000ffff00000001LL, 0);
    TEST_P_OP_P(vconj, 0x800000000000ffffLL, 0x7fff00000000ffffLL, 1);

    TEST_P_OP_PP(vxaddsubw, 0x8765432101234567LL, 0x00000002ffffffffLL,
                 0x8765432201234569LL, 0);
    TEST_P_OP_PP(vxaddsubw, 0x7fffffff7fffffffLL, 0xffffffffffffffffLL,
                 0x7fffffff7ffffffeLL, 1);
    TEST_P_OP_PP(vxaddsubw, 0x800000000fffffffLL, 0x0000000a00000008LL,
                 0x8000000010000009LL, 1);

    TEST_P_OP_P(vabshsat, 0x0001000afffff800LL, 0x0001000a00010800LL, 0);
    TEST_P_OP_P(vabshsat, 0x8000000b000c000aLL, 0x7fff000b000c000aLL, 1);

    TEST_P_OP_PP(vnavgwr, 0x8765432101234567LL, 0x00000002ffffffffLL,
                 0xc3b2a1900091a2b4LL, 0);
    TEST_P_OP_PP(vnavgwr, 0x7fffffff8000000aLL, 0x80000000ffffffffLL,
                 0x7fffffffc0000006LL, 1);

    TEST_R_OP_RI(round_ri_sat,         0x0000ffff, 2, 0x00004000, 0);
    TEST_R_OP_RI(round_ri_sat,         0x7fffffff, 2, 0x1fffffff, 1);

    TEST_R_OP_RR(asr_r_r_sat,          0x0000ffff, 0x00000002, 0x00003fff, 0);
    TEST_R_OP_RR(asr_r_r_sat,          0x00ffffff, 0xfffffff5, 0x7fffffff, 1);
    TEST_R_OP_RR(asr_r_r_sat,          0x80000000, 0xfffffff5, 0x80000000, 1);

    TEST_XPp_OP_PP(ACS, 0x0004000300020001ULL, 0x0001000200030004ULL,
                   0x0000000000000000ULL, 0x0004000300030004ULL, 0xf0, 0);
    TEST_XPp_OP_PP(ACS, 0x0004000300020001ULL, 0x0001000200030004ULL,
                   0x000affff000d0000ULL, 0x000e0003000f0004ULL, 0xcc, 0);
    TEST_XPp_OP_PP(ACS, 0x00047fff00020001ULL, 0x00017fff00030004ULL,
                  0x000a0fff000d0000ULL, 0x000e7fff000f0004ULL, 0xfc, 1);
    TEST_XPp_OP_PP(ACS, 0x00047fff00020001ULL, 0x00017fff00030004ULL,
                   0x000a0fff000d0000ULL, 0x000e7fff000f0004ULL, 0xf0, 1);

    /* Floating point */
    TEST_R_OP_RR(sfmin,  SF_one,      SF_small_neg,    SF_small_neg,    0);
    TEST_R_OP_RR(sfmin,  SF_one,      SF_SNaN,         SF_one,          1);
    TEST_R_OP_RR(sfmin,  SF_SNaN,     SF_one,          SF_one,          1);
    TEST_R_OP_RR(sfmin,  SF_one,      SF_QNaN,         SF_one,          0);
    TEST_R_OP_RR(sfmin,  SF_QNaN,     SF_one,          SF_one,          0);
    TEST_R_OP_RR(sfmin,  SF_SNaN,     SF_QNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfmin,  SF_QNaN,     SF_SNaN,         SF_HEX_NaN,      1);

    TEST_R_OP_RR(sfmax,  SF_one,      SF_small_neg,    SF_one,          0);
    TEST_R_OP_RR(sfmax,  SF_one,      SF_SNaN,         SF_one,          1);
    TEST_R_OP_RR(sfmax,  SF_SNaN,     SF_one,          SF_one,          1);
    TEST_R_OP_RR(sfmax,  SF_one,      SF_QNaN,         SF_one,          0);
    TEST_R_OP_RR(sfmax,  SF_QNaN,     SF_one,          SF_one,          0);
    TEST_R_OP_RR(sfmax,  SF_SNaN,     SF_QNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfmax,  SF_QNaN,     SF_SNaN,         SF_HEX_NaN,      1);

    TEST_R_OP_RR(sfadd,  SF_one,      SF_QNaN,         SF_HEX_NaN,      0);
    TEST_R_OP_RR(sfadd,  SF_one,      SF_SNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfadd,  SF_QNaN,     SF_SNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfadd,  SF_SNaN,     SF_QNaN,         SF_HEX_NaN,      1);

    TEST_R_OP_RR(sfsub,  SF_one,      SF_QNaN,         SF_HEX_NaN,      0);
    TEST_R_OP_RR(sfsub,  SF_one,      SF_SNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfsub,  SF_QNaN,     SF_SNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfsub,  SF_SNaN,     SF_QNaN,         SF_HEX_NaN,      1);

    TEST_R_OP_RR(sfmpy,  SF_one,      SF_QNaN,         SF_HEX_NaN,      0);
    TEST_R_OP_RR(sfmpy,  SF_one,      SF_SNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfmpy,  SF_QNaN,     SF_SNaN,         SF_HEX_NaN,      1);
    TEST_R_OP_RR(sfmpy,  SF_SNaN,     SF_QNaN,         SF_HEX_NaN,      1);

    TEST_XR_OP_RR(sffma, SF_one,      SF_one,       SF_one,      SF_two,     0);
    TEST_XR_OP_RR(sffma, SF_zero,     SF_one,       SF_QNaN,     SF_HEX_NaN, 0);
    TEST_XR_OP_RR(sffma, SF_zero,     SF_one,       SF_SNaN,     SF_HEX_NaN, 1);
    TEST_XR_OP_RR(sffma, SF_zero,     SF_QNaN,      SF_SNaN,     SF_HEX_NaN, 1);
    TEST_XR_OP_RR(sffma, SF_zero,     SF_SNaN,      SF_QNaN,     SF_HEX_NaN, 1);

    TEST_XR_OP_RR(sffms, SF_one,      SF_one,       SF_one,      SF_zero,    0);
    TEST_XR_OP_RR(sffms, SF_zero,     SF_one,       SF_QNaN,     SF_HEX_NaN, 0);
    TEST_XR_OP_RR(sffms, SF_zero,     SF_one,       SF_SNaN,     SF_HEX_NaN, 1);
    TEST_XR_OP_RR(sffms, SF_zero,     SF_QNaN,      SF_SNaN,     SF_HEX_NaN, 1);
    TEST_XR_OP_RR(sffms, SF_zero,     SF_SNaN,      SF_QNaN,     SF_HEX_NaN, 1);

    TEST_CMP_RR(sfcmpuo, SF_one,      SF_large_pos,    0x00,    0);
    TEST_CMP_RR(sfcmpuo, SF_INF,      SF_large_pos,    0x00,    0);
    TEST_CMP_RR(sfcmpuo, SF_QNaN,     SF_large_pos,    0xff,    0);
    TEST_CMP_RR(sfcmpuo, SF_QNaN_neg, SF_large_pos,    0xff,    0);
    TEST_CMP_RR(sfcmpuo, SF_SNaN,     SF_large_pos,    0xff,    1);
    TEST_CMP_RR(sfcmpuo, SF_SNaN_neg, SF_large_pos,    0xff,    1);
    TEST_CMP_RR(sfcmpuo, SF_QNaN,     SF_QNaN,         0xff,    0);
    TEST_CMP_RR(sfcmpuo, SF_QNaN,     SF_SNaN,         0xff,    1);

    TEST_CMP_RR(sfcmpeq, SF_one,      SF_QNaN,         0x00,    0);
    TEST_CMP_RR(sfcmpeq, SF_one,      SF_SNaN,         0x00,    1);
    TEST_CMP_RR(sfcmpgt, SF_one,      SF_QNaN,         0x00,    0);
    TEST_CMP_RR(sfcmpgt, SF_one,      SF_SNaN,         0x00,    1);
    TEST_CMP_RR(sfcmpge, SF_one,      SF_QNaN,         0x00,    0);
    TEST_CMP_RR(sfcmpge, SF_one,      SF_SNaN,         0x00,    1);

    TEST_P_OP_PP(dfadd,  DF_any,      DF_QNaN,         DF_HEX_NaN,      0);
    TEST_P_OP_PP(dfadd,  DF_any,      DF_SNaN,         DF_HEX_NaN,      1);
    TEST_P_OP_PP(dfadd,  DF_QNaN,     DF_SNaN,         DF_HEX_NaN,      1);
    TEST_P_OP_PP(dfadd,  DF_SNaN,     DF_QNaN,         DF_HEX_NaN,      1);

    TEST_P_OP_PP(dfsub,  DF_any,      DF_QNaN,         DF_HEX_NaN,      0);
    TEST_P_OP_PP(dfsub,  DF_any,      DF_SNaN,         DF_HEX_NaN,      1);
    TEST_P_OP_PP(dfsub,  DF_QNaN,     DF_SNaN,         DF_HEX_NaN,      1);
    TEST_P_OP_PP(dfsub,  DF_SNaN,     DF_QNaN,         DF_HEX_NaN,      1);

#if CORE_IS_V67
    TEST_P_OP_PP(dfmin,  DF_any,      DF_small_neg,    DF_small_neg,    0);
    TEST_P_OP_PP(dfmin,  DF_any,      DF_SNaN,         DF_any,          1);
    TEST_P_OP_PP(dfmin,  DF_SNaN,     DF_any,          DF_any,          1);
    TEST_P_OP_PP(dfmin,  DF_any,      DF_QNaN,         DF_any,          1);
    TEST_P_OP_PP(dfmin,  DF_QNaN,     DF_any,          DF_any,          1);
    TEST_P_OP_PP(dfmin,  DF_SNaN,     DF_QNaN,         DF_HEX_NaN,      1);
    TEST_P_OP_PP(dfmin,  DF_QNaN,     DF_SNaN,         DF_HEX_NaN,      1);

    TEST_P_OP_PP(dfmax,  DF_any,      DF_small_neg,    DF_any,          0);
    TEST_P_OP_PP(dfmax,  DF_any,      DF_SNaN,         DF_any,          1);
    TEST_P_OP_PP(dfmax,  DF_SNaN,     DF_any,          DF_any,          1);
    TEST_P_OP_PP(dfmax,  DF_any,      DF_QNaN,         DF_any,          1);
    TEST_P_OP_PP(dfmax,  DF_QNaN,     DF_any,          DF_any,          1);
    TEST_P_OP_PP(dfmax,  DF_SNaN,     DF_QNaN,         DF_HEX_NaN,      1);
    TEST_P_OP_PP(dfmax,  DF_QNaN,     DF_SNaN,         DF_HEX_NaN,      1);

    TEST_XP_OP_PP(dfmpyhh, DF_one,    DF_one,     DF_one,     DF_one_hh,     0);
    TEST_XP_OP_PP(dfmpyhh, DF_zero,   DF_any,     DF_QNaN,    DF_HEX_NaN,    0);
    TEST_XP_OP_PP(dfmpyhh, DF_zero,   DF_any,     DF_SNaN,    DF_HEX_NaN,    1);
    TEST_XP_OP_PP(dfmpyhh, DF_zero,   DF_QNaN,    DF_SNaN,    DF_HEX_NaN,    1);
    TEST_XP_OP_PP(dfmpyhh, DF_zero,   DF_SNaN,    DF_QNaN,    DF_HEX_NaN,    1);
#else
    printf("v67 instructions skipped\n");
#endif

    TEST_CMP_PP(dfcmpuo, DF_small_neg, DF_any,          0x00,    0);
    TEST_CMP_PP(dfcmpuo, DF_large_pos, DF_any,          0x00,    0);
    TEST_CMP_PP(dfcmpuo, DF_QNaN,      DF_any,          0xff,    0);
    TEST_CMP_PP(dfcmpuo, DF_QNaN_neg,  DF_any,          0xff,    0);
    TEST_CMP_PP(dfcmpuo, DF_SNaN,      DF_any,          0xff,    1);
    TEST_CMP_PP(dfcmpuo, DF_SNaN_neg,  DF_any,          0xff,    1);
    TEST_CMP_PP(dfcmpuo, DF_QNaN,      DF_QNaN,         0xff,    0);
    TEST_CMP_PP(dfcmpuo, DF_QNaN,      DF_SNaN,         0xff,    1);

    TEST_CMP_PP(dfcmpeq, DF_any,       DF_QNaN,         0x00,    0);
    TEST_CMP_PP(dfcmpeq, DF_any,       DF_SNaN,         0x00,    1);
    TEST_CMP_PP(dfcmpgt, DF_any,       DF_QNaN,         0x00,    0);
    TEST_CMP_PP(dfcmpgt, DF_any,       DF_SNaN,         0x00,    1);
    TEST_CMP_PP(dfcmpge, DF_any,       DF_QNaN,         0x00,    0);
    TEST_CMP_PP(dfcmpge, DF_any,       DF_SNaN,         0x00,    1);

    TEST_P_OP_R(conv_sf2df,            SF_QNaN,    DF_HEX_NaN,              0);
    TEST_P_OP_R(conv_sf2df,            SF_SNaN,    DF_HEX_NaN,              1);
    TEST_R_OP_R(conv_sf2uw,            SF_QNaN,    0xffffffff,              1);
    TEST_R_OP_R(conv_sf2uw,            SF_SNaN,    0xffffffff,              1);
    TEST_R_OP_R(conv_sf2w,             SF_QNaN,    0xffffffff,              1);
    TEST_R_OP_R(conv_sf2w,             SF_SNaN,    0xffffffff,              1);
    TEST_P_OP_R(conv_sf2ud,            SF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_R(conv_sf2ud,            SF_SNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_R(conv_sf2d,             SF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_R(conv_sf2d,             SF_SNaN,    0xffffffffffffffffULL,   1);
    TEST_R_OP_R(conv_sf2uw_chop,       SF_QNaN,    0xffffffff,              1);
    TEST_R_OP_R(conv_sf2uw_chop,       SF_SNaN,    0xffffffff,              1);
    TEST_R_OP_R(conv_sf2w_chop,        SF_QNaN,    0xffffffff,              1);
    TEST_R_OP_R(conv_sf2w_chop,        SF_SNaN,    0xffffffff,              1);
    TEST_P_OP_R(conv_sf2ud_chop,       SF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_R(conv_sf2ud_chop,       SF_SNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_R(conv_sf2d_chop,        SF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_R(conv_sf2d_chop,        SF_SNaN,    0xffffffffffffffffULL,   1);

    TEST_R_OP_P(conv_df2sf,            DF_QNaN,    SF_HEX_NaN,              0);
    TEST_R_OP_P(conv_df2sf,            DF_SNaN,    SF_HEX_NaN,              1);
    TEST_R_OP_P(conv_df2uw,            DF_QNaN,    0xffffffff,              1);
    TEST_R_OP_P(conv_df2uw,            DF_SNaN,    0xffffffff,              1);
    TEST_R_OP_P(conv_df2w,             DF_QNaN,    0xffffffff,              1);
    TEST_R_OP_P(conv_df2w,             DF_SNaN,    0xffffffff,              1);
    TEST_P_OP_P(conv_df2ud,            DF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_P(conv_df2ud,            DF_SNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_P(conv_df2d,             DF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_P(conv_df2d,             DF_SNaN,    0xffffffffffffffffULL,   1);
    TEST_R_OP_P(conv_df2uw_chop,       DF_QNaN,    0xffffffff,              1);
    TEST_R_OP_P(conv_df2uw_chop,       DF_SNaN,    0xffffffff,              1);
    TEST_R_OP_P(conv_df2w_chop,        DF_QNaN,    0xffffffff,              1);
    TEST_R_OP_P(conv_df2w_chop,        DF_SNaN,    0xffffffff,              1);
    TEST_P_OP_P(conv_df2ud_chop,       DF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_P(conv_df2ud_chop,       DF_SNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_P(conv_df2d_chop,        DF_QNaN,    0xffffffffffffffffULL,   1);
    TEST_P_OP_P(conv_df2d_chop,        DF_SNaN,    0xffffffffffffffffULL,   1);

    TEST_R_OP_R(conv_uw2sf,            0x00000001,             SF_one,      0);
    TEST_R_OP_R(conv_uw2sf,            0x010020a5,             0x4b801052,  1);
    TEST_R_OP_R(conv_w2sf,             0x00000001,             SF_one,      0);
    TEST_R_OP_R(conv_w2sf,             0x010020a5,             0x4b801052,  1);
    TEST_R_OP_P(conv_ud2sf,            0x0000000000000001ULL,  SF_one,      0);
    TEST_R_OP_P(conv_ud2sf,            0x00000000010020a5ULL,  0x4b801052,  1);
    TEST_R_OP_P(conv_d2sf,             0x0000000000000001ULL,  SF_one,      0);
    TEST_R_OP_P(conv_d2sf,             0x00000000010020a5ULL,  0x4b801052,  1);

    TEST_XR_OP_RRp(sffma_sc, SF_one,   SF_one,    SF_one,   1, SF_four,     0);
    TEST_XR_OP_RRp(sffma_sc, SF_QNaN,  SF_one,    SF_one,   1, SF_HEX_NaN,  0);
    TEST_XR_OP_RRp(sffma_sc, SF_one,   SF_QNaN,   SF_one,   1, SF_HEX_NaN,  0);
    TEST_XR_OP_RRp(sffma_sc, SF_one,   SF_one,    SF_QNaN,  1, SF_HEX_NaN,  0);
    TEST_XR_OP_RRp(sffma_sc, SF_SNaN,  SF_one,    SF_one,   1, SF_HEX_NaN,  1);
    TEST_XR_OP_RRp(sffma_sc, SF_one,   SF_SNaN,   SF_one,   1, SF_HEX_NaN,  1);
    TEST_XR_OP_RRp(sffma_sc, SF_one,   SF_one,    SF_SNaN,  1, SF_HEX_NaN,  1);

    TEST_Rp_OP_RR(sfrecipa, SF_one,    SF_one,    SF_one_recip,   0x00,     0);
    TEST_Rp_OP_RR(sfrecipa, SF_QNaN,   SF_one,    SF_HEX_NaN,     0x00,     0);
    TEST_Rp_OP_RR(sfrecipa, SF_one,    SF_QNaN,   SF_HEX_NaN,     0x00,     0);
    TEST_Rp_OP_RR(sfrecipa, SF_one,    SF_SNaN,   SF_HEX_NaN,     0x00,     1);
    TEST_Rp_OP_RR(sfrecipa, SF_SNaN,   SF_one,    SF_HEX_NaN,     0x00,     1);

    TEST_R_OP_RR(sffixupn, SF_one,     SF_one,    SF_one,       0);
    TEST_R_OP_RR(sffixupn, SF_QNaN,    SF_one,    SF_HEX_NaN,   0);
    TEST_R_OP_RR(sffixupn, SF_one,     SF_QNaN,   SF_HEX_NaN,   0);
    TEST_R_OP_RR(sffixupn, SF_SNaN,    SF_one,    SF_HEX_NaN,   1);
    TEST_R_OP_RR(sffixupn, SF_one,     SF_SNaN,   SF_HEX_NaN,   1);

    TEST_R_OP_RR(sffixupd, SF_one,     SF_one,    SF_one,       0);
    TEST_R_OP_RR(sffixupd, SF_QNaN,    SF_one,    SF_HEX_NaN,   0);
    TEST_R_OP_RR(sffixupd, SF_one,     SF_QNaN,   SF_HEX_NaN,   0);
    TEST_R_OP_RR(sffixupd, SF_SNaN,    SF_one,    SF_HEX_NaN,   1);
    TEST_R_OP_RR(sffixupd, SF_one,     SF_SNaN,   SF_HEX_NaN,   1);

    TEST_R_OP_R(sffixupr, SF_one,             SF_one,           0);
    TEST_R_OP_R(sffixupr, SF_QNaN,            SF_HEX_NaN,       0);
    TEST_R_OP_R(sffixupr, SF_SNaN,            SF_HEX_NaN,       1);

    TEST_Rp_OP_R(sfinvsqrta, SF_one,          SF_one_invsqrta,  0x00, 0);
    TEST_Rp_OP_R(sfinvsqrta, SF_zero,         SF_one,           0x00, 0);
    TEST_Rp_OP_R(sfinvsqrta, SF_QNaN,         SF_HEX_NaN,       0x00, 0);
    TEST_Rp_OP_R(sfinvsqrta, SF_small_neg,    SF_HEX_NaN,       0x00, 1);
    TEST_Rp_OP_R(sfinvsqrta, SF_SNaN,         SF_HEX_NaN,       0x00, 1);

    puts(err ? "FAIL" : "PASS");
    return err;
}
