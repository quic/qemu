/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
 * Test instructions where the semantics write to the destination
 * before all the operand reads have been completed.
 *
 * These instructions are problematic when we short-circuit the
 * register writes because the destination and source operands could
 * be the same TCGv.
 *
 * We test by forcing the read and write to be register r7.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

int err;

#include "hex_test.h"

#define insert(RES, X, WIDTH, OFFSET) \
    asm("r7 = %1\n\t" \
        "r7 = insert(r7, #" #WIDTH ", #" #OFFSET ")\n\t" \
        "%0 = r7\n\t" \
        : "=r"(RES) : "r"(X) : "r7")

static void test_insert(void)
{
    uint32_t res;

    insert(res, 0x12345678, 8, 1);
    check32(res, 0x123456f0);
    insert(res, 0x12345678, 0, 1);
    check32(res, 0x12345678);
    insert(res, 0x12345678, 20, 16);
    check32(res, 0x56785678);
}

static inline uint32_t insert_rp(uint32_t x, uint32_t width, uint32_t offset)
{
    uint64_t width_offset = (uint64_t)width << 32 | offset;
    uint32_t res;
    asm("r7 = %1\n\t"
        "r7 = insert(r7, %2)\n\t"
        "%0 = r7\n\t"
        : "=r"(res) : "r"(x), "r"(width_offset) : "r7");
    return res;

}

static void test_insert_rp(void)
{
    check32(insert_rp(0x12345678,   8,  1), 0x123456f0);
    check32(insert_rp(0x12345678,  63,  8), 0x34567878);
    check32(insert_rp(0x12345678, 127,  8), 0x34567878);
    check32(insert_rp(0x12345678,   8, 24), 0x78345678);
    check32(insert_rp(0x12345678,   8, 63), 0x12345678);
    check32(insert_rp(0x12345678,   8, 64), 0x00000000);
}

static inline uint32_t asr_r_svw_trun(uint64_t x, uint32_t y)
{
    uint32_t res;
    asm("r7 = %2\n\t"
        "r7 = vasrw(%1, r7)\n\t"
        "%0 = r7\n\t"
        : "=r"(res) : "r"(x), "r"(y) : "r7");
    return res;
}

static void test_asr_r_svw_trun(void)
{
    check32(asr_r_svw_trun(0x1111111122222222ULL, 5),
            0x88881111);
    check32(asr_r_svw_trun(0x1111111122222222ULL, 63),
            0x00000000);
    check32(asr_r_svw_trun(0x1111111122222222ULL, 64),
            0x00000000);
    check32(asr_r_svw_trun(0x1111111122222222ULL, 127),
            0x22224444);
    check32(asr_r_svw_trun(0x1111111122222222ULL, 128),
            0x11112222);
    check32(asr_r_svw_trun(0xffffffff22222222ULL, 128),
            0xffff2222);
}

static inline uint32_t swiz(uint32_t x)
{
    uint32_t res;
    asm("r7 = %1\n\t"
        "r7 = swiz(r7)\n\t"
        "%0 = r7\n\t"
        : "=r"(res) : "r"(x) : "r7");
    return res;
}

static void test_swiz(void)
{
    check32(swiz(0x11223344), 0x44332211);
}

static inline uint32_t satuh(uint32_t x, bool *ovf)
{
    uint32_t res;
    uint32_t usr;
    asm("r3 = usr\n\t"
        "r3 = and(r3, #0xfffffffe)\n\t"
        "usr = r3\n\t"
        "r3 = %2\n\t"
        "r3 = satuh(r3)\n\t"
        "%0 = r3\n\t"
        "%1 = usr\n\t"
        : "=r"(res), "=r"(usr) : "r"(x) : "r3");
    *ovf = usr & 1;
    return res;
}

static void test_satuh(void)
{
    bool ovf;
    check32(satuh(0x10000, &ovf), 0xffff);
    check32(ovf, 1);
    check32(satuh(0, &ovf), 0);
    check32(ovf, 0);
    check32(satuh(0xf0000000, &ovf), 0x0);
    check32(ovf, 1);
}

static inline uint32_t addasl_rrri(uint32_t x, uint32_t y)
{
    uint32_t res;
    asm("r3 = %1\n\t"
        "r3 = addasl(r3, %2, #4)\n\t"
        "%0 = r3\n\t"
        : "=r"(res) : "r"(x), "r"(y) : "r3");
    return res;
}

static void test_addasl_rrri(void)
{
    check32(addasl_rrri(0x11111111, 0x11223344), 0x23344551);
}

static inline uint32_t vsplatb(uint32_t x)
{
    uint32_t res;
    asm("r3 = %1\n\t"
        "r3 = vsplatb(r3)\n\t"
        "%0 = r3\n\t"
        : "=r"(res) : "r"(x) : "r3");
    return res;
}

static void test_vsplatb(void)
{
    check32(vsplatb(0x12), 0x12121212);
}

static inline uint32_t combine_ll(uint32_t x, uint32_t y)
{
    uint32_t res;
    asm("r3 = %1\n\t"
        "r3 = combine(r3.L, %2.L)\n\t"
        "%0 = r3\n\t"
        : "=r"(res) : "r"(x), "r"(y) : "r3");
    return res;
}

static void test_combine_ll(void)
{
    check32(combine_ll(0x1111, 0x2222), 0x11112222);
}

static inline uint32_t combine_lh(uint32_t x, uint32_t y)
{
    uint32_t res;
    asm("r3 = %1\n\t"
        "r3 = combine(r3.L, %2.H)\n\t"
        "%0 = r3\n\t"
        : "=r"(res) : "r"(x), "r"(y) : "r3");
    return res;
}

static void test_combine_lh(void)
{
    check32(combine_lh(0x1111, 0x22220000), 0x11112222);
}

static inline uint32_t mpyri_addr_u2(uint32_t x, uint32_t y)
{
    uint32_t res;
    asm("r3 = %2\n\t"
        "r3 = add(r3, mpyi(#16, %1))\n\t"
        "%0 = r3\n\t"
        : "=r"(res) : "r"(x), "r"(y) : "r3");
    return res;
}

static void test_mpyi_addr_u2(void)
{
    check32(mpyri_addr_u2(0x11223344, 0xf), 0x1223344f);
}

static inline uint32_t accii(uint32_t x, uint32_t y)
{
    asm("%0 += add(%1, #8)\n\t"
        : "+r"(x) : "r"(y));
    return x;
}

static void test_accii(void)
{
    check32(accii(0x12345600, 0xf0), 0x123456f8);
}

static inline uint32_t acci(uint32_t x, uint32_t y)
{
    uint32_t res;
    asm("r3 = %2\n\t"
        "r3 += add(%1, r3)\n\t"
        "%0 = r3\n\t"
        : "=r"(res) : "r"(x), "r"(y));
    return res;
}

static void test_acci(void)
{
    check32(acci(0x12345600, 0xf0), 0x123457e0);
}

int main()
{
    test_insert();
    test_insert_rp();
    test_asr_r_svw_trun();
    test_swiz();

    test_satuh();
    test_addasl_rrri();
    test_vsplatb();
    test_combine_ll();
    test_combine_lh();
    test_mpyi_addr_u2();
    test_accii();
    test_acci();

    puts(err ? "FAIL" : "PASS");
    return err ? EXIT_FAILURE : EXIT_SUCCESS;
}
