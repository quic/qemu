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

/* Define the bits in Hexagon USR register */
#define USR_OVF_BIT          0        /* Sticky saturation overflow */
#define USR_FPINVF_BIT       1        /* IEEE FP invalid sticky flag */
#define USR_FPDBZF_BIT       2        /* IEEE FP divide-by-zero sticky flag */
#define USR_FPOVFF_BIT       3        /* IEEE FP overflow sticky flag */
#define USR_FPUNFF_BIT       4        /* IEEE FP underflow sticky flag */
#define USR_FPINPF_BIT       5        /* IEEE FP inexact sticky flag */

/*
 * Templates for functions to execute an instruction
 *
 * The templates vary by the number of arguments and the types of the args
 * and result.  We use one letter in the macro name for the result and each
 * argument:
 *     x             unknown (specified in a subsequent template) or don't care
 *     R             register (32 bits)
 *     P             pair (64 bits)
 */

/* Template for instructions with one register operand */
#define FUNC_x_OP_x(RESTYPE, SRCTYPE, NAME, INSN, BIT) \
static RESTYPE NAME(SRCTYPE src, uint32_t *usr_result) \
{ \
    RESTYPE result; \
    uint32_t usr; \
    asm("r2 = usr\n\t" \
        "r2 = clrbit(r2, #%3)\n\t" \
        "usr = r2\n\t" \
        INSN  "\n\t" \
        "%1 = usr\n\t" \
        : "=&r"(result), "+r"(usr) \
        : "r"(src), "i"(BIT) \
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

/* Template for instructions with two register operands */
#define FUNC_x_OP_xx(RESTYPE, SRC1TYPE, SRC2TYPE, NAME, INSN, BIT) \
static RESTYPE NAME(SRC1TYPE src1, SRC2TYPE src2, uint32_t *usr_result) \
{ \
    RESTYPE result; \
    uint32_t usr; \
    asm("r2 = usr\n\t" \
        "r2 = clrbit(r2, #%4)\n\t" \
        "usr = r2\n\t" \
        INSN "\n\t" \
        "%1 = usr\n\t" \
        : "=&r"(result), "+r"(usr) \
        : "r"(src1), "r"(src2), "i"(BIT) \
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
FUNC_R_OP_P(vasrhub_sat,        "%0 = vasrhub(%2, #3):sat",         USR_OVF_BIT)

/* Vector arithmetic shift halfwords with round, saturate and pack */
FUNC_R_OP_P(vasrhub_rnd_sat,    "%0 = vasrhub(%2, #3):rnd:sat",     USR_OVF_BIT)

FUNC_R_OP_RR(addsat,            "%0 = add(%2, %3):sat",             USR_OVF_BIT)
/* Similar to above but with register pairs */
FUNC_P_OP_PP(addpsat,           "%0 = add(%2, %3):sat",             USR_OVF_BIT)

FUNC_R_OP_RR(mpy_acc_sat_hh_s0, "%0 = #0x7fffffff\n\t"
                                "%0 += mpy(%2.H, %3.H):sat",        USR_OVF_BIT)
FUNC_R_OP_RR(mpy_sat_hh_s1,     "%0 = mpy(%2.H, %3.H):<<1:sat",     USR_OVF_BIT)
FUNC_R_OP_RR(mpy_sat_rnd_hh_s1, "%0 = mpy(%2.H, %3.H):<<1:rnd:sat", USR_OVF_BIT)
FUNC_R_OP_RR(mpy_up_s1_sat,     "%0 = mpy(%2, %3):<<1:sat",         USR_OVF_BIT)
FUNC_P_OP_RR(vmpy2s_s1,         "%0 = vmpyh(%2, %3):<<1:sat",       USR_OVF_BIT)
FUNC_P_OP_RR(vmpy2su_s1,        "%0 = vmpyhsu(%2, %3):<<1:sat",     USR_OVF_BIT)
FUNC_R_OP_RR(vmpy2su_s1pack,    "%0 = vmpyh(%2, %3):<<1:rnd:sat",   USR_OVF_BIT)
FUNC_P_OP_PP(vmpy2es_s1,        "%0 = vmpyeh(%2, %3):<<1:sat",      USR_OVF_BIT)
FUNC_R_OP_PP(vdmpyrs_s1,        "%0 = vdmpy(%2, %3):<<1:rnd:sat",   USR_OVF_BIT)
FUNC_P_OP_PP(vdmacs_s0,         "%0 = #0x0fffffff\n\t"
                                "%0 += vdmpy(%2, %3):sat",          USR_OVF_BIT)
FUNC_R_OP_RR(cmpyrs_s0,         "%0 = cmpy(%2, %3):rnd:sat",        USR_OVF_BIT)
FUNC_P_OP_RR(cmacs_s0,          "%0 = #0x0fffffff\n\t"
                                "%0 += cmpy(%2, %3):sat",           USR_OVF_BIT)
FUNC_P_OP_RR(cnacs_s0,          "%0 = #0x8fffffff\n\t"
                                "%0 -= cmpy(%2, %3):sat",           USR_OVF_BIT)
FUNC_P_OP_PP(vrcmpys_s1_h,      "%0 = vrcmpys(%2, %3):<<1:sat:raw:hi",
                                                                    USR_OVF_BIT)
FUNC_P_OP_PP(mmacls_s0,         "%0 = #0x6fffffff\n\t"
                                "%0 += vmpyweh(%2, %3):sat",        USR_OVF_BIT)
FUNC_R_OP_RR(hmmpyl_rs1,        "%0 = mpy(%2, %3.L):<<1:rnd:sat",   USR_OVF_BIT)
FUNC_P_OP_PP(mmaculs_s0,        "%0 = #0x7fffffff\n\t"
                                "%0 += vmpyweuh(%2, %3):sat",       USR_OVF_BIT)
FUNC_R_OP_PR(cmpyi_wh,          "%0 = cmpyiwh(%2, %3):<<1:rnd:sat", USR_OVF_BIT)
FUNC_P_OP_PP(vcmpy_s0_sat_i,    "%0 = vcmpyi(%2, %3):sat",          USR_OVF_BIT)
FUNC_P_OP_PR(vcrotate,          "%0 = vcrotate(%2, %3)",            USR_OVF_BIT)
FUNC_P_OP_PR(vcnegh,            "%0 = vcnegh(%2, %3)",              USR_OVF_BIT)
FUNC_R_OP_PP(wcmpyrw,           "%0 = cmpyrw(%2, %3):<<1:sat",      USR_OVF_BIT)
FUNC_R_OP_RR(addh_l16_sat_ll,   "%0 = add(%2.L, %3.L):sat",         USR_OVF_BIT)
FUNC_P_OP_P(vconj,              "%0 = vconj(%2):sat",               USR_OVF_BIT)
FUNC_P_OP_PP(vxaddsubw,         "%0 = vxaddsubw(%2, %3):sat",       USR_OVF_BIT)
FUNC_P_OP_P(vabshsat,           "%0 = vabsh(%2):sat",               USR_OVF_BIT)
FUNC_P_OP_PP(vnavgwr,           "%0 = vnavgw(%2, %3):rnd:sat",      USR_OVF_BIT)
FUNC_R_OP_R(round_ri_sat,       "%0 = round(%2, #2):sat",           USR_OVF_BIT)
FUNC_R_OP_RR(asr_r_r_sat,       "%0 = asr(%2, %3):sat",             USR_OVF_BIT)

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

int main()
{
    TEST_R_OP_R(satub, 0, 0, 0);
    TEST_R_OP_R(satub, 0xff, 0xff, 0);
    TEST_R_OP_R(satub, 0xfff, 0xff, 1);
    TEST_R_OP_R(satub, -1, 0, 1);

    TEST_P_OP_PP(vaddubs, 0xfeLL, 0x01LL, 0xffLL, 0);
    TEST_P_OP_PP(vaddubs, 0xffLL, 0xffLL, 0xffLL, 1);

    TEST_P_OP_PP(vadduhs, 0xfffeLL, 0x1LL, 0xffffLL, 0);
    TEST_P_OP_PP(vadduhs, 0xffffLL, 0x1LL, 0xffffLL, 1);

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

    TEST_R_OP_P(vsathub, 0x0001000300020001LL, 0x01030201, 0);
    TEST_R_OP_P(vsathub, 0x010000700080ffffLL, 0xff708000, 1);

    TEST_R_OP_P(vsatwuh, 0x0000ffff00000001LL, 0xffff0001, 0);
    TEST_R_OP_P(vsatwuh, 0x800000000000ffffLL, 0x0000ffff, 1);

    TEST_P_OP_P(vsatwuh_nopack, 0x0000ffff00000001LL, 0x0000ffff00000001LL, 0);
    TEST_P_OP_P(vsatwuh_nopack, 0x800000000000ffffLL, 0x000000000000ffffLL, 1);

    TEST_R_OP_R(svsathub, 0x00020001, 0x0201, 0);
    TEST_R_OP_R(svsathub, 0x0080ffff, 0x8000, 1);

    TEST_R_OP_P(vasrhub_sat, 0x004f003f002f001fLL, 0x09070503, 0);
    TEST_R_OP_P(vasrhub_sat, 0x004fffff8fff001fLL, 0x09000003, 1);

    TEST_R_OP_P(vasrhub_rnd_sat, 0x004f003f002f001fLL, 0x0a080604, 0);
    TEST_R_OP_P(vasrhub_rnd_sat, 0x004fffff8fff001fLL, 0x0a000004, 1);

    TEST_R_OP_RR(addsat, 1, 2, 3, 0);
    TEST_R_OP_RR(addsat, 0x7fffffff, 0x00000010, 0x7fffffff, 1);
    TEST_R_OP_RR(addsat, 0x80000000, 0x80000006, 0x80000000, 1);

    TEST_P_OP_PP(addpsat, 1LL, 2LL, 3LL, 0);
    /* overflow to max positive */
    TEST_P_OP_PP(addpsat, 0x7ffffffffffffff0LL, 0x0000000000000010LL,
                 0x7fffffffffffffffLL, 1);
    /* overflow to min negative */
    TEST_P_OP_PP(addpsat, 0x8000000000000003LL, 0x8000000000000006LL,
                 0x8000000000000000LL, 1);

    TEST_R_OP_RR(mpy_acc_sat_hh_s0, 0xffff0000, 0x11110000, 0x7fffeeee, 0);
    TEST_R_OP_RR(mpy_acc_sat_hh_s0, 0x7fff0000, 0x7fff0000, 0x7fffffff, 1);

    TEST_R_OP_RR(mpy_sat_hh_s1, 0xffff0000, 0x11110000, 0xffffddde, 0);
    TEST_R_OP_RR(mpy_sat_hh_s1, 0x7fff0000, 0x7fff0000, 0x7ffe0002, 0);
    TEST_R_OP_RR(mpy_sat_hh_s1, 0x80000000, 0x80000000, 0x7fffffff, 1);

    TEST_R_OP_RR(mpy_sat_rnd_hh_s1, 0xffff0000, 0x11110000, 0x00005dde, 0);
    TEST_R_OP_RR(mpy_sat_rnd_hh_s1, 0x7fff0000, 0x7fff0000, 0x7ffe8002, 0);
    TEST_R_OP_RR(mpy_sat_rnd_hh_s1, 0x80000000, 0x80000000, 0x7fffffff, 1);

    TEST_R_OP_RR(mpy_up_s1_sat, 0xffff0000, 0x11110000, 0xffffddde, 0);
    TEST_R_OP_RR(mpy_up_s1_sat, 0x7fff0000, 0x7fff0000, 0x7ffe0002, 0);
    TEST_R_OP_RR(mpy_up_s1_sat, 0x80000000, 0x80000000, 0x7fffffff, 1);

    TEST_P_OP_RR(vmpy2s_s1, 0x7fff0000, 0x7fff0000, 0x7ffe000200000000LL, 0);
    TEST_P_OP_RR(vmpy2s_s1, 0x80000000, 0x80000000, 0x7fffffff00000000LL, 1);

    TEST_P_OP_RR(vmpy2su_s1, 0x7fff0000, 0x7fff0000, 0x7ffe000200000000LL, 0);
    TEST_P_OP_RR(vmpy2su_s1, 0xffffbd97, 0xffffffff, 0xfffe000280000000LL, 1);

    TEST_R_OP_RR(vmpy2su_s1pack, 0x7fff0000, 0x7fff0000, 0x7ffe0000, 0);
    TEST_R_OP_RR(vmpy2su_s1pack, 0x80008000, 0x80008000, 0x7fff7fff, 1);

    TEST_P_OP_PP(vmpy2es_s1, 0x7fff7fff7fff7fffLL, 0x1fff1fff1fff1fffLL,
                 0x1ffec0021ffec002LL, 0);
    TEST_P_OP_PP(vmpy2es_s1, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fffffff7fffffffLL, 1);

    TEST_R_OP_PP(vdmpyrs_s1, 0x7fff7fff7fff7fffLL, 0x1fff1fff1fff1fffLL,
                 0x3ffe3ffe, 0);
    TEST_R_OP_PP(vdmpyrs_s1, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fff7fffLL, 1);

    TEST_P_OP_PP(vdmacs_s0, 0x00ff00ff00ff00ffLL, 0x00ff00ff00ff00ffLL,
                 0x0001fc021001fc01LL, 0);
    TEST_P_OP_PP(vdmacs_s0, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fffffff7fffffffLL, 1);

    TEST_R_OP_RR(cmpyrs_s0, 0x7fff0000, 0x7fff0000, 0x0000c001, 0);
    TEST_R_OP_RR(cmpyrs_s0, 0x80008000, 0x80008000, 0x7fff0000, 1);

    TEST_P_OP_RR(cmacs_s0, 0x7fff0000, 0x7fff0000,
                 0x00000000d000fffeLL, 0);
    TEST_P_OP_RR(cmacs_s0, 0x80008000, 0x80008000,
                 0x7fffffff0fffffffLL, 1);

    TEST_P_OP_RR(cnacs_s0, 0x7fff0000, 0x7fff0000,
                 0x00000000cfff0000LL, 0);
    TEST_P_OP_RR(cnacs_s0, 0x00002001, 0x00007ffd,
                 0x0000000080000000LL, 1);

    TEST_P_OP_PP(vrcmpys_s1_h, 0x00ff00ff00ff00ffLL, 0x00ff00ff00ff00ffLL,
                 0x0003f8040003f804LL, 0);
    TEST_P_OP_PP(vrcmpys_s1_h, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x7fffffff7fffffffLL, 1);

    TEST_P_OP_PP(mmacls_s0, 0x00ff00ff00ff00ffLL, 0x00ff00ff00ff00ffLL,
                 0x0000fe017000fe00LL, 0);
    TEST_P_OP_PP(mmacls_s0, 0x8000800080008000LL, 0x8000800080008000LL,
                 0x3fffc0007fffffffLL, 1);

    TEST_R_OP_RR(hmmpyl_rs1, 0x7fff0000, 0x7fff0001, 0x0000fffe, 0);
    TEST_R_OP_RR(hmmpyl_rs1, 0x80000000, 0x80008000, 0x7fffffff, 1);

    TEST_P_OP_PP(mmaculs_s0, 0xffff800080008000LL, 0xffff800080008000LL,
                 0xffffc00040003fffLL, 0);
    TEST_P_OP_PP(mmaculs_s0, 0x00ff00ff00ff00ffLL, 0x00ff00ff00ff00ffLL,
                 0x0000fe017fffffffLL, 1);

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

    TEST_R_OP_PP(wcmpyrw, 0x8765432101234567LL, 0x00000002ffffffffLL,
                 0x00000001, 0);
    TEST_R_OP_PP(wcmpyrw, 0x800000007fffffffLL, 0x000000ff7fffffffLL,
                 0x7fffffff, 1);
    TEST_R_OP_PP(wcmpyrw, 0x7fffffff80000000LL, 0x7fffffff000000ffLL,
                 0x80000000, 1);

    TEST_R_OP_RR(addh_l16_sat_ll, 0x0000ffff, 0x00000002, 0x00000001, 0);
    TEST_R_OP_RR(addh_l16_sat_ll, 0x00007fff, 0x00000005, 0x00007fff, 1);
    TEST_R_OP_RR(addh_l16_sat_ll, 0x00008000, 0x00008000, 0xffff8000, 1);

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

    TEST_R_OP_R(round_ri_sat, 0x0000ffff, 0x00004000, 0);
    TEST_R_OP_R(round_ri_sat, 0x7fffffff, 0x1fffffff, 1);

    TEST_R_OP_RR(asr_r_r_sat, 0x0000ffff, 0x00000002, 0x00003fff, 0);
    TEST_R_OP_RR(asr_r_r_sat, 0x00ffffff, 0xfffffff5, 0x7fffffff, 1);
    TEST_R_OP_RR(asr_r_r_sat, 0x80000000, 0xfffffff5, 0x80000000, 1);

    puts(err ? "FAIL" : "PASS");
    return err;
}
