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

#include "invalid_opcode.h"

/* Using volatile because we are testing atomics */
volatile int mem;
static void test_interrupt_cleans_llsc(void)
{
    int res = 0;

    asm volatile("1: r1 = memw_locked(%1)\n"
                 "   p0 = cmp.eq(r1,#0)\n"
                 "   if (!p0) jump 1b\n"
                 /* invalid opcode should trigger an exception */
                 "   call invalid_opcode\n"
                 /*
                  * this should return false in p0 as the exception
                  * should clean the llsc state.
                  */
                 "   r1 = #1\n"
                 "   memw_locked(%1, p0) = r1\n"
                 "   %0 = p0\n"
                 : "=r"(res) : "r"(&mem) : "r1", "p0");

    if (res || mem) {
        err++;
    }
}

INVALID_OPCODE_MAIN("LLSC on exception test", test_interrupt_cleans_llsc)
