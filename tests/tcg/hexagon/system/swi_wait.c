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

#include "interrupts.h"
#include "util.h"
#include <hexagon_standalone.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_INT_NUM (8)
#define ALL_INTERRUPTS_MASK (0xff)

/* volatile because it tracks when interrupts have been processed */
volatile int ints_by_irq[MAX_INT_NUM]; /* volatile required here */

static bool all_ints_delivered(int n)
{
    bool all_delivered = true;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        bool delivered = (ints_by_irq[i] == n);
        if (!delivered) {
            printf("ints_by_irq[%d] = %d, expected %d\n", i, ints_by_irq[i], n);
        }
        all_delivered = all_delivered && delivered;
    }
    return all_delivered;
}

static void wait_for_ints_delivered(int n, int mask)
{
    bool all_delivered = false;
    while (!all_delivered) {
        int delivered_mask = 0;
        for (int i = 0; i < MAX_INT_NUM; i++) {
            if ((mask & (1 << i)) == 0) {
                continue;
            }
            if (ints_by_irq[i] == n) {
                delivered_mask = delivered_mask | (1 << n);
            } else {
                printf("ints_by_irq[%d] == %d\n", i, ints_by_irq[i]);
            }
        }
        printf("delivered mask 0x%02x, compare mask 0x%02x\n",
            delivered_mask, mask);
        all_delivered = delivered_mask == mask;
    }
}

uint32_t read_modectl(void)
{
    unsigned modectl;
    asm volatile("%0 = MODECTL\n\t" : "=r"(modectl));
    return modectl;
}

void wait_for_thread(int tid)
{
    unsigned tmode;
    do {
        unsigned modectl = read_modectl();
        tmode = modectl & (0x1 << (16 + tid));
    } while (tmode == 0);
}
volatile bool tasks_enabled = true; /* multiple hw threads running */
volatile int times_woken; /* multiple hw threads running */
static void wait_thread(void *param)
{
    while (tasks_enabled) {
        wait_for_interrupts();
        times_woken++;
    }
}

static long long wait_stack[1024];
void wait_for_wake_count(int wake_count)
{
    while (times_woken < wake_count) {
        pcycle_pause(10000);
    }
}

static inline uint32_t get_htid(void)
{
    uint32_t htid;
    asm volatile("%0 = htid\n\t" : "=r"(htid));
    return htid;
}

static void interrupt_handler(int intno)
{
    uint32_t htid = get_htid();
    ints_by_irq[intno]++;
}

int main()
{
    for (int i = 0; i < MAX_INT_NUM; i++) {
        register_interrupt(i, interrupt_handler);
    }

    set_thread_imask(ALL_INTERRUPTS_MASK);

    static int WAIT_THREAD_ID = 1;
    thread_create(wait_thread,
            &wait_stack[ARRAY_SIZE(wait_stack) - 1], WAIT_THREAD_ID,
            NULL);
    /* make sure thread is up and in wait state before sending int */
    wait_for_thread(WAIT_THREAD_ID);

    static int INT_MASK = ALL_INTERRUPTS_MASK;

    /* Test ordinary swi interrupts */
    swi(INT_MASK);
    printf("waiting for wake #1\n");
    wait_for_wake_count(1);
    printf("\twake count now %d\n", times_woken);
    assert(all_ints_delivered(1));

    /*
     * Test swi interrupts, triggered
     * while ints disabled.
     */
    wait_for_thread(WAIT_THREAD_ID);
    global_int_disable();
    swi(INT_MASK);
    global_int_enable();
    wait_for_wake_count(2);
    printf("\twake count now %d\n", times_woken);
    assert(all_ints_delivered(2));

    /*
     * Test swi interrupts, triggered
     * while ints masked for all threads.
     */
    delay(1);
    int INT_THREAD_MASK = 0x1f;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        iassignw(i, INT_THREAD_MASK);
    }
    swi(INT_MASK);
    /* Now unmask: */
    INT_THREAD_MASK = ~(1 << WAIT_THREAD_ID);
    for (int i = 0; i < MAX_INT_NUM; i++) {
        iassignw(i, INT_THREAD_MASK);
    }
    delay(1);
    printf("waiting for wake #3\n");
    wait_for_wake_count(3);
    printf("\twake count now %d\n", times_woken);
    assert(all_ints_delivered(3));

    /* Teardown: */
    tasks_enabled = false;
    for (int i = 0; i < 5; i++) {
        swi(0x1);
        pcycle_pause(100000);
    }
    thread_join(1 << WAIT_THREAD_ID);

    puts("PASS");
    return 0;
}
