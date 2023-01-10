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

/*
 * This program tests the Hexagon start instruction.
 * We test the instruction by intstalling our own reset handler.
 * Inside this handler,
 *     set a bit in reset_mask to indicate the thread was reset
 *     log the cause field in SSR register
 *     stop the thread
 *
 * The start instruction should reset all threads in the argument
 * except the current thread.  We check that the cause field in SSR
 * is preserved in the current thread and reset in the others in the
 * mask.
 */

#include <stdlib.h>
#include <stdio.h>
#include <q6standalone.h>
#include "mmu.h"

int err;

/* Mark volatile because it is modified by multiple threads */
static volatile uint32_t reset_mask;

#define NUM_THREADS           4
#define DUMMY_CAUSE           0x13
/* Mark volatile because it is modified by multiple threads */
static volatile uint32_t ssr_cause_log[NUM_THREADS];

#define get_htid(htid) \
    asm("%0 = htid\n\t" : "=r"(htid))

#define get_ssr(ssr) \
    asm volatile ("%0 = ssr\n\t" : "=r"(ssr))

#define set_ssr(ssr) \
    asm volatile ("ssr = %0\n\t" : : "r"(ssr))

#define set_ssr_cause(cause) \
    do { \
        uint32_t ssr; \
        get_ssr(ssr); \
        ssr &= ~0xff; \
        ssr |= ((cause) & 0xff); \
        set_ssr(ssr); \
    } while (0)


#define read_pcycle_reg_pair(pcycle) \
    asm volatile("syncht\n\t"        \
                 "%0 = pcycle\n\t"   \
                 : "=r"(pcycle))

void my_reset_helper(void)
{
    uint32_t htid;
    uint32_t ssr;

    /* Set the htid bit in the mask */
    get_htid(htid);
    reset_mask |= (1 << htid);

    /* Log the SSR cause field */
    get_ssr(ssr);
    ssr_cause_log[htid] = ssr & 0xff;

    /* Stop this thread */
    asm volatile("stop(r0)\n\t");
}

#define MY_EVENT_HANDLE_RESET \
void my_event_handle_reset(void) \
{ \
    asm volatile("jump my_reset_helper\n\t"); \
}

/* Set up the event handlers */
MY_EVENT_HANDLE_RESET

DEFAULT_EVENT_HANDLE(my_event_handle_error,       HANDLE_ERROR_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)

void pcycle_pause(uint64_t pcycle_wait)
{
    uint64_t pcycle, pcycle_start;

    read_pcycle_reg_pair(pcycle_start);
    do {
        asm volatile("pause(#1)\n\t");
        read_pcycle_reg_pair(pcycle);
    } while (pcycle < pcycle_start + pcycle_wait);
}

static void test_start(uint32_t mask)
{
    uint32_t htid;
    uint32_t expect;
    uint32_t ssr;

    get_htid(htid);
    expect = ~(1 << htid);
    reset_mask = 0;
    for (int i = 0; i < NUM_THREADS; i++) {
        ssr_cause_log[i] = DUMMY_CAUSE;
    }
    set_ssr_cause(0x15);
    asm volatile("start(%0)\n\t" :: "r"(mask));
    get_ssr(ssr);
    check(ssr & 0xff, 0x15);
    pcycle_pause(100);
    check(reset_mask, (mask & expect));
    for (int i = 0; i < NUM_THREADS; i++) {
        if (i != htid) {
            if (mask & (1 << i)) {
                check(ssr_cause_log[i], 0);
            } else {
                check(ssr_cause_log[i], DUMMY_CAUSE);
            }
        }
    }
}

#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) thread_stack[STACK_SIZE];

static void thread_func(void *x)
{
    test_start(0x3);
    test_start(0x7);
    test_start(0xf);
}

int main()
{
    puts("Hexagon start instruction test");

    install_my_event_vectors();

    /* Run the tests on thread 0 */
    test_start(0x3);
    test_start(0x7);
    test_start(0xf);

    /* Run the tests on other threads */
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_create(thread_func, (void *)&thread_stack[STACK_SIZE - 16],
                      i, NULL);
        thread_join(1 << i);
    }
    puts(err ? "FAIL" : "PASS");
    return err;
}
