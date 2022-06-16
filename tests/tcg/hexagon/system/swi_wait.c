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

#include "interrupts.h"
#include <hexagon_standalone.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MAX_INT_NUM (8)
#define ALL_INTERRUPTS_MASK (0xff)

/* volatile bacause it tracks when interrupts have been processed */
volatile int ints_by_irq[MAX_INT_NUM];

static bool all_ints_delivered(int n)
{
    bool all_delivered = true;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        bool delivered = (ints_by_irq[i] == n);
        if (!delivered) {
            printf("ints_by_irq[%d] = %d\n", i, ints_by_irq[i]);
        }
        all_delivered = all_delivered && delivered;
    }
    return all_delivered;
}

volatile bool tasks_enabled = true;
volatile int times_woken = 0;
bool x;
static void wait_thread(void *param)
{
    printf("wait thread spawned\n");
    while (tasks_enabled) {
        printf("> wait thread waiting\n");
        wait_for_interrupts();
        times_woken++;
        x = tasks_enabled;
    }
}

static long long wait_stack[1024];
void wait_for_wake_count(int wake_count) {
    while (times_woken != wake_count) {
        printf("checking times woken %d, wake_count to match %d\n", times_woken, wake_count);
        asm volatile("pause(#1)\n\t");
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

    static int WAIT_THREAD_ID = 1;
    thread_create(wait_thread,
            &wait_stack[ARRAY_SIZE(wait_stack) - 1], WAIT_THREAD_ID,
            NULL);


    delay(10000);
    swi(ALL_INTERRUPTS_MASK);
    printf("waiting for wake\n");
    wait_for_wake_count(1);
    delay(1000);

    assert(all_ints_delivered(1));

    delay(10000);
    global_int_disable();
    swi(ALL_INTERRUPTS_MASK);
    global_int_enable();
    printf("waiting for wake\n");
    wait_for_wake_count(2);
    delay(1000);

    assert(all_ints_delivered(2));

    delay(10000);
    int INT_THREAD_MASK = 0xff;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        iassignw(i, INT_THREAD_MASK);
    }
    swi(ALL_INTERRUPTS_MASK);
    /* Now unmask: */
    INT_THREAD_MASK = ~(1 << WAIT_THREAD_ID);
    for (int i = 0; i < MAX_INT_NUM; i++) {
        iassignw(i, INT_THREAD_MASK);
    }
    wait_for_wake_count(3);
    delay(1000);

    assert(all_ints_delivered(3));

    /* Teardown: */
    tasks_enabled = false;
    for (int i = 0; i < 5; i++) {
        swi(0x1);
        asm volatile("pause(#1)\n\t");
    }
    thread_join(1 << WAIT_THREAD_ID);

    puts("PASS");
    return 0;
}
