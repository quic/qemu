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

#ifndef THREAD_COMMON_H
#define THREAD_COMMON_H

#include <stdint.h>
#define FORCE_INLINE __attribute__((always_inline))

/* MUST be inlined for start.c's reset handler */
static inline FORCE_INLINE uint32_t get_htid(void)
{
    uint32_t htid;
    asm volatile("%0 = htid\n\t" : "=r"(htid));
    return htid;
}

/* MUST be inlined for start.c's reset handler */
static inline FORCE_INLINE uint32_t remove_myself(uint32_t mask)
{
    return mask & ~(1 << get_htid());
}

#define THREAD_SEMAPHORE_OFF 0
#define THREAD_SEMAPHORE_ON_WAIT 1
#define THREAD_SEMAPHORE_GO 2
void set_semaphore_state(uint32_t mask, int state);

void create_waiting_thread(void (*func)(void *), void *sp, int tid, void *param);
void start_waiting_threads(uint32_t mask);
void thread_create_blocked(void (*func)(void *), void *sp, int tid, void *param);
void thread_run_blocked(void (*func)(void *), void *sp, int tid, void *param);

extern volatile int thread_semaphore[32]; /* volatile: changed by multiple threads */

/* MUST be inlined for start.c's reset handler */
static inline FORCE_INLINE void wait_on_semaphore(void)
{
    uint32_t htid = get_htid();
    thread_semaphore[htid] = THREAD_SEMAPHORE_ON_WAIT;
    while (thread_semaphore[htid] != THREAD_SEMAPHORE_GO) {
        asm volatile("pause(#1)\n");
    }
}

#endif
