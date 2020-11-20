#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <q6standalone.h>
#include <string.h>
#include <stdbool.h>

static const int LOOP_CNT = 10000;
static const int EXPECTED_TICK_COUNT = 60001;

#define asm_modectl() \
     __asm__ __volatile__ ( \
         "%0 = MODECTL" \
         : "=r" (modectl) \
    );

#define asm_wait()    \
    __asm__ __volatile__ (  \
  "    wait(r0)\n"  \
  : : : "r0"    \
    )

static inline void asm_resume(int mask) {
    __asm__ __volatile__ (
    "    r0 = %0\n"
    "    resume(r0)\n"
    : : "r" (mask) : "r0"
    );
}


/* Using volatile because we are testing atomics */
static inline int atomic_inc32(volatile int *x, int increment)
{
    int old, dummy;
    int iters;
    __asm__ __volatile__(
        "   %2 = #0\n\t"
        "// initialize p0 to false:\n\t"
        "   p0 = cmp.eq(%2,#1)\n\t"
        "1: %0 = memw_locked(%3)\n\t"
        "   %1 = add(%0, %4)\n\t"
        "   %2 = add(%2, #1)\n\t"
        "   wait(r0)\n\t"
        "   memw_locked(%3, p0) = %1\n\t"
        "   if (!p0) jump 1b\n\t"
        : "=&r"(old), "=&r"(dummy), "=&r"(iters)
        : "r"(x), "r"(increment)
        : "p0", "r0", "memory");
    return (int) iters;
}

static inline int atomic_inc32_mismatch(volatile int *x, volatile int *y)
{
    /* This is to cover the case where a store-conditional
     * targets a virtual address other than the current
     * load-locked address.
     */
     int old, dummy, pred;
    __asm__ __volatile__(
        "   r0 = #0\n\t"
        "// initialize p0 to true:\n\t"
        "   p0 = cmp.eq(r0,#0)\n\t"
        "   r1 = #0\n\t"
        "1: %0 = memw_locked(%2)\n\t"
        "   memw_locked(%3, p0) = r1\n\t"
        "   if (p0) %1 = #1\n\t"
        "   if (!p0) %1 = #0\n\t"
        : "=&r"(old), "=&r"(pred)
        : "r"(x), "r"(y)
        : "p0", "r0", "r1", "memory");

    return pred;
}


/* Using volatile because we are testing atomics */
static inline long long atomic_inc64(volatile long long *x, long long increment)
{
    long long old, dummy;
    int iters;
    __asm__ __volatile__(
        "   %2 = #0\n\t"
        "// initialize p0 to false:\n\t"
        "   p0 = cmp.eq(%2,#1)\n\t"
        "1: %0 = memd_locked(%3)\n\t"
        "   %1 = add(%0, %4)\n\t"
        "   wait(r0)\n\t"
        "   %2 = add(%2, #1)\n\t"
        "   memd_locked(%3, p0) = %1\n\t"
        "   if (!p0) jump 1b\n\t"
        : "=&r"(old), "=&r"(dummy), "=&r"(iters)
        : "r"(x), "r"(increment)
        : "p0", "r0", "memory");
    return iters;
}

static inline int atomic_inc64_mismatch(volatile long long *x, volatile long long *y)
{
    /* This is to cover the case where a store-conditional
     * targets a virtual address other than the current
     * load-locked address.
     */
     long long old;
     int pred;
    __asm__ __volatile__(
        "   r0 = #0\n\t"
        "// initialize p0 to true:\n\t"
        "   p0 = cmp.eq(r0,#0)\n\t"
        "   r1:0 = #0\n\t"
        "1: %0 = memd_locked(%2)\n\t"
        "   memd_locked(%3, p0) = r1:0\n\t"
        "   if (p0) %1 = #1\n\t"
        "   if (!p0) %1 = #0\n\t"
        : "=&r"(old), "=&r"(pred)
        : "r"(x), "r"(y)
        : "p0", "r0", "r1", "memory");

    return pred;
}


int err;
/* Using volatile because we are testing atomics */
volatile int tick32 = 1;
/* Using volatile because we are testing atomics */
volatile long long tick64 = 1;
int done[3];

uint32_t sat_add(uint32_t a, uint32_t b) {
    uint32_t result = a + b;

    return (result < a || result < b)
       ? UINT32_MAX
       : result;
}

