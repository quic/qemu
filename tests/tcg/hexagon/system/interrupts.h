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

#ifndef INTERRUPTS_H
#define INTERRUPTS_H

#include <stdint.h>

void global_int_disable()
{
    asm volatile("r0 = syscfg\n\t"
                 "r0 = clrbit(r0, #4)\n\t"
                 "syscfg = r0\n\t"
                 :
                 :
                 : "r0");
}

void global_int_enable()
{
    asm volatile("r0 = syscfg\n\t"
                 "r0 = setbit(r0, #4)\n\t"
                 "syscfg = r0\n\t"
                 :
                 :
                 : "r0");
}

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
int vals[512];
int delay(int iters)
{
    for (int i = 0; i < iters; i++) {
        vals[i % ARRAY_SIZE(vals)] = i;
        asm volatile("pause(#1)\n\t");
        vals[i % ARRAY_SIZE(vals)]++;
    }
    return vals[0];
}

static inline void swi(uint32_t mask)
{
    asm volatile("r0 = %0\n"
                 "swi(r0)\n"
                 :
                 : "r"(mask)
                 : "r0");
}

static inline void set_thread_imask(uint32_t mask)
{
    asm volatile("r0 = %0\n"
                 "imask = r0\n"
                 :
                 : "r"(mask)
                 : "r0");
}

#endif
