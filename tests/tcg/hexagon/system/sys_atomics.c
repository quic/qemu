#include <math.h>
#include <time.h>
#include <sys/types.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <q6standalone.h>

#define LOOP_CNT 1000

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

#define asm_resume()    \
    __asm__ __volatile__ (  \
    "    r0 = %0\n"   \
    "    resume(r0)\n"    \
    : : "r" (mask) : "r0"      \
    );

/* Using volatile because we are testing atomics */
static inline int atomic_inc32(volatile int *x)
{
    int old, dummy;
    __asm__ __volatile__(
        "1: %0 = memw_locked(%2)\n\t"
        "   %1 = add(%0, #1)\n\t"
        "   memw_locked(%2, p0) = %1\n\t"
        "   if (!p0) jump 1b\n\t"
        : "=&r"(old), "=&r"(dummy)
        : "r"(x)
        : "p0", "memory");
    return old;
}

/* Using volatile because we are testing atomics */
static inline long long atomic_inc64(volatile long long *x)
{
    long long old, dummy;
    __asm__ __volatile__(
        "1: %0 = memd_locked(%2)\n\t"
        "   %1 = #1\n\t"
        "   %1 = add(%0, %1)\n\t"
        "   memd_locked(%2, p0) = %1\n\t"
        "   if (!p0) jump 1b\n\t"
        : "=&r"(old), "=&r"(dummy)
        : "r"(x)
        : "p0", "memory");
    return old;
}

/* Using volatile because we are testing atomics */
static inline int atomic_dec32(volatile int *x)
{
    int old, dummy;
    __asm__ __volatile__(
        "1: %0 = memw_locked(%2)\n\t"
        "   %1 = add(%0, #-1)\n\t"
        "   memw_locked(%2, p0) = %1\n\t"
        "   if (!p0) jump 1b\n\t"
        : "=&r"(old), "=&r"(dummy)
        : "r"(x)
        : "p0", "memory");
    return old;
}

/* Using volatile because we are testing atomics */
static inline long long atomic_dec64(volatile long long *x)
{
    long long old, dummy;
    __asm__ __volatile__(
        "1: %0 = memd_locked(%2)\n\t"
        "   %1 = #-1\n\t"
        "   %1 = add(%0, %1)\n\t"
        "   memd_locked(%2, p0) = %1\n\t"
        "   if (!p0) jump 1b\n\t"
        : "=&r"(old), "=&r"(dummy)
        : "r"(x)
        : "p0", "memory");
    return old;
}

int err;
/* Using volatile because we are testing atomics */
volatile int tick32 = 1;
/* Using volatile because we are testing atomics */
volatile long long tick64 = 1;

void my_thread(void *y)
{
    asm_wait();

    int i;
    int id = *(int *)y;
    for (i = 0; i < LOOP_CNT; i++) {
        if (id == 1) {
            atomic_dec32(&tick32);
            atomic_inc64(&tick64);
        } else {
            atomic_inc32(&tick32);
            atomic_dec64(&tick64);
        }
    }
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

long long stack1[1024];
long long stack2[1024];
int id1 = 1;
int id2 = 2;

int main()
{
    puts("Hexagon atomics test");

    /* thread-create takes a HW thread number */
    thread_create(my_thread, &stack1[1023], 1, (void *)&id1);
    thread_create(my_thread, &stack2[1023], 2, (void *)&id2);

    unsigned modectl = 0;
    asm_modectl();
    wait_for_threads();

    unsigned int mask = 0x6;
    asm_resume();
    asm_modectl();

    /* join takes a mask not a thread number */
    thread_join(1 << 1);
    thread_join(1 << 2);

    if (tick32 != 1) {
        printf("ERROR: tick32 %d != 1\n", tick32);
        err++;
    }
    if (tick64 != 1) {
        printf("ERROR: tick64 %lld != 1\n", tick64);
        err++;
    }
    puts(err ? "FAIL" : "PASS");
    return err;
}

