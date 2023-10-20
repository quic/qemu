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
#include "thread_common.h"
#include <hexagon_standalone.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "filename.h"

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

static long long wait_stack[1024];
volatile bool tasks_enabled = true; /* multiple hw threads running */
volatile int times_woken; /* multiple hw threads running */
static void wait_thread(void *param)
{
    while (tasks_enabled) {
        wait_for_interrupts();
        times_woken++;
    }
}

void wait_for_ints_delivered(int n)
{
    while (!all_ints_delivered(n)) {
        pcycle_pause(10000);
    }
    /*
     * We can't use '==' because more than one interrupt may have been
     * processed in the same "wake window".
     */
    while (times_woken < n) {
        pcycle_pause(10000);
    }
}

static void interrupt_handler(int intno)
{
    ints_by_irq[intno]++;
}

int main()
{
    for (int i = 0; i < MAX_INT_NUM; i++) {
        register_interrupt(i, interrupt_handler);
    }

    set_thread_imask(ALL_INTERRUPTS_MASK);

    const int WAIT_THREAD_ID = 1;
    thread_create_blocked(wait_thread,
            &wait_stack[ARRAY_SIZE(wait_stack) - 1], WAIT_THREAD_ID,
            NULL);
    /* make sure thread is up and in wait state before sending int */
    wait_for_thread(WAIT_THREAD_ID);

    static int INT_MASK = ALL_INTERRUPTS_MASK;

    /* Test ordinary swi interrupts */
    swi(INT_MASK);
    printf("waiting for wake #1\n");
    wait_for_ints_delivered(1);

    /*
     * Test swi interrupts, triggered
     * while ints disabled.
     */
    wait_for_thread(WAIT_THREAD_ID);
    global_int_disable();
    swi(INT_MASK);
    global_int_enable();
    printf("waiting for wake #2\n");
    wait_for_ints_delivered(2);

    /*
     * Test swi interrupts, triggered
     * while ints masked for all threads.
     */
    wait_for_thread(WAIT_THREAD_ID);
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
    printf("waiting for wake #3\n");
    wait_for_ints_delivered(3);

    /* Teardown: */
    tasks_enabled = false;
    for (int i = 0; i < 5; i++) {
        swi(0x1);
        pcycle_pause(100000);
    }
    thread_join(1 << WAIT_THREAD_ID);

    printf("%s : %s\n", "PASS", __FILENAME__);
    return 0;
}
