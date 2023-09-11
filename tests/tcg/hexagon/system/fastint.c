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
 * Test the fastl2vic interface.
 *
 *  hexagon-sim a.out --subsystem_base=0xfab0  --cosim_file q6ss.cfg
 * The q6ss.cfg might look like this:
 * /local/mnt/workspace/tools/cosims/qtimer/bin/lnx64/qtimer.so --debug --csr_base=0xFab00000 --irq_p=2 --freq=19200000 --cnttid=1
 * /local/mnt/workspace/tools/cosims/l2vic/src/../bin/lnx64/l2vic.so 32 0xFab10000
 */

#include <assert.h>
#include <hexagon_standalone.h>
#include "filename.h"

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
    ((volatile unsigned int *) ((b) + 0x100 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_ENABLE_SET(b, n) \
   ((volatile unsigned int *) ((b) + 0x200 + 4 * (n / 32))) /* device memory */
#define L2VIC_SOFT_INT_SET(b, n) \
   ((volatile unsigned int *) ((b) + 0x480 + 4 * (n / 32))) /* device memory */
#define L2VIC_INT_TYPE(b, n) \
   ((volatile unsigned int *) ((b) + 0x280 + 4 * (n / 32))) /* device memory */

#define RESET 0
#define INTMAX 1024
volatile int pass;
int g_irq;
uint32_t g_l2vic_base;
void intr_handler(int irq)
{
    unsigned int vid, irq_bit;
    __asm__ __volatile__("%0 = VID" : "=r"(vid));
    g_irq = vid;

    /* re-enable the irq */
    irq_bit = (1 << (vid  % 32));
    *L2VIC_INT_ENABLE_SET(g_l2vic_base, vid)   = irq_bit;
    pass++;

}
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

    g_l2vic_base = (_rdcfg(cfgbase, 0x8) << 16) + 0x10000;

    register_interrupt(2, intr_handler);

    /* Setup interrupts */
    for (int irq = 1; irq < INTMAX; irq++) {
        irq_bit  = (1 << (irq  % 32));
        *A = irq;
        *L2VIC_INT_TYPE(g_l2vic_base, irq) |= irq_bit; /* Edge */

        if ((*L2VIC_INT_ENABLE(g_l2vic_base, irq) &
             (1 << (irq % 32))) != (1 << irq%32)) {
            printf(" ERROR got: 0x%x\n", *L2VIC_INT_ENABLE(g_l2vic_base, irq));
            printf(" ERROR exp: 0x%x\n",  (1 << irq % 32));
            ret = __LINE__;
        }
    }

    /* Trigger interrupts */
    for (int irq = 1; irq < INTMAX; irq++) {
        irq_bit  = (1 << (irq  % 32));
        *L2VIC_SOFT_INT_SET(g_l2vic_base, irq) = irq_bit;
       while (!pass) {
             ;
             /* printf("waiting for irq %d\n", irq); */
        }
        assert(g_irq == irq);
	pass = RESET;
     }

    if (ret) {
        printf("%s: FAIL, last failure near line %d\n", __FILENAME__, ret);
    } else {
        printf("PASS : %s\n", __FILENAME__);
    }
    return ret;
}
