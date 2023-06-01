/*
 * Test the fastl2vic interface.
 *
 *  hexagon-sim a.out --subsystem_base=0xfab0  --cosim_file q6ss.cfg
 */

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

#define CSR_BASE  0xfab00000
#define L2VIC_BASE ((CSR_BASE) + 0x10000)
#define L2VIC_INT_ENABLE(b, n) \
        ((volatile unsigned int *) ((b) + 0x100 + 4 * (n / 32)))
#define L2VIC_INT_ENABLE_SET(b, n) \
        ((volatile unsigned int *) ((b) + 0x200 + 4 * (n / 32)))

int
main()
{
    int ret = 0;
    unsigned int irq_bit;

    /* setup the fastl2vic interface and setup an indirect mapping */
    volatile  uint32_t *A = (uint32_t *)0x888e0000;
    uint32_t cfgbase = get_cfgbase();
    unsigned int fastl2vic = _rdcfg(cfgbase, 0x28);   /* Fastl2vic */
    add_translation_extended(3, (void *)A, fastl2vic << 16, 16, 7, 4, 0, 0, 3);

    uint32_t l2vic_base = (_rdcfg(cfgbase, 0x8) << 16) + 0x10000;

    /* set and verify an interrupt using the L2VIC_BASE */
    irq_bit  = (1 << (66  % 32));
    *L2VIC_INT_ENABLE_SET(l2vic_base, 66)   = irq_bit;
    if (*L2VIC_INT_ENABLE(l2vic_base, 64) != 0x4) {
        ret = __LINE__;
    }

    /* set and verify an interrupt using the FASTL2VIC interface */
    *A = 68;
    if (*L2VIC_INT_ENABLE(l2vic_base, 64) != 0x14) {
        ret = __LINE__;
    }
    *A = 67;
    if (*L2VIC_INT_ENABLE(l2vic_base, 64) != 0x1C) {
        ret = __LINE__;
    }


    /* Now clear the lines */
    *A = ((1 << 16) | 68);
    if (*L2VIC_INT_ENABLE(l2vic_base, 64) != 0xC) {
        ret = __LINE__;
    }
    *A = ((1 << 16) | 66);
    if (*L2VIC_INT_ENABLE(l2vic_base, 64) != 0x8) {
        ret = __LINE__;
    }
    *A = ((1 << 16) | 67);
    if (*L2VIC_INT_ENABLE(l2vic_base, 64) != 0x0) {
        ret = __LINE__;
    }

    if (ret) {
        printf("%s: FAIL, last failure near line %d\n", __FILE__, ret);
    } else {
        printf("PASS\n");
    }
    return ret;
}
