#include <stdlib.h>
#include <stdio.h>
#include <hexagon_standalone.h>

/* This and other volatiles are for dealing with mmio registers */
typedef volatile unsigned int vu32;
static uint32_t HWTID[6];

static uint32_t get_cfgbase()
{
  uint32_t R;
  asm volatile ("%0=cfgbase;"
    : "=r"(R));
    return R;
}
static uint32_t _rdcfg(uint32_t cfgbase, uint32_t offset)
{
  asm volatile ("%[cfgbase]=asl(%[cfgbase], #5)\n\t"
                "%[offset]=memw_phys(%[offset], %[cfgbase])"
    : [cfgbase] "+r" (cfgbase), [offset] "+r" (offset)
    :
    : );
  return offset;
}

vu32 g_l2vic_base;
vu32 *FAST_INTF_VA = (vu32 *)0x888e0000;
#define FIRST_IRQ (36)
#define STACK_SIZE (16384)
#define ARCH_REV __HEXAGON_ARCH__

static int IRQ[4] = { FIRST_IRQ, FIRST_IRQ + 1, FIRST_IRQ + 2, FIRST_IRQ + 3 };

/* Using volatile for MMIO writes */
#define L2VIC_INT_ENABLE(n) \
    ((vu32 *) ((g_l2vic_base) + 0x100 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_ENABLE_CLEAR(n) \
   ((vu32 *)((g_l2vic_base) + 0x180 + 4 * (n / 32)))
#define L2VIC_INT_ENABLE_SET(n) \
   ((vu32 *) ((g_l2vic_base) + 0x200 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_CLEAR(n) \
   ((vu32 *)((g_l2vic_base) + 0x400 + 4 * (n / 32)))
#define L2VIC_SOFT_INT_SET(n) \
   ((vu32 *) ((g_l2vic_base) + 0x480 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_TYPE(n) \
   ((vu32 *) ((g_l2vic_base) + 0x280 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_GRP(n) ((vu32 *)((g_l2vic_base) + 0x600 + 4 * n))

static int count[4] = { 0, 0, 0, 0 };

#define asm_wait() __asm__ __volatile__("    wait(r0)\n" : : : "r0")

#define asm_resume()                                                           \
    __asm__ __volatile__("    r0 = #7\n"                                       \
             "    resume(r0)\n"                                    \
             :                                                     \
             :                                                     \
             : "r0");

unsigned long long my_read_pcycles(void)
{
    uint32_t lo, hi;
    uint64_t pcyc;

    __asm__ __volatile__("%0 = pcyclelo\n"
             "%1 = pcyclehi\n"
             : "=r"(lo), "=r"(hi));
    pcyc = (((uint64_t)hi) << 32) + lo;
    return pcyc;
}

static uint64_t t1, t2;

void update_l2vic(uint32_t irq)
{
    *FAST_INTF_VA = (0 << 16) + irq; /* fast re-enable */
}

void init_l2vic()
{
    int i;

    for (i = 0; i < 4; i++) {
        uint32_t irq = IRQ[i];
        uint32_t irq_bit = (1 << (irq % 32));

        *L2VIC_INT_ENABLE_CLEAR(irq) = irq_bit;
        *L2VIC_INT_TYPE(irq) |= irq_bit;
        *L2VIC_INT_ENABLE_SET(irq) = irq_bit;
    }
}

void intr_handler(int irq)
{
    uint32_t pcyclo, htid, i, j, vid;
    static uint32_t count[4] = { 0, 0, 0, 0 };

    __asm__ __volatile__("%0 = pcyclelo\n"
             "%1 = htid\n"
             : "=r"(pcyclo), "=r"(htid));
    HWTID[htid]++;


    vid = *((uint32_t *)(g_l2vic_base + (irq - 2) * 4));
    printf(" ### INT %d on HT %lu l2vid %lu\n", irq, htid, vid);
    fflush(stdout);
    printf("     @ 0x%lx\n", pcyclo);
    fflush(stdout);
    if ((vid - FIRST_IRQ) > 3) {
        printf(" unexpected L2IRQ number %lu\n", vid);
        fflush(stdout);
        exit(-1);
    }
    count[vid - FIRST_IRQ]++;
    j = 0;
    for (i = 0; i <= 3; i++) {
        if (count[i] >= 4) {
            j++;
        }
    }
    if (j == 4) {
        printf("We have done enough\n");
    }
    fflush(stdout);
    update_l2vic(vid);
}

/* assign l2irq to specific L1INT */
void assign_l2irq_to_l1int(int *irq_array, int how_many)
{
    int i, irq, word_id, set_id;

    for (i = 0; i < how_many; i++) {
        irq = irq_array[i];
        word_id = irq / 32;
        set_id = irq % 32;
        *L2VIC_INT_GRP(word_id) |= (8 + i) << (set_id * 4);
    }
}

void enable_core_interrupt()
{
    uint32_t set;

    register_interrupt(2, intr_handler); /* T1 */
    register_interrupt(3, intr_handler); /* T2 */
    register_interrupt(4, intr_handler); /* T3 */
    register_interrupt(5, intr_handler); /* T4 */

    set = (2 << 16) + 0xd; /* only T1 to serve INT#2 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");
    set = (3 << 16) + 0xb; /* only T2 to serve INT#3 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");
    set = (4 << 16) + 0x7; /* T3 serves INT#4 and INT#5 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");
    set = (5 << 16) + 0x1f; /* T4 serves INT#4 and INT#5 */
    __asm__ __volatile__("r0 = %0\n"
             "iassignw(r0)\n"
             :
             : "r"(set)
             : "r0");

    assign_l2irq_to_l1int(IRQ, 4);
}

int waste_some_time(int count)
{
    /* Don't want the optimizer to stop this */
    volatile int i, j, k, res = 0;
    for (i = 0; i < count; i++) {
        for (j = 0; j < 10; j++) {
            for (k = 0; k < 100; k++) {
                res += j;
            }
        }
        res += k;
    }
    return res;
}

char __attribute__((aligned(16))) stack1[STACK_SIZE];
char __attribute__((aligned(16))) stack2[STACK_SIZE];
char __attribute__((aligned(16))) stack3[STACK_SIZE];

void thread1()
{
    uint32_t pcyclo;
    for (;;) {
        __asm__ __volatile__("%0 = pcyclelo" : "=r"(pcyclo));
    }
    return;
}

void thread2()
{
    uint32_t pcyclo;
    for (;;) {
        __asm__ __volatile__("%0 = pcyclelo" : "=r"(pcyclo));
    }
    return;
}

void thread3()
{
    uint32_t pcyclo;
    for (;;) {
        __asm__ __volatile__("%0 = pcyclelo" : "=r"(pcyclo));
    }
    return;
}

int main()
{
    int i, j;
    unsigned int base;
    unsigned int va;
    unsigned long long int pa;
    thread_create(thread3, &stack3[STACK_SIZE - 16], 3, NULL);
    thread_create(thread2, &stack2[STACK_SIZE - 16], 2, NULL);
    thread_create(thread1, &stack1[STACK_SIZE - 16], 1, NULL);

    /* setup the fastl2vic interface and setup an indirect mapping */
    uint32_t cfgbase = get_cfgbase();
    unsigned int fastl2vic = _rdcfg(cfgbase, 0x28);   /* Fastl2vic */
    add_translation_extended(3, (void *)FAST_INTF_VA, fastl2vic << 16, 16, 7, 4, 0, 0, 3);

    g_l2vic_base = (_rdcfg(cfgbase, 0x8) << 16) + 0x10000;


    enable_core_interrupt();
    init_l2vic();
    waste_some_time(5);

    for (i = 0; i < 6; i++) { /* disable(1)-enable(4)-disable(1) */
        printf(" iteration S0 %d @ 0x%llx\n", i, my_read_pcycles());
        fflush(stdout);

        /* clear off all pending interrupts */
        for (j = 0; j < 4; j++) {
            unsigned int bit = 1 << (IRQ[j] % 32);
            *L2VIC_INT_CLEAR(IRQ[j]) = bit;
        }
        waste_some_time(5);

        /* set up */
        printf(" iteration S1 %d @ 0x%llx\n", i, my_read_pcycles());
        fflush(stdout);
        if ((i == 0) || (i == 5)) {
            for (j = 0; j < 4; j++) {
                *FAST_INTF_VA = (1 << 16) + IRQ[j]; /* disable them */
            }
        }
        if (i == 1) {
            for (j = 0; j < 4; j++) {
                *FAST_INTF_VA = (0 << 16) + IRQ[j]; /* enable them */
            }
        }
        waste_some_time(5);

        /* action */
        printf(" iteration S2 %d @ 0x%llx\n", i, my_read_pcycles());
        fflush(stdout);

        for (j = 0; j < 4; j++) {
            *FAST_INTF_VA = (2 << 16) + IRQ[j];
        }
        waste_some_time(5);
    }
    waste_some_time(5);

    /* Each of the selected HW threads should get at least one interrupt */
    for (i = 1; i < 3; i++) {
        if (HWTID[i] < 1) {
            printf("FAIL: HWTID[%d] = %ld\n", i, HWTID[i]);
            return 1;
        }
    }
    printf("PASS\n");
    return 0;
}
