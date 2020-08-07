/*
 *  Copyright(c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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


#include <stdint.h>
#include <stdio.h>

#define CSR_BASE1 0xfab00000
#define QTMR_BASE ((CSR_BASE1) + 0x20000)
#define QTMR_CNTP1_CTL ((uint32_t *)((QTMR_BASE) + 0x102c))
#define QTMR_CNTP1_TVAL ((uint32_t *)((QTMR_BASE) + 0x1028))
#define QTMR_FREQ 192000

void timer_init()
{
    *QTMR_CNTP1_TVAL = QTMR_FREQ / 1000;
    *QTMR_CNTP1_CTL = 0x1; // enable
}
uint64_t utimer_read()
{
    uint32_t timer_low, timer_high;
    asm volatile("%1 = utimerhi\n\t"
                 "%0 = utimerlo\n\t"
                 : "=r"(timer_low), "=r"(timer_high));
    return ((uint64_t)timer_high << 32) | timer_low;
}
uint64_t utimer_read_pair()
{
    uint32_t timer_low, timer_high;
#if 0
    /* FIXME */
    asm volatile("r1:0 = c63:62\n\t"
                 "%0 = r0\n\t"
                 "%1 = r1\n\t"
                 : "=r" (timer_low),
                   "=r" (timer_high));
#endif
    return ((uint64_t)timer_high << 32) | timer_low;
}
uint64_t timer_read()
{
    uint32_t timer_low, timer_high;
    asm volatile("%1 = s57\n\t"
                 "%0 = s56\n\t"
                 : "=r"(timer_low), "=r"(timer_high));
    return ((uint64_t)timer_high << 32) | timer_low;
}
uint64_t timer_read_pair()
{
    uint32_t timer_low, timer_high;
    asm volatile("r1:0 = s57:56\n\t"
                 "%0 = r0\n\t"
                 "%1 = r1\n\t"
                 : "=r"(timer_low), "=r"(timer_high));
    return ((uint64_t)timer_high << 32) | timer_low;
}

int main()
{
    timer_init();

    /* FIXME make some test assertions... */
    for (int i = 0; i < 30; i++) {
        uint64_t val = timer_read();
        printf("\treg:   %llu | %08llx\n", val, val);
        val = timer_read_pair();
        printf("\tpair:  %llu | %08llx\n", val, val);
        val = utimer_read();
        printf("\tureg:  %llu | %08llx\n", val, val);
        val = utimer_read_pair();
        printf("\tupair: %llu | %08llx\n", val, val);
    }
}
