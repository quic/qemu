#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <hexagon_standalone.h>

#define ERR 0.1
#define LOW_ERR (1 - ERR / 2)
#define HIG_ERR (1 + ERR / 2)

#define NUM_PMU_CTRS 8
static uint32_t get_pmu_counter(int index);
static uint32_t get_gpmu_counter(int index);

static int err;

enum regtype {
    SREG,
    GREG,
};

static inline void __check_val_range(uint32_t val,
                                     int regnum, enum regtype type,
                                     uint32_t lo, uint32_t hi,
                                     int line)
{
    if (val < lo || val > hi) {
        printf("ERROR at line %d: %s counter %u outside"
               " [%"PRIu32", %"PRIu32"] range (%"PRIu32")\n",
               line, (type == GREG ? "greg" : "sys"), regnum, lo, hi, val);
        err = 1;
    }
}

#define check_range(regnum, regtype, lo, hi) \
    __check_val_range(regtype == GREG ? \
                      get_gpmu_counter(regnum) : \
                      get_pmu_counter(regnum), \
                      regnum, regtype, lo, hi, __LINE__)

#define check_val_range(val, regnum, regtype, lo, hi) \
    __check_val_range(val, regnum, regtype, lo, hi, __LINE__)

static void pmu_config(uint32_t counter_id, uint32_t event)
{
    uint32_t off = (counter_id % 4) * 8;
    if (counter_id < 4) {
        asm volatile(
            "r0 = pmuevtcfg\n"
            "r2 = %0\n" /*off*/
            "r3 = #8\n" /*width*/
            "r0 = insert(%1, r3:2)\n"
            "pmuevtcfg = r0\n"
            :
            : "r"(off), "r"(event)
            : "r0", "r2", "r3" );
    } else {
        asm volatile(
            "r0 = pmuevtcfg1\n"
            "r2 = %0\n" /*off*/
            "r3 = #8\n" /*width*/
            "r0 = insert(%1, r3:2)\n"
            "pmuevtcfg1 = r0\n"
            :
            : "r"(off), "r"(event)
            : "r0", "r2", "r3" );
    }
}

static void pmu_set_counters(uint32_t val)
{
    asm volatile(
        "pmucnt0 = %0\n"
        "pmucnt1 = %0\n"
        "pmucnt2 = %0\n"
        "pmucnt3 = %0\n"
        "pmucnt4 = %0\n"
        "pmucnt5 = %0\n"
        "pmucnt6 = %0\n"
        "pmucnt7 = %0\n"
        : : "r"(val));
}

#define PM_SYSCFG_BIT 9
static inline void pmu_start(void)
{
    asm volatile(
        "r1 = syscfg\n"
        "r1 = setbit(r1, #%0)\n"
        "syscfg = r1\n"
        "isync\n"
        : : "i"(PM_SYSCFG_BIT) : "r1");
}

static inline void pmu_stop(void)
{
    asm volatile(
        "r1 = syscfg\n"
        "r1 = clrbit(r1, #%0)\n"
        "syscfg = r1\n"
        "isync\n"
        : : "i"(PM_SYSCFG_BIT) : "r1");
}

static uint32_t get_pmu_counter(int index)
{
    uint32_t counter;
    switch (index) {
    case 0:
        asm volatile ("%0 = pmucnt0\n" : "=r"(counter));
        break;
    case 1:
        asm volatile ("%0 = pmucnt1\n" : "=r"(counter));
        break;
    case 2:
        asm volatile ("%0 = pmucnt2\n" : "=r"(counter));
        break;
    case 3:
        asm volatile ("%0 = pmucnt3\n" : "=r"(counter));
        break;
    case 4:
        asm volatile ("%0 = pmucnt4\n" : "=r"(counter));
        break;
    case 5:
        asm volatile ("%0 = pmucnt5\n" : "=r"(counter));
        break;
    case 6:
        asm volatile ("%0 = pmucnt6\n" : "=r"(counter));
        break;
    case 7:
        asm volatile ("%0 = pmucnt7\n" : "=r"(counter));
        break;
    default:
        printf("ERROR at line %d: invalid counter index %d\n", __LINE__, index);
        abort();
    }
    return counter;
}

#define PE_SSR_BIT 24
static void config_gpmu(bool enable)
{
    if (enable) {
        asm volatile(
                "r0 = ssr\n"
                "r0 = setbit(r0, #%0)\n"
                "ssr = r0"
                : : "i"(PE_SSR_BIT) : "r0");
    } else {
        asm volatile(
                "r0 = ssr\n"
                "r0 = clrbit(r0, #0)\n"
                "ssr = r0"
                : : "i"(PE_SSR_BIT) : "r0");
    }
}

