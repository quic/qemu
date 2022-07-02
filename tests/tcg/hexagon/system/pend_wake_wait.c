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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hexagon_standalone.h"
#include "interrupts.h"

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

static volatile int threads_started = 0;
static const int ROUNDS = 10;
static void thread_func(void *arg)
{
    int taskId = (int) arg;
    set_thread_imask(0);
    threads_started++;
    for (int i = 0; i < ROUNDS; i++) {
        k0lock();
        pause();
        k0unlock();
        printf("thread[%d] round %d\n", taskId, i);
    }
    printf("thread exiting %d\n", taskId);
}

static volatile int ints_handled = 0;
static void interrupt_handler(int intno)
{
    k0lock();
    isr_work();
    k0unlock();
    ints_handled++;
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
    for (int i = 1; i < COMPUTE_THREADS + 1; i++) {
        uint32_t th_bit = i << 1;
        bool th_w = (W & th_bit) != 0;
        bool th_e = (E & th_bit) != 0;
        printf("thread %d, w: %d, e: %d\n", i, th_w, th_e);
        if (th_w || th_e) {
            some_running = true;
        }
    }
    printf("some running: %d\n", some_running);
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
        thread_create((void *)thread_func,
            &stack[i][ARRAY_SIZE(stack[i]) - 8], i + 1, (void *)i);
    }

    while (threads_started < COMPUTE_THREADS) {
        printf("Threads started: %d\n", threads_started);
        pause();
    }
    printf("\tnow threads started: %d\n", threads_started);

    for (int i = 0; i < ROUNDS; i++) {
        int ints_expected = ints_handled + 1;
        printf("Round %d, doing work for each int:\n", i);
        for (int j = 0; j < MAX_INT_NUM; j++) {
            work();
            while (ints_handled < ints_expected) {
                swi(1 << j);
                pause();
            }
        }
    }
    puts("waiting for threads...");
    for (int i = 0; i < ROUNDS; i++) {
        for (int j = 0; j < MAX_INT_NUM; j++) {
            swi(1 << j);
            pause();
        }
    }
    while (some_threads_running()) {
        swi(ALL_INTERRUPTS_MASK);
        pause();
    }

    puts("ints exhausted...");
    thread_join(((1 << COMPUTE_THREADS) - 1) << 1);

    puts("PASS");
    return 0;
}
