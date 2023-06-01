/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

#define SYSCFG_PCYCLEEN_BIT             6
#define SSR_CE_BIT                      23
#define SSR_PE_BIT                      24
#define MAX_HW_THREADS                  6

int err;
#include "../hex_test.h"

static inline void __check_range(uint32_t val, uint32_t min, uint32_t max, int line)
{
    if (val < min || val > max) {
        printf("ERROR at line %d: %" PRIu32 " not in [%" PRIu32 ", %" PRIu32 "]\n",
            line, val, min, max);
        err++;
    }
}

#define check_range(V, MIN, MAX) __check_range(V, MIN, MAX, __LINE__)

static void test_pcycle(void)
{
    uint64_t pcycle;
    uint32_t pcyclelo, pcyclehi;

    asm volatile("r2 = syscfg\n\t"
                 "r2 = clrbit(r2, #%3)\n\t"
                 "syscfg = r2\n\t"
                 "isync\n\t"
                 "r2 = setbit(r2, #%3)\n\t"
                 "syscfg = r2\n\t"
                 "isync\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "syncht\n\t" /* flush TB */
                 "%0 = pcycle\n\t"
                 "%1 = pcyclehi\n\t"
                 "%2 = pcyclelo\n\t"
                 : "=r"(pcycle),  "=r"(pcyclehi),
                   "=r"(pcyclelo)
                 : "i"(SYSCFG_PCYCLEEN_BIT)
                 : "r2", "r3");

    /*
     * QEMU executes threads one at a time, but hexagon-sim interleaves them.
     * So, we check a range of pcycles to make the same test pass on both.
     */
    check32(pcyclehi, 0);
    check_range(pcycle, 6, 6 * MAX_HW_THREADS);
    check_range(pcyclelo, 6, 6 * MAX_HW_THREADS);
}

static void test_pcycle_read(void)
{
    uint64_t val0 = 0;
    uint64_t val1 = 0;

    asm volatile (
        "%0 = PCYCLE\n\t"
        "isync\n\t"
        "%1 = PCYCLE\n\t"
        : "=r"(val0), "=r"(val1)
    );

    check32_ne(val0, val1);

    val0 = 0;
    val1 = 0;

    asm volatile (
        "r0 = #0x52\n\t"
        "trap0(#0)\n\t"
        "%0 = r1:0\n\t"
        "r0 = #0x52\n\t"
        "trap0(#0)\n\t"
        "%1 = r1:0\n\t"
        : "=r"(val0), "=r"(val1)
        :: "r0", "r1"
    );

    check32_ne(val0, val1);
}

#define read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi) \
    asm volatile("syncht\n\t" \
                 "%0 = pcyclelo\n\t" \
                 "%1 = pcyclehi\n\t" \
                 "%2 = upcyclelo\n\t" \
                 "%3 = upcyclehi\n\t" \
                 : "=r"(pcyclelo), "=r"(pcyclehi), \
                   "=r"(upcyclelo), "=r"(upcyclehi))

#define read_upcycle_reg_pair(pcycle, upcycle) \
    asm volatile("syncht\n\t" \
                 "%0 = pcycle\n\t" \
                 "%1 = upcycle\n\t" \
                 : "=r"(pcycle), "=r"(upcycle)) \


#define read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi) \
    asm volatile("syncht\n\t" \
                 "%0 = pcyclelo\n\t" \
                 "%1 = pcyclehi\n\t" \
                 "%2 = gpcyclelo\n\t" \
                 "%3 = gpcyclehi\n\t" \
                 : "=r"(pcyclelo), "=r"(pcyclehi), \
                   "=r"(gpcyclelo), "=r"(gpcyclehi))

#define read_gpcycle_reg_pair(pcycle, gpcycle) \
    asm volatile("syncht\n\t" \
                 "%0 = pcycle\n\t" \
                 "%1 = g25:24\n\t" \
                 : "=r"(pcycle), "=r"(gpcycle)) \

static void clr_ssr_ce(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = clrbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_CE_BIT));
}

static void set_ssr_ce(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = setbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_CE_BIT));
}

