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

int main()
{
    puts("Check the pcyclehi/pcyclelo counters in qemu");
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
  check_range(pcyclehi, 0, 0);
  check_range(pcyclelo, 6, 6 * MAX_HW_THREADS);

  puts(err ? "FAIL" : "PASS");
  return err;
}