static uint32_t get_gpmu_counter(int index)
{
    uint32_t counter;
    switch (index) {
    case 0:
        asm volatile ("%0 = gpmucnt0\n" : "=r"(counter));
        break;
    case 1:
        asm volatile ("%0 = gpmucnt1\n" : "=r"(counter));
        break;
    case 2:
        asm volatile ("%0 = gpmucnt2\n" : "=r"(counter));
        break;
    case 3:
        asm volatile ("%0 = gpmucnt3\n" : "=r"(counter));
        break;
    case 4:
        asm volatile ("%0 = gpmucnt4\n" : "=r"(counter));
        break;
    case 5:
        asm volatile ("%0 = gpmucnt5\n" : "=r"(counter));
        break;
    case 6:
        asm volatile ("%0 = gpmucnt6\n" : "=r"(counter));
        break;
    case 7:
        asm volatile ("%0 = gpmucnt7\n" : "=r"(counter));
        break;
    default:
        printf("ERROR at line %d: invalid counter index %d\n", __LINE__, index);
        abort();
    }
    return counter;
}

static void pmu_reset(void)
{
    pmu_stop();
    pmu_set_counters(0);
    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        pmu_config(i, 0);
    }
}

#define COMMITTED_PKT_T0 12
#define COMMITTED_PKT_T1 13
#define COMMITTED_PKT_T2 14
#define COMMITTED_PKT_T3 15
#define COMMITTED_PKT_T4 16
#define COMMITTED_PKT_T5 17
#define COMMITTED_PKT_T6 21
#define COMMITTED_PKT_T7 22

#define NUM_THREADS 8
#define STACK_SIZE 0x8000
char __attribute__((aligned(16))) stack[NUM_THREADS - 1][STACK_SIZE];

#define BASE_WORK_COUNT 205 /* Expected num of counted insn per work(0) call */
static void work(void *_id)
{
    int id = (int)_id;
    pmu_start();
    for (int i = 0; i < 100 * (id + 1); i++) {
        asm volatile("nop");
    }
    pmu_stop();
}

static void test_config_with_pmu_enabled(int start_offset)
{
    pmu_reset();

    pmu_set_counters(start_offset);

    pmu_start();
    for (int i = 0; i < 800; i++) {
        asm volatile("nop");
    }
    pmu_config(0, COMMITTED_PKT_T0);
    pmu_stop();

    check_range(0, SREG, start_offset, start_offset + 15);
}

static void test_threaded_pkt_count(enum regtype type, bool enable_gpmu)
{
    pmu_reset();

    pmu_config(1, COMMITTED_PKT_T1);
    pmu_config(2, COMMITTED_PKT_T2);
    pmu_config(3, COMMITTED_PKT_T3);
    pmu_config(4, COMMITTED_PKT_T4);
    pmu_config(5, COMMITTED_PKT_T5);
    pmu_config(6, COMMITTED_PKT_T6);
    pmu_config(7, COMMITTED_PKT_T7);

    for (int i = 1; i < NUM_THREADS; i++) {
        thread_create(work, (void *)&stack[i - 1][STACK_SIZE - 16], i, (void *)i);
        thread_join(1 << i);
    }
    pmu_config(0, COMMITTED_PKT_T0);
    work(0);

    config_gpmu(enable_gpmu);
    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        if (type == GREG && !enable_gpmu) {
            check_range(i, type, 0, 0);
        } else {
            check_range(i, type, BASE_WORK_COUNT * (i + 1) * LOW_ERR,
                                 BASE_WORK_COUNT * (i + 1) * HIG_ERR);
        }
    }
}

