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


#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define QTMR_BASE ((CSR_BASE) + 0x20000)
#define QTMR_CNTP1_CTL ((uint32_t *)((QTMR_BASE) + 0x102c))
#define QTMR_CNTP1_TVAL ((uint32_t *)((QTMR_BASE) + 0x1028))
#define QTMR_FREQ 19200000ULL

#if 0
#define DEBUG printf
#else
#define DEBUG(...)
#endif

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
    asm volatile("r1:0 = utimer\n\t"
                 "%0 = r0\n\t"
                 "%1 = r1\n\t"
                 : "=r" (timer_low),
                   "=r" (timer_high));
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

    uint64_t start = timer_read();
    for (int i = 0; i < 30; i++) {
        uint64_t val = timer_read();
        DEBUG("\treg:   %llu | %08llx\n", val, val);
        val = timer_read_pair();
        assert(val != 0);
        DEBUG("\tpair:  %llu | %08llx\n", val, val);
        val = utimer_read();
        assert(val != 0);
        DEBUG("\tureg:  %llu | %08llx\n", val, val);
        val = utimer_read_pair();
        DEBUG("\tupair: %llu | %08llx\n", val, val);
        assert(val != 0);
    }
    assert(start != timer_read());
    puts("PASS");
}