static void test_upcycle(void)
{
    uint32_t pcyclelo, pcyclehi;
    uint32_t upcyclelo, upcyclehi;
    uint64_t pcycle, upcycle;

    /*
     * Before SSR[CE] is set, upcycle registers should be zero
     */
    clr_ssr_ce();
    read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi);
    check32(upcyclelo, 0);
    check32(upcyclehi, 0);

    read_upcycle_reg_pair(pcycle, upcycle);
    check32(upcycle, 0);

    /*
     * After SSR[CE] is set, upcycle registers should match pcycle
     */
    set_ssr_ce();
    read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi);
    check_range(upcyclelo, pcyclelo, pcyclelo * MAX_HW_THREADS);
    check32(upcyclehi, pcyclehi);

    read_upcycle_reg_pair(pcycle, upcycle);
    check_range(upcycle, pcycle, pcycle * MAX_HW_THREADS);
}

static void test_gpcycle(void)
{
    uint32_t pcyclelo, pcyclehi;
    uint32_t gpcyclelo, gpcyclehi;
    uint64_t pcycle, gpcycle;

    /*
     * Before SSR[CE] is set, gpcycle registers should be zero
     */
    clr_ssr_ce();
    read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi);
    check32(gpcyclelo, 0);
    check32(gpcyclehi, 0);

    read_gpcycle_reg_pair(pcycle, gpcycle);
    check32(gpcycle, 0);

    /*
     * After SSR[CE] is set, gpcycle registers should match pcycle
     */
    set_ssr_ce();
    read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi);
    check_range(gpcyclelo, pcyclelo, pcyclelo * MAX_HW_THREADS);
    check32(gpcyclehi, pcyclehi);

    read_gpcycle_reg_pair(pcycle, gpcycle);
    check_range(gpcycle, pcycle, pcycle * MAX_HW_THREADS);
}

#define read_gcycle_xt_regs(gcycle_1t, gcycle_2t, gcycle_3t, \
                            gcycle_4t, gcycle_5t, gcycle_6t) \
    asm volatile("syncht\n\t" \
                 "%0 = gpcycle1t\n\t" \
                 "%1 = gpcycle2t\n\t" \
                 "%2 = gpcycle3t\n\t" \
                 "%3 = gpcycle4t\n\t" \
                 "%4 = gpcycle5t\n\t" \
                 "%5 = gpcycle6t\n\t" \
                 : "=r"(gcycle_1t), "=r"(gcycle_2t), \
                   "=r"(gcycle_3t), "=r"(gcycle_4t), \
                   "=r"(gcycle_5t), "=r"(gcycle_6t))

static void clr_ssr_pe(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = clrbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_PE_BIT));
}

static void set_ssr_pe(void)
{
    asm volatile("r2 = ssr\n\t"
                 "r2 = setbit(r2, #%0)\n\t"
                 "ssr = r2\n\t"
                 : : "i"(SSR_PE_BIT));
}

static inline uint32_t read_pcyclelo(void)
{
    uint32_t ret;
    asm volatile("%0 = pcyclelo\n\t" : "=r"(ret));
    return ret;
}

static void test_gcycle_xt(void)
{
    uint32_t gcycle_1t, gcycle_2t, gcycle_3t,
             gcycle_4t, gcycle_5t, gcycle_6t;

    /*
     * Before SSR[PE] is set, gcycleXt registers should be zero
     */
    clr_ssr_pe();
    read_gcycle_xt_regs(gcycle_1t, gcycle_2t, gcycle_3t,
                        gcycle_4t, gcycle_5t, gcycle_6t);
    check32(gcycle_1t, 0);
    check32(gcycle_2t, 0);
    check32(gcycle_3t, 0);
    check32(gcycle_4t, 0);
    check32(gcycle_5t, 0);
    check32(gcycle_6t, 0);

    /*
     * After SSR[PE] is set, gcycleXt registers have real values
     */
    set_ssr_pe();
    read_gcycle_xt_regs(gcycle_1t, gcycle_2t, gcycle_3t,
                        gcycle_4t, gcycle_5t, gcycle_6t);
    /*
     * This is a single-threaded test, so only gcycle_1t should be non-zero
     * The range of values we check lets the test pass on hexagon-sim and
     * gives some ability for the underlying runtime code to change.
     */
    check_range(gcycle_1t, 1850, 7500);
    check32(gcycle_2t, 0);
    check32(gcycle_3t, 0);
    check32(gcycle_4t, 0);
    check32(gcycle_5t, 0);
    check32(gcycle_6t, 0);
}

int main()
{
    puts("Check the pcyclehi/pcyclelo counters in qemu");

    test_gcycle_xt();
    test_pcycle();
    test_pcycle_read();
    test_gpcycle();
    test_upcycle();

    puts(err ? "FAIL" : "PASS");
    return err;
}