int thread_body(int id) {
    /* These counters are to track how many times an atomic_incZZ()
     * had to iterate because the store-conditional returned 'false'.
     */
    unsigned tick32_iters = 0;
    static const int EXPECTED_IGNORED = 99;
    int ignored32 = EXPECTED_IGNORED;
    long long ignored64 = EXPECTED_IGNORED;
    unsigned long long tick64_iters = 0;
    int i;
    for (i = 0; i < LOOP_CNT; i++) {
        int scale = id * (i % 5);
        tick32_iters = sat_add(tick32_iters, atomic_inc32(&tick32, 1 * scale));
        tick64_iters = sat_add(tick64_iters, atomic_inc64(&tick64, 1 * scale));

        /* For each iteration through the loop, we'll try to load-locked
         * 'ignored32/64' and store-conditional 'tick32/64'.  'ignored'
         * should never change, the predicate value from the store-cond
         * should never be true.  If tick32 is modified, it will be
         * detected by other assertions.
         */
        bool pred_true = atomic_inc32_mismatch(&tick32, &ignored32);
        assert(!pred_true);
        assert(ignored32 == EXPECTED_IGNORED);

        pred_true = atomic_inc64_mismatch(&tick64, &ignored64);
        assert(!pred_true);
        assert(ignored64 == EXPECTED_IGNORED);
    }

#if 0
    printf("tick32_iters[%d]: %u\n", id, tick32_iters);
    printf("tick64_iters[%d]: %llu\n", id, tick64_iters);
#endif
    return (tick32_iters - LOOP_CNT) + (tick64_iters - LOOP_CNT);
}

void my_thread(void *y)
{
    asm_wait();

    const int id = *(int *)y;
    const int contention = thread_body(id);

    /* All threads must encounter some contention otherwise
    ** the test is not valid.
    */
    assert(contention > 0);
    done[id] = 1;
}

void wait_for_threads()
{
    unsigned modectl;
    unsigned t1mode, t2mode;
    do {
        asm_modectl();
        t1mode = modectl & (0x1 << (16 + 1));  /* thread 1 wait bit */
        t2mode = modectl & (0x1 << (16 + 2));  /* thread 2 wait bit */
    } while (t1mode == 0 || t2mode == 0);
}

static inline void wait_for_threads_waiting_or_stopped(void)
{
    unsigned modectl;
    unsigned t1_waiting, t2_waiting;
    unsigned t1_enabled, t2_enabled;

    do {
        asm_modectl();
        t1_waiting = modectl & (0x1 << (16 + 1));  /* thread 1 wait bit */
        t2_waiting = modectl & (0x1 << (16 + 2));  /* thread 2 wait bit */
        t1_enabled = modectl & (0x1 << (0 + 1));  /* thread 1 enabled bit */
        t2_enabled = modectl & (0x1 << (0 + 2));  /* thread 2 enabled bit */
    } while ((!t1_waiting && t1_enabled) || (!t2_waiting && t2_enabled));
}

long long stack1[1024];
long long stack2[1024];
int id1 = 1;
int id2 = 2;

static bool not_done(void) {
    return !(done[id1] && done[id2]);
}

int main()
{
    puts("Hexagon atomics test");

    memset(done, 0, sizeof(done));

    /* thread-create takes a HW thread number */
    thread_create(my_thread, &stack1[1023], 1, (void *)&id1);
    thread_create(my_thread, &stack2[1023], 2, (void *)&id2);

    unsigned modectl = 0;
    asm_modectl();
    wait_for_threads();

    unsigned int mask = 0x6;
    asm_resume(mask);
    asm_modectl();

    int prev_tick32 = tick32;
    long long prev_tick64 = tick64;
    while (not_done()) {
        wait_for_threads_waiting_or_stopped();

        asm_resume(1 << 1);
        asm_resume(1 << 2);

        /* The tick values must be monotonically increasing. */
        assert(prev_tick32 <= tick32);
        assert(prev_tick64 <= tick64);

        prev_tick32 = tick32;
        prev_tick64 = tick64;
    }

    /* join takes a mask not a thread number */
    thread_join(1 << 1);
    thread_join(1 << 2);

    if (tick32 != EXPECTED_TICK_COUNT) {
        printf("ERROR: tick32 %d != %d\n", tick32, EXPECTED_TICK_COUNT);
        err++;
    }
    if (tick64 != EXPECTED_TICK_COUNT) {
        printf("ERROR: tick64 %lld != %d\n", tick64, EXPECTED_TICK_COUNT);
        err++;
    }
    puts(err ? "FAIL" : "PASS");
    return err;
}

