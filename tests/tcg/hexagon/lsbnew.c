/*
 *  Copyright(c) 2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

int err;

void __check(int line, int result, int expect)
{
    if (result != expect) {
        printf("ERROR at line %d: 0x%08x != 0x%08x\n",
               line, result, expect);
        err++;
    }
}

#define check(RES, EXP) __check(__LINE__, RES, EXP)

void test_lsbnew(void)
{
    int result;

    asm("r0 = #2\n\t"
        "r1 = #5\n\t"
        "{\n\t"
        "    p0 = r0\n\t"
        "    if (p0.new) r1 = #3\n\t"
        "}\n\t"
        "%0 = r1\n\t"
        : "=r"(result) :: "r0", "r1", "p0");
    check(result, 5);
}

int main()
{
    test_lsbnew();

    puts(err ? "FAIL" : "PASS");
    return err ? 1 : 0;
}
