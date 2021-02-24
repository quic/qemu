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

#include <stdio.h>
#include <stdint.h>

#define SYSCFG_PCYCLEEN_BIT             6
#define SSR_CE_BIT                      23
#define SSR_PE_BIT                      24
#define MAX_HW_THREADS                  6

int err;

static inline void __check_range(uint32_t val, uint32_t min, uint32_t max, int line)
{
    if (val < min || val > max) {
        printf("ERROR at line %d: %lu not in [%lu, %lu]\n", line, val, min, max);
        err++;
    }
}

#define check_range(V, MIN, MAX) __check_range(V, MIN, MAX, __LINE__)

static inline void __check(uint32_t val, uint32_t expect, int line)
{
    if (val != expect) {
        printf("ERROR at line %d: %lu != %lu\n", line, val, expect);
        err++;
    }
}

#define check(V, E) __check(V, E, __LINE__)

static void test_pcycle(void)
{
    uint32_t pcyclelo, pcyclehi;

    asm volatile("r2 = syscfg\n\t"
                 "r2 = clrbit(r2, #%2)\n\t"
                 "syscfg = r2\n\t"
                 "r2 = setbit(r2, #%2)\n\t"
                 "syscfg = r2\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r2 = add(r2, #1)\n\t"
                 "r3:2 = s31:30\n\t"
                 "%0 = r3\n\t"
                 "%1 = r2\n\t"
                 : "=r"(pcyclehi), "=r"(pcyclelo)
                 : "i"(SYSCFG_PCYCLEEN_BIT)
                 : "r2", "r3");

    /*
     * QEMU executes threads one at a time, but hexagon-sim interleaves them.
     * So, we check a range of pcycles to make the same test pass on both.
     */
    check(pcyclehi, 0);
    check_range(pcyclelo, 6, 6 * MAX_HW_THREADS);
}

#define read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi) \
    asm volatile("%0 = pcyclelo\n\t" \
                 "%1 = pcyclehi\n\t" \
                 "%2 = upcyclelo\n\t" \
                 "%3 = upcyclehi\n\t" \
                 : "=r"(pcyclelo), "=r"(pcyclehi), \
                   "=r"(upcyclelo), "=r"(upcyclehi))

#define read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi) \
    asm volatile("%0 = pcyclelo\n\t" \
                 "%1 = pcyclehi\n\t" \
                 "%2 = gpcyclelo\n\t" \
                 "%3 = gpcyclehi\n\t" \
                 : "=r"(pcyclelo), "=r"(pcyclehi), \
                   "=r"(gpcyclelo), "=r"(gpcyclehi))

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

    /*
     * Before SSR[CE] is set, upcycle registers should be zero
     */
    clr_ssr_ce();
    read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi);
    check(upcyclelo, 0);
    check(upcyclehi, 0);

    /*
     * After SSR[CE] is set, upcycle registers should match pcycle
     */
    set_ssr_ce();
    read_upcycle_regs(pcyclelo, pcyclehi, upcyclelo, upcyclehi);
    check_range(upcyclelo, pcyclelo + 2, pcyclelo + 2 * MAX_HW_THREADS);
    check(upcyclehi, pcyclehi);
}

static void test_gpcycle(void)
{
    uint32_t pcyclelo, pcyclehi;
    uint32_t gpcyclelo, gpcyclehi;

    /*
     * Before SSR[CE] is set, gpcycle registers should be zero
     */
    clr_ssr_ce();
    read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi);
    check(gpcyclelo, 0);
    check(gpcyclehi, 0);

    /*
     * After SSR[CE] is set, gpcycle registers should match pcycle
     */
    set_ssr_ce();
    read_gpcycle_regs(pcyclelo, pcyclehi, gpcyclelo, gpcyclehi);
    check_range(gpcyclelo, pcyclelo + 2, pcyclelo + 2 * MAX_HW_THREADS);
    check(gpcyclehi, pcyclehi);

}

#define read_gcycle_xt_regs(gcycle_1t, gcycle_2t, gcycle_3t, \
                            gcycle_4t, gcycle_5t, gcycle_6t) \
    asm volatile("%0 = gpcycle1t\n\t" \
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
    check(gcycle_1t, 0);
    check(gcycle_2t, 0);
    check(gcycle_3t, 0);
    check(gcycle_4t, 0);
    check(gcycle_5t, 0);
    check(gcycle_6t, 0);

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
    check(gcycle_2t, 0);
    check(gcycle_3t, 0);
    check(gcycle_4t, 0);
    check(gcycle_5t, 0);
    check(gcycle_6t, 0);
}

int main()
{
    puts("Check the pcyclehi/pcyclelo counters in qemu");

    test_gcycle_xt();
    test_pcycle();
    test_upcycle();
    test_gpcycle();

    puts(err ? "FAIL" : "PASS");
    return err;
}
