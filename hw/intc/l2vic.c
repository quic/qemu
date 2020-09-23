/*
 * QEMU L2VIC Interrupt Controller
 *
 * Arm PrimeCell PL190 Vector Interrupt Controller was used as a reference.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/sysbus.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "l2vic.h"

#define L2VICA(s, n) \
    (s[(n)>>2])

#define TYPE_L2VIC "l2vic"
#define L2VIC(obj) OBJECT_CHECK(L2VICState, (obj), TYPE_L2VIC)

#define INT_COUNT 32

typedef struct L2VICState {
    SysBusDevice parent_obj;

    QemuMutex active;
    MemoryRegion iomem;
    uint32_t level;
    uint32_t int_clear[INT_COUNT];  /* Clear Status of Active Edge interrupt, not used*/
    uint32_t int_enable[INT_COUNT]; /* Enable interrupt source */
    uint32_t int_enable_clear;    /* Clear (set to 0) corresponding bit in int_enable */
    uint32_t int_enable_set;      /* Set (to 1) corresponding bit in int_enable */
    uint32_t int_pending[INT_COUNT]; /* Present for debugging, not used */
    uint32_t int_soft[INT_COUNT];   /* Generate an interrupt, not used */
    uint32_t int_status[INT_COUNT]; /* Which enabled interrupt is active */
    uint32_t int_type[INT_COUNT];  /* Edge or Level interrupt */
    qemu_irq irq[8];
} L2VICState;

/* Utility routines for dealing with the 128 possible irq bits */
typedef struct {
    uint64_t lo;
    uint64_t hi;
} irq_t;

static void setbit(void *addr, unsigned int n) {
    irq_t irq;
    memcpy(&irq, addr, sizeof(irq));
    if (n > 128)
        return;
    if (n < 64)
        irq.lo |= (1ull << n);
    else
        irq.hi |= (1ull << (n - 64));
    memcpy(addr, &irq, sizeof(irq));
    return;
}
static void clearbit(void  *addr, unsigned int n) {
    irq_t irq;
    memcpy(&irq, addr, sizeof(irq));
    if (n > 128)
        return;
    if (n < 64)
        irq.lo &= ~(1ull << n);
    else
        irq.hi &= ~(1ull << (n - 64));
    memcpy(addr, &irq, sizeof(irq));
    return;
}
static int isset(const void *addr, unsigned int n)
{
    irq_t irq;
    memcpy(&irq, addr, sizeof(irq));
    if (n>128)
        return 0;
    if (n<64)
        return ((irq.lo & (1ull << n)) != 0);
    return ((irq.hi & (1ull << (n - 64))) != 0);
}

static void l2vic_update(L2VICState *s, int level)
{
    return qemu_set_irq(s->irq[2], level);
}

#define IRQBIT(irq) (1 << (irq) % 32)
static void l2vic_set_irq(void *opaque, int irq, int level) {
    L2VICState *s = (L2VICState *) opaque;
    s->level = level;

    if (level && isset(s->int_enable, irq)) {
#if 0
        printf ("ACK, irq = %d\n", irq);
        printf ("\t(L2VICA(s->int_enable, 4 * (irq / 32)) = 0x%lx\n",
                L2VICA(s->int_enable, 4 * (irq / 32)));
        printf ("\tIRQBIT(irq) = 0x%x\n", IRQBIT(irq));
#endif
        setbit(s->int_status, irq);

        /* Interrupts are automatically cleared and disabled, the ISR must
         * re-enable it by writing to L2VIC_INT_ENABLE_SETn area
        */
        clearbit(s->int_enable, irq);
        /* Use the irq number as "level" so that hexagon_cpu_set_irq
         * can update the VID register*/
        l2vic_update (s, irq);
        return;

    } else {
#if 0
        printf ("NACK, irq = %d\n", irq);
        printf ("\t(L2VICA(s->int_enable, 4 * (irq / 32)) = 0x%lx\n",
                L2VICA(s->int_enable, 4 * (irq / 32)));
        printf ("\tIRQBIT(irq) = 0x%x\n", IRQBIT(irq));
#endif

        return l2vic_update (s, 0);
    }
}

static uint64_t l2vic_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    L2VICState *s = (L2VICState *)opaque;

    if (offset >= L2VIC_INT_ENABLEn &&
        offset <  (L2VIC_INT_ENABLEn + ARRAY_SIZE(s->int_enable)) ) {
        return L2VICA(s->int_enable, offset-L2VIC_INT_ENABLEn);
    }
    else if (offset >= (L2VIC_INT_ENABLEn + ARRAY_SIZE(s->int_enable)) &&
             offset <  L2VIC_INT_ENABLE_CLEARn) {
        qemu_log_mask(LOG_UNIMP, "%s: offset %x\n", __func__, (int) offset);
    }
    else if (offset >= L2VIC_INT_ENABLE_CLEARn &&
             offset < L2VIC_INT_ENABLE_SETn) {
        return 0;
    }
    else if (offset >= L2VIC_INT_ENABLE_SETn &&
             offset < L2VIC_INT_TYPEn) {
        return 0;
    }
    else if (offset >= L2VIC_INT_TYPEn &&
             offset < L2VIC_INT_TYPEn+0x80) {
        return L2VICA(s->int_type, offset-L2VIC_INT_TYPEn);
    }
    else if (offset >= L2VIC_INT_STATUSn &&
             offset < L2VIC_INT_CLEARn) {
        return L2VICA(s->int_status, offset-L2VIC_INT_STATUSn);
    }
    else if (offset >= L2VIC_INT_CLEARn &&
             offset < L2VIC_SOFT_INTn) {
        return L2VICA(s->int_clear, offset-L2VIC_INT_CLEARn);
    }
    else if (offset >= L2VIC_INT_PENDINGn &&
             offset < L2VIC_INT_PENDINGn + 0x80) {
        return L2VICA(s->int_pending, offset-L2VIC_INT_PENDINGn);
    }
    return 0;
}