static void test_paired_access(enum regtype type, bool enable_gpmu)
{
    uint32_t v[NUM_PMU_CTRS];
    pmu_reset();

    pmu_config(1, COMMITTED_PKT_T1);
    pmu_config(2, COMMITTED_PKT_T2);
    pmu_config(3, COMMITTED_PKT_T3);
    pmu_config(4, COMMITTED_PKT_T4);
    pmu_config(5, COMMITTED_PKT_T5);
    pmu_config(6, COMMITTED_PKT_T6);
    pmu_config(7, COMMITTED_PKT_T7);

    /*
     * Set pmucnt0, 1, ... respectively to 0, 1000, 0, 1000, ...
     * Note: we don't want to condition the assignment on `type` as
     * guest writes should be ignored.
     */
    asm volatile(
        "r0 = #0\n"
        "r1 = #1000\n"
        "s49:48 = r1:0\n"
        "s51:50 = r1:0\n"
        "s45:44 = r1:0\n"
        "s47:46 = r1:0\n"
        : : : "r0", "r1");

    for (int i = 1; i < NUM_THREADS; i++) {
        thread_create(work, (void *)&stack[i - 1][STACK_SIZE - 16], i, (void *)i);
        thread_join(1 << i);
    }
    pmu_config(0, COMMITTED_PKT_T0);
    work(0);

    if (type == GREG) {
        config_gpmu(enable_gpmu);
        asm volatile(
            "r1:0 = g27:26\n"
            "r3:2 = g29:28\n"
            "r5:4 = g17:16\n"
            "r7:6 = g19:18\n"
            "%0 = r0\n"
            "%1 = r1\n"
            "%2 = r2\n"
            "%3 = r3\n"
            "%4 = r4\n"
            "%5 = r5\n"
            "%6 = r6\n"
            "%7 = r7\n"
            : "=r"(v[0]), "=r"(v[1]), "=r"(v[2]), "=r"(v[3]), "=r"(v[4]),
              "=r"(v[5]), "=r"(v[6]), "=r"(v[7])
            :
            : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7");
    } else {
        asm volatile(
            "r1:0 = s49:48\n"
            "r3:2 = s51:50\n"
            "r5:4 = s45:44\n"
            "r7:6 = s47:46\n"
            "%0 = r0\n"
            "%1 = r1\n"
            "%2 = r2\n"
            "%3 = r3\n"
            "%4 = r4\n"
            "%5 = r5\n"
            "%6 = r6\n"
            "%7 = r7\n"
            : "=r"(v[0]), "=r"(v[1]), "=r"(v[2]), "=r"(v[3]), "=r"(v[4]),
              "=r"(v[5]), "=r"(v[6]), "=r"(v[7])
            :
            : "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7");
    }

    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        if (type == GREG && !enable_gpmu) {
            check_range(i, type, 0, 0);
        } else {
            int off = i % 2 ? 1000 : 0;
            check_val_range(v[i], i, type,
                            off + BASE_WORK_COUNT * (i + 1) * LOW_ERR,
                            off + BASE_WORK_COUNT * (i + 1) * HIG_ERR);
        }
    }
}

static void test_gpmucnt(void)
{
    /* gpmucnt should be 0 if SSR:PE is 0 */
    pmu_reset();
    test_threaded_pkt_count(GREG, false);
    pmu_reset();
    test_paired_access(GREG, false);

    /* gpmucnt should alias the sys pmucnts if SSR:PE is 1 */
    pmu_reset();
    test_threaded_pkt_count(GREG, true);
    pmu_reset();
    test_paired_access(GREG, true);

    /* gpmucnt writes should be ignored. */
    config_gpmu(1);
    pmu_reset();
    asm volatile(
        "r0 = #2\n"
        "gpmucnt0 = r0\n"
        "gpmucnt1 = r0\n"
        "gpmucnt2 = r0\n"
        "gpmucnt3 = r0\n"
        "gpmucnt4 = r0\n"
        "gpmucnt5 = r0\n"
        "gpmucnt6 = r0\n"
        "gpmucnt7 = r0\n"
        : : : "r0");
    for (int i = 0; i < NUM_PMU_CTRS; i++) {
        check_range(i, GREG, 0, 0);
    }
}

static void config_thread(void *_)
{
    pmu_set_counters(100);
    pmu_config(0, COMMITTED_PKT_T0);
    pmu_start();
}

static void test_config_from_another_thread(void)
{
    const int tid = 1;
    pmu_reset();
    thread_create(config_thread, (void *)&stack[tid - 1][STACK_SIZE - 16],
                  tid, NULL);
    thread_join(1 << tid);
    pmu_stop();
    check_range(0, SREG, 100, 1000); /* We just want to check >= 100, really */
}

int main()
{
    test_config_with_pmu_enabled(0);
    test_config_with_pmu_enabled(100);
    test_threaded_pkt_count(SREG, false);
    test_paired_access(SREG, false);
    test_gpmucnt();
    test_config_from_another_thread();

    puts(err ? "FAIL" : "PASS");
    return err;
}
