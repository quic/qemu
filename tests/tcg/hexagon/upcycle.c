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

#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

int err;

static inline void __check_range(uint32_t val, uint32_t min, uint32_t max, int line)
{
    if (val < min || val > max) {
        fprintf(stderr,
                "ERROR at line %d: %" PRIu32 " not in [%" PRIu32 ", %" PRIu32 "]\n",
                line, val, min, max);
        err++;
    }
}

#define check_range(V, MIN, MAX) __check_range(V, MIN, MAX, __LINE__)

static inline void __check(uint32_t val, uint32_t expect, int line)
{
    if (val != expect) {
        fprintf(stderr,
                "ERROR at line %d: %" PRIu32 " != %" PRIu32 "\n",
                line, val, expect);
        err++;
    }
}

#define check(V, E) __check(V, E, __LINE__)

int main()
{
    uint32_t upcyclelo, upcyclehi;
    uint64_t upcycle;

    puts("Check that we can read upcycle counters in linux-user mode");

    /*
     * On linux-user mode, we assume SSR[CE] is always set.
     */
    asm volatile("%0 = upcycle\n\t"
                 "%1 = upcyclelo\n\t"
                 "%2 = upcyclehi\n\t"
                 : "=r"(upcycle), "=r"(upcyclelo), "=r"(upcyclehi));

    check(upcyclehi, 0);
    check(upcyclelo, upcycle);
    check_range(upcycle, 3500, 6500);

    puts(err ? "FAIL" : "PASS");
    return err;
}