static void l2vic_write(void *opaque, hwaddr offset,
                        uint64_t val, unsigned size)
{
    L2VICState *s = (L2VICState *)opaque;
    qemu_mutex_lock(&s->active);

    if (offset >= L2VIC_INT_ENABLEn &&
        offset <  (L2VIC_INT_ENABLEn + ARRAY_SIZE(s->int_enable)) ) {
        L2VICA(s->int_enable, offset-L2VIC_INT_ENABLEn) = val;
    }
    else if (offset >= (L2VIC_INT_ENABLEn + ARRAY_SIZE(s->int_enable)) &&
        offset <  L2VIC_INT_ENABLE_CLEARn) {
        qemu_log_mask(LOG_UNIMP, "%s: offset %x\n", __func__, (int) offset);
    }
    else if (offset >= L2VIC_INT_ENABLE_CLEARn &&
             offset < L2VIC_INT_ENABLE_SETn) {
        L2VICA(s->int_enable, offset-L2VIC_INT_ENABLE_CLEARn) &= ~val;
    }
    else if (offset >= L2VIC_INT_ENABLE_SETn &&
             offset < L2VIC_INT_TYPEn) {
        L2VICA(s->int_status, offset-L2VIC_INT_ENABLE_SETn) &= ~val;
        L2VICA(s->int_enable, offset-L2VIC_INT_ENABLE_SETn) |= val;
        /* This will also clear int_status bit. */
        //qemu_set_irq(s->irq[2], 0);
    }
    else if (offset >= L2VIC_INT_TYPEn &&
             offset < L2VIC_INT_TYPEn+0x80) {
        L2VICA(s->int_type, offset-L2VIC_INT_TYPEn) = val;
    }
    else if (offset >= L2VIC_INT_STATUSn &&
             offset < L2VIC_INT_CLEARn) {
        L2VICA(s->int_status, offset-L2VIC_INT_STATUSn) = val;
    }
    else if (offset >= L2VIC_INT_CLEARn &&
             offset < L2VIC_SOFT_INTn) {
        L2VICA(s->int_clear, offset-L2VIC_INT_CLEARn) = val;
    }
    else if (offset >= L2VIC_INT_PENDINGn &&
             offset < L2VIC_INT_PENDINGn + 0x80) {
        L2VICA(s->int_pending, offset-L2VIC_INT_PENDINGn) = val;
    }
    qemu_mutex_unlock(&s->active);
    return;
}

static const MemoryRegionOps l2vic_ops = {
    .read = l2vic_read,
    .write = l2vic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void l2vic_reset(DeviceState *d)
{
    L2VICState *s = L2VIC(d);
    memset(s->int_clear, 0, sizeof(s->int_clear));
    memset(s->int_enable, 0, sizeof(s->int_enable));
    memset(s->int_pending, 0, sizeof(s->int_pending));
    memset(s->int_soft, 0, sizeof(s->int_soft));
    memset(s->int_status, 0, sizeof(s->int_status));
    memset(s->int_type, 0, sizeof(s->int_type));

    l2vic_update(s, 0);
}

static void l2vic_init(Object *obj)
{
    DeviceState *dev = DEVICE(obj);
    L2VICState *s = L2VIC(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);
    int i;

    memory_region_init_io(&s->iomem, obj, &l2vic_ops, s, "l2vic", 0x1000);
    sysbus_init_mmio(sbd, &s->iomem);
    qdev_init_gpio_in(dev, l2vic_set_irq, 128);
    for (i=0; i<8; i++)
        sysbus_init_irq(sbd, &s->irq[i]);
    qemu_mutex_init(&s->active); /* TODO: Remove this is an experiment */
}

static const VMStateDescription vmstate_l2vic = {
    .name = "l2vic",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(level, L2VICState),
        VMSTATE_UINT32_ARRAY(int_enable, L2VICState, INT_COUNT),
        VMSTATE_UINT32(int_enable_clear, L2VICState),
        VMSTATE_UINT32(int_enable_set, L2VICState),
        VMSTATE_UINT32_ARRAY(int_type, L2VICState, INT_COUNT),
        VMSTATE_UINT32_ARRAY(int_status, L2VICState, INT_COUNT),
        VMSTATE_UINT32_ARRAY(int_clear, L2VICState, INT_COUNT),
        VMSTATE_UINT32_ARRAY(int_soft, L2VICState, INT_COUNT),
        VMSTATE_UINT32_ARRAY(int_pending, L2VICState, INT_COUNT),
        VMSTATE_END_OF_LIST()
    }
};

static void l2vic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = l2vic_reset;
    dc->vmsd = &vmstate_l2vic;
}

static const TypeInfo l2vic_info = {
    .name          = TYPE_L2VIC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(L2VICState),
    .instance_init = l2vic_init,
    .class_init    = l2vic_class_init,
};

static void l2vic_register_types(void)
{
    type_register_static(&l2vic_info);
}

type_init(l2vic_register_types)
