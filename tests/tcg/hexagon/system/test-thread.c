/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <q6standalone.h>
#include <string.h>

#define THCNT 8
static int thid[THCNT] = { 0 };
static int expect[THCNT] = { 0, 1, 2, 3, 4, 5, 6, 7 };

static int Mx;
volatile int thcnt = 0;
void thread(void *y)
{
    int i;
    unsigned int id;
    unsigned int ex;
    id = thread_get_tnum();
    assert(id < (THCNT));
    while (1) {
        if (trylockMutex(&Mx))
            break;
        __asm__ volatile("pause (#200)\n");
    }
    thcnt++;
    thid[id] = id;
    unlockMutex(&Mx);
}

#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) stack1[STACK_SIZE];
char __attribute__((aligned(16))) stack2[STACK_SIZE];
char __attribute__((aligned(16))) stack3[STACK_SIZE];
char __attribute__((aligned(16))) stack4[STACK_SIZE];
char __attribute__((aligned(16))) stack5[STACK_SIZE];
char __attribute__((aligned(16))) stack6[STACK_SIZE];
char __attribute__((aligned(16))) stack7[STACK_SIZE];

int main()
{
    unsigned int id;
    lockMutex(&Mx);
    thread_create(thread, (void *)&stack1[STACK_SIZE - 16], 1, (void *)&thcnt);
    thread_create(thread, (void *)&stack2[STACK_SIZE - 16], 2, (void *)&thcnt);
    thread_create(thread, (void *)&stack3[STACK_SIZE - 16], 3, (void *)&thcnt);
    thread_create(thread, (void *)&stack4[STACK_SIZE - 16], 4, (void *)&thcnt);
    thread_create(thread, (void *)&stack5[STACK_SIZE - 16], 5, (void *)&thcnt);
    thread_create(thread, (void *)&stack6[STACK_SIZE - 16], 6, (void *)&thcnt);
    thread_create(thread, (void *)&stack7[STACK_SIZE - 16], 7, (void *)&thcnt);
    unlockMutex(&Mx);
    while (thcnt < (THCNT - 1))
        __asm__ volatile("pause (#200)\n");

    thread_join(1 << 1);
    thread_join(1 << 2);
    thread_join(1 << 3);
    thread_join(1 << 4);
    thread_join(1 << 5);
    thread_join(1 << 6);
    thread_join(1 << 7);

    if (memcmp(thid, expect, sizeof(expect))) {
        printf("FAIL\n");
        for (int I = 0; I < THCNT; I++)
            printf("EXPECT: expect[%d] = %d, GOT: thid[%d] = %d\n", I,
                   expect[I], I, thid[I]);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
