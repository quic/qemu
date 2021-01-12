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

#include <stdio.h>
#include <stdint.h>

/*
 * Make sure we ignore GP when the load is constant extended
 * Approach:
 *     Normally, in linux-user mode GP is always 0
 *     Change it to 4, then do a constant-extended load
 *     Check that the load returned the first element of the array
 */

static inline void *getgp()
{
  void *reg;
  asm volatile ("%0 = GP"
                : "=r"(reg));
  return reg;
}

static inline void putgp(void *x)
{
  asm volatile ("GP = %0"
                :: "r"(x));
}

static inline uint32_t loadgp()
{
  uint32_t reg;
  asm volatile ("%0 = memw(##array)"
                : "=r"(reg));
  return reg;
}

int err;
uint32_t array[2] = { 0xdead, 0xbeef };

static void check(int n, int expect) \
{
    if (n != expect) {
        printf("ERROR: 0x%04x != 0x%04x\n", n, expect);
        err++;
    }
}

int main()
{
    void *old_gp = getgp();
    uint32_t res;

    putgp(old_gp + 4);
    res = loadgp();
    putgp(old_gp);
    check(res, 0xdead);

    puts(err ? "FAIL" : "PASS");
    return err;
}

