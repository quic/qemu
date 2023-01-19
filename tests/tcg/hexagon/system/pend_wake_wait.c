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

#include "hexagon_standalone.h"
#include "interrupts.h"
#include "util.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int MAX_INT_NUM = 8;
static const int ALL_INTERRUPTS_MASK = 0xff;

static inline void k0lock(void)
{
    asm volatile("k0lock\n");
}

static inline void wait(void)
{
    asm volatile("wait(r0)\n");
}

static inline void k0unlock(void)
{
    asm volatile("k0unlock\n");
}

static int buf[32 * 1024];
static int isr_buf[ARRAY_SIZE(buf)];
static void work()
{
    memcpy(buf, isr_buf, sizeof(buf));
}
static void isr_work()
{
    memcpy(isr_buf, buf, sizeof(isr_buf));
}


#define COMPUTE_THREADS           3
#define STACK_SIZE            16384
char stack[COMPUTE_THREADS][STACK_SIZE] __attribute__((__aligned__(8)));

static volatile int threads_started; /* used by different hw threads */
static const int ROUNDS = 10;

static volatile int thread_exit_flag; /* used by different hw threads */
static void thread_func(void *arg)
{
    int taskId = (int) arg;
    set_thread_imask(0);
    __atomic_fetch_add(&threads_started, 1, __ATOMIC_SEQ_CST);
    unsigned long long i = 0;
    while (thread_exit_flag == 0) {
        k0lock();
        pcycle_pause(100);
        k0unlock();
        printf("thread[%d] round %llu\n", taskId, ++i);
    }
    set_thread_imask(ALL_INTERRUPTS_MASK);
    printf("thread exiting %d\n", taskId);
}

static volatile int ints_handled; /* used by different hw threads */
static void interrupt_handler(int intno)
{
    k0lock();
    isr_work();
    k0unlock();
    __atomic_fetch_add(&ints_handled, 1, __ATOMIC_SEQ_CST);
}

static inline uint32_t get_modectl()
{
    uint32_t modectl;
    asm volatile("%0 = modectl\n"
                : "=r"(modectl)
                );
    return modectl;
}

static inline uint32_t get_wait_mask()
{
    uint32_t modectl = get_modectl();
    return (modectl >> 16) & 0xff;
}

static inline uint32_t get_enabled_mask()
{
    uint32_t modectl = get_modectl();
    return modectl & 0xff;
}

static bool some_threads_running()
{
    uint32_t W = get_wait_mask();
    uint32_t E = get_enabled_mask();
    bool some_running = false;
    unsigned running_mask = 0x0;
    for (int i = 1; i < COMPUTE_THREADS + 1; i++) {
        uint32_t th_bit = 1 << i;
        bool th_w = (W & th_bit) != 0;
        bool th_e = (E & th_bit) != 0;
        printf("thread %d, w: %d, e: %d\n", i, th_w, th_e);
        if (th_w || th_e) {
            some_running = true;
            running_mask |= th_bit;
        }
    }
    printf("some running: %d : mask 0x%x\n", some_running, running_mask);
    return some_running;
}

int main(int argc, char *argv[])
{
    for (int i = 0; i < MAX_INT_NUM; i++) {
        register_interrupt(i, interrupt_handler);
    }
    set_thread_imask(ALL_INTERRUPTS_MASK);
    memset(buf, 0xa5, sizeof(buf));

    puts("Testing pend/wake/wait");
    for (int i = 0; i < COMPUTE_THREADS; i++) {
        thread_create((void *)thread_func, &stack[i][ARRAY_SIZE(stack[i]) - 8],
                      i + 1, (void *)(i + 1));
    }

    while (threads_started < COMPUTE_THREADS) {
        printf("Threads started: %d\n", threads_started);
        pcycle_pause(10000);
    }
    printf("\tnow threads started: %d\n", threads_started);

    for (int i = 0; i < ROUNDS; i++) {
        int ints_expected = ints_handled + 1;
        printf("Round %d/%d/%d, doing work for each int: expecting %d\n", i,
               ROUNDS, MAX_INT_NUM, ints_expected);
        for (int j = 0; j < MAX_INT_NUM; j++) {
            work();
            while (ints_handled < ints_expected) {
                swi(1 << j);
                pcycle_pause(10000);
            }
            printf("done: Round %d: %d vs %d\n", i, ints_handled,
                   ints_expected);
        }
    }

    __atomic_fetch_add(&thread_exit_flag, 1, __ATOMIC_SEQ_CST);
    puts("waiting for threads...");
    for (int i = 0; i < ROUNDS; i++) {
        for (int j = 0; j < MAX_INT_NUM; j++) {
            pcycle_pause(10000);
            swi(1 << j);
        }
    }
    while (some_threads_running()) {
        pcycle_pause(2500000);
        swi(ALL_INTERRUPTS_MASK);
    }

    puts("ints exhausted...");
    thread_join(((1 << COMPUTE_THREADS) - 1) << 1);

    puts("PASS");
    return 0;
}
