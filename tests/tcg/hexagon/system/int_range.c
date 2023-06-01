/*
 * Test the range of the l2vic interface.
 */

#include <assert.h>
#include <hexagon_standalone.h>

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

#define L2VIC_INT_ENABLE(b, n) \
    ((volatile unsigned int *) ((b) + 0x100 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_ENABLE_SET(b, n) \
   ((volatile unsigned int *) ((b) + 0x200 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_ENABLE_CLEAR(b, n) \
   ((volatile unsigned int *) ((b) + 0x180 + 4 * (n / 32))) /* device memory */
#define L2VIC_SOFT_INT_SET(b, n) \
   ((volatile unsigned int *) ((b) + 0x480 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_TYPE(b, n) \
   ((volatile unsigned int *) ((b) + 0x280 + 4 * (n / 32))) /* device memory */

volatile int pass;
int g_irq;
volatile uint32_t g_l2vic_base;


/*
 * When complete the irqlog will contain the value of the vid when the
 * handler was active.
 */
#define INTMAX 1024
#define LEFT_SET 666

int
main()
{
    unsigned int irq_bit;
    unsigned int left_set = 0;
    int ret = 0;

    /* setup the fastl2vic interface and setup an indirect mapping */
    uint32_t cfgbase = get_cfgbase();
    g_l2vic_base = (_rdcfg(cfgbase, 0x8) << 16) + 0x10000;

    /* Setup interrupts */
    for (int irq = 1; irq < INTMAX; irq++) {
        irq_bit  = (1 << (irq  % 32));
        *L2VIC_INT_ENABLE(g_l2vic_base, irq) |= irq_bit;
    }

    /* Read them all back and check */
    for (int irq = 1; irq < INTMAX; irq++) {
        if ((*L2VIC_INT_ENABLE(g_l2vic_base, irq) &
             (1 << (irq % 32))) != (1 << irq % 32)) {
            printf("%d: ERROR: irq: %d: 0x%x\n", __LINE__, irq, *L2VIC_INT_ENABLE(g_l2vic_base, irq));
            ret = 1;
        }
    }
    /* Clear them all, except int 1 and LEFT_SET (test)  */
    for (int irq = 1; irq < INTMAX; irq++) {
	if (!(irq % LEFT_SET)) continue;
        irq_bit  = (1 << (irq  % 32));
        *L2VIC_INT_ENABLE_CLEAR(g_l2vic_base, irq) |= irq_bit;
    }

    /* make sure just LEFT_SET is set */
    for (int irq = 0; irq < INTMAX; irq++) {
        if ((*L2VIC_INT_ENABLE(g_l2vic_base, irq) &
             (1 << (irq % 32))) != (0 << irq % 32)) {
            if (irq != LEFT_SET) {
                printf("%d: ERROR: irq: %d: 0x%x\n", __LINE__, irq, *L2VIC_INT_ENABLE(g_l2vic_base, irq));
		ret = 1;
            } else {
                left_set = irq;
            }
        }
    }
    if (left_set == LEFT_SET) {
        printf ("PASS\n");
    }
    return ret;
}
