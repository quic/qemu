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
#define MAX_THREADS (4 - 1) /* 1 for cycling settings */

#define check(a, b) check_(a, b, __LINE__)
static inline void check_(int a, int b, int linenum)
{
    if (a != b) {
        fprintf(stderr, "error: %d != %d at line %d\n", a, b, linenum);
        abort();
    }
}

static inline uint32_t get_stid(void)
{
    uint32_t stid;
    asm volatile("%0 = stid\n\t" : "=r"(stid));
    return stid;
}

static inline uint32_t get_htid(void)
{
    uint32_t htid;
    asm volatile("%0 = htid\n\t" : "=r"(htid));
    return htid;
}

static inline void set_task_prio(int prio)
{
    static uint32_t PRIO_CLR_MASK = 0xff00ffff;
    prio = prio & 0xff;
    asm volatile("r0 = stid\n\t"
                 "r0 = and(r0, %0)\n\t"
                 "r0 = or(r0, %1)\n\t"
                 "stid = r0\n\t"
                 "isync\n\t"
                 :
                 : "r"(PRIO_CLR_MASK), "r"(prio << 16)
                 : "r0");
}

/* volatile bacause it tracks when interrupts have been processed */
volatile int ints_by_irq[MAX_INT_NUM];

static bool all_ints_delivered(int n)
{
    bool all_delivered = true;
    for (int i = 0; i < MAX_INT_NUM; i++) {
        all_delivered = all_delivered && (ints_by_irq[i] == n);
    }
    return all_delivered;
}

static void cycle_thread_imask(int delay_amt)
{
    set_thread_imask(ALL_INTERRUPTS_MASK);
    delay(delay_amt);
    set_thread_imask(ALL_INTERRUPTS_MASK);
}

static void cycle_ints(int delay_amt)
{
    global_int_disable();
    delay(delay_amt);
    global_int_enable();
}

/* volatile because it tracks when work is finished */
volatile bool tasks_enabled = true;
long long intcycles_stack[1024];
static void interrupt_cycles(void *y)
{
    int iters = 0;
    while (tasks_enabled) {
        int delay_amt = iters % 3;
        cycle_ints(delay_amt);
        cycle_thread_imask(delay_amt);

        cycle_ints(delay_amt);
        cycle_thread_imask(delay_amt);

        set_task_prio(iters % 255);

        iters++;
    }
}

static const int TOTAL_INTS = 10;
static const int INTS_PER_ITER = 5;
static const int TEST_ITER_COUNT = TOTAL_INTS / INTS_PER_ITER;

int x[1024];
int y[1024];
static void task_thread(void *param)
{
    int accum = 0;
    static int bogus_fd = -1;
    while (tasks_enabled) {
        for (int i = 0; i < ARRAY_SIZE(x) && tasks_enabled; i++) {
            for (int j = 0; j < ARRAY_SIZE(y) && tasks_enabled; j++) {
                accum += x[i] * y[j];
                set_task_prio((i + j) % 255);
                write(bogus_fd, x, sizeof(x));
            }
        }
    }
    x[0] = accum;
}

static void interrupt_handler(int intno)
{
    ints_by_irq[intno]++;
}

long long task_stack[MAX_THREADS][1024];
void run_test()
{
    for (int i = 0; i < MAX_INT_NUM; i++) {
        ints_by_irq[i] = 0;
    }

    for (int i = 0; i < INTS_PER_ITER; i++) {
        swi(ALL_INTERRUPTS_MASK);

        while (!all_ints_delivered(i + 1)) {
            delay(5);
        }
    }

    for (int i = 0; i < MAX_INT_NUM; i++) {
        check(ints_by_irq[i], INTS_PER_ITER);
    }
}


int main()
{
    for (int i = 0; i < MAX_INT_NUM; i++) {
        register_interrupt(i, interrupt_handler);
    }

    thread_create(interrupt_cycles,
                  &intcycles_stack[ARRAY_SIZE(intcycles_stack) - 1], 1, NULL);

    int task_thread_count = 2;
    printf("spawning threads\n");
    for (int i = 0; i < task_thread_count; i++) {
        thread_create(task_thread,
                      &task_stack[i][ARRAY_SIZE(task_stack[i]) - 1], i + 1 + 1,
                      (void *)i);
    }
    delay(10);

    printf("running tests\n");
    for (int i = 0; i < TEST_ITER_COUNT; i++) {
        run_test();
        delay(20);
    }
    tasks_enabled = false;

    printf("tests done, waiting for cycle task\n");

    thread_join(1 << 1);
    for (int i = 0; i < task_thread_count; i++) {
        printf("tests done, waiting for task #%d\n", i);
        thread_join(1 << (i + 1 + 1));
    }

    puts("PASS");
    return 0;
}
