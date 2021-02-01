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

#define INTERRUPT_MAX 1024
#define SLICE_MAX (INTERRUPT_MAX / 32)

typedef struct L2VICState {
    SysBusDevice parent_obj;

    QemuMutex active;
    MemoryRegion iomem;
    uint32_t level;
    uint32_t vid_group[4]; /* offset 0:vid group 0 etc, 10 bits in each group are used. */
    uint32_t int_clear[SLICE_MAX];  /* Clear Status of Active Edge interrupt, not used*/
    uint32_t int_enable[SLICE_MAX]; /* Enable interrupt source */
    uint32_t int_enable_clear;    /* Clear (set to 0) corresponding bit in int_enable */
    uint32_t int_enable_set;      /* Set (to 1) corresponding bit in int_enable */
    uint32_t int_pending[SLICE_MAX]; /* Present for debugging, not used */
    uint32_t int_soft;   /* Generate an interrupt */
    uint32_t int_status[SLICE_MAX]; /* Which enabled interrupt is active */
    uint32_t int_type[SLICE_MAX];  /* Edge or Level interrupt */
    uint32_t int_group_n0[SLICE_MAX];  /* L2 interrupt group 0 0x600-0x680 */
    uint32_t int_group_n1[SLICE_MAX];  /* L2 interrupt group 1 0x680-0x6FF */
    uint32_t int_group_n2[SLICE_MAX];  /* L2 interrupt group 2 0x700-0x77F */
    uint32_t int_group_n3[SLICE_MAX];  /* L2 interrupt group 3 0x780-0x7FF */
    qemu_irq irq[8];
} L2VICState;

/* Find out if this irq is associated with a group other than
 * the default group */
static uint32_t *get_int_group(L2VICState *s, int irq)
{
    int n = irq & 0x1f;
    if (n < 8) {
        return s->int_group_n0;
    }
    if (n < 16) {
        return s->int_group_n1;
    }
    if (n < 24) {
        return s->int_group_n2;
    }
    return s->int_group_n3;

}

static int find_slice(int irq)
{
    return irq / 32;
}

static int get_vid(L2VICState *s, int irq)
{
    uint32_t *group = get_int_group(s, irq);
    uint32_t slice = group[find_slice(irq)];
    /* Mask with 0x7 to remove the GRP:EN bit */
    uint32_t val = slice >> ((irq & 0x7) * 4);
    if (val & 0x8) {
        return val & 0x7;
    } else {
        return 0;
    }
}

static void l2vic_update(L2VICState *s, int level)
{
    int vid = get_vid(s, level);
    return qemu_set_irq(s->irq[vid + 2], level);
}

#define IRQBIT(irq) (1 << (irq) % 32)
static void l2vic_set_irq(void *opaque, int irq, int level)
{
    L2VICState *s = (L2VICState *) opaque;
    s->level = level;

    if (level && test_bit(irq, (unsigned long *)s->int_enable)) {
#if 0
        printf("ACK, irq:level = %d, %d\n", irq, level);
        printf("\t(L2VICA(s->int_enable, 4 * (irq / 32)) = 0x%x\n",
                L2VICA(s->int_enable, 4 * (irq / 32)));
        printf("\tIRQBIT(irq) = 0x%x\n", IRQBIT(irq));
#endif
        set_bit(irq, (unsigned long *)s->int_status);
        set_bit(irq, (unsigned long *)s->int_pending);

        /* Interrupts are automatically cleared and disabled, the ISR must
         * re-enable it by writing to L2VIC_INT_ENABLE_SETn area
        */
        clear_bit(irq, (unsigned long *)s->int_enable);
        /* Use the irq number as "level" so that hexagon_cpu_set_irq
         * can update the VID register*/
        l2vic_update (s, irq);
        s->vid_group[get_vid(s, irq)] = irq;
        return;

    } else {
#if 0
        printf("NACK, irq:level = %d, %d\n", irq, level);
        printf("\t(L2VICA(s->int_enable, 4 * (irq / 32)) = 0x%x\n",
                L2VICA(s->int_enable, 4 * (irq / 32)));
        printf("\tIRQBIT(irq) = 0x%x\n", IRQBIT(irq));
#endif

        return l2vic_update (s, 0);
    }
}

static uint64_t l2vic_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    L2VICState *s = (L2VICState *)opaque;

    if (offset >= L2VIC_INT_ENABLEn &&
        offset < (L2VIC_INT_ENABLEn + sizeof(s->int_enable))) {
        return L2VICA(s->int_enable, offset-L2VIC_INT_ENABLEn);
    } else if (offset >= (L2VIC_INT_ENABLEn + sizeof(s->int_enable)) &&
               offset <  L2VIC_INT_ENABLE_CLEARn) {
        qemu_log_mask(LOG_UNIMP, "%s: offset %x\n", __func__, (int) offset);
    } else if (offset >= L2VIC_INT_ENABLE_CLEARn &&
               offset < L2VIC_INT_ENABLE_SETn) {
        return 0;
    } else if (offset >= L2VIC_INT_ENABLE_SETn &&
               offset < L2VIC_INT_TYPEn) {
        return 0;
    } else if (offset >= L2VIC_INT_TYPEn &&
             offset < L2VIC_INT_TYPEn+0x80) {
        return L2VICA(s->int_type, offset-L2VIC_INT_TYPEn);
    } else if (offset >= L2VIC_INT_STATUSn &&
             offset < L2VIC_INT_CLEARn) {
        return L2VICA(s->int_status, offset-L2VIC_INT_STATUSn);
    } else if (offset >= L2VIC_INT_CLEARn &&
             offset < L2VIC_SOFT_INTn) {
        return L2VICA(s->int_clear, offset-L2VIC_INT_CLEARn);
    } else if (offset >= L2VIC_INT_PENDINGn &&
             offset < L2VIC_INT_PENDINGn + 0x80) {
        return L2VICA(s->int_pending, offset-L2VIC_INT_PENDINGn);
    } else if (offset >= L2VIC_INT_GRPn_0 &&
               offset < L2VIC_INT_GRPn_1) {
        return L2VICA(s->int_group_n0, offset - L2VIC_INT_GRPn_0);
    } else if (offset >= L2VIC_INT_GRPn_1 &&
               offset < L2VIC_INT_GRPn_2) {
        return L2VICA(s->int_group_n1, offset - L2VIC_INT_GRPn_1);
    } else if (offset >= L2VIC_INT_GRPn_2 &&
               offset < L2VIC_INT_GRPn_3) {
        return L2VICA(s->int_group_n2, offset - L2VIC_INT_GRPn_2);
    } else if (offset >= L2VIC_INT_GRPn_3 &&
               offset < L2VIC_INT_GRPn_3 + 0x80) {
        return L2VICA(s->int_group_n3, offset - L2VIC_INT_GRPn_3);
    } else if (offset == 0) {
        return s->vid_group[0];
    } else if (offset == 4) {
        return s->vid_group[1];
    } else if (offset == 8) {
        return s->vid_group[2];
    } else if (offset == 12) {
        return s->vid_group[3];
    }
    return 0;
}

static void l2vic_write(void *opaque, hwaddr offset,
                        uint64_t val, unsigned size) {
    L2VICState *s = (L2VICState *) opaque;
    qemu_mutex_lock(&s->active);

    if (offset >= L2VIC_INT_ENABLEn &&
        offset < (L2VIC_INT_ENABLEn + ARRAY_SIZE(s->int_enable))) {
        L2VICA(s->int_enable, offset - L2VIC_INT_ENABLEn) = val;
    } else if (offset >= (L2VIC_INT_ENABLEn + ARRAY_SIZE(s->int_enable)) &&
               offset < L2VIC_INT_ENABLE_CLEARn) {
        qemu_log_mask(LOG_UNIMP, "%s: offset %x\n", __func__, (int) offset);
    } else if (offset >= L2VIC_INT_ENABLE_CLEARn &&
               offset < L2VIC_INT_ENABLE_SETn) {
        L2VICA(s->int_enable, offset - L2VIC_INT_ENABLE_CLEARn) &= ~val;
    } else if (offset >= L2VIC_INT_ENABLE_SETn &&
               offset < L2VIC_INT_TYPEn) {
        L2VICA(s->int_status, offset - L2VIC_INT_ENABLE_SETn) &= ~val;
        L2VICA(s->int_pending, offset - L2VIC_INT_ENABLE_SETn) &= ~val;
        L2VICA(s->int_enable, offset - L2VIC_INT_ENABLE_SETn) |= val;
    } else if (offset >= L2VIC_INT_TYPEn &&
               offset < L2VIC_INT_TYPEn + 0x80) {
        L2VICA(s->int_type, offset - L2VIC_INT_TYPEn) = val;
    } else if (offset >= L2VIC_INT_STATUSn &&
               offset < L2VIC_INT_CLEARn) {
        L2VICA(s->int_status, offset - L2VIC_INT_STATUSn) = val;
    } else if (offset >= L2VIC_INT_CLEARn &&
               offset < L2VIC_SOFT_INTn) {
        L2VICA(s->int_clear, offset - L2VIC_INT_CLEARn) = val;
    } else if (offset >= L2VIC_INT_PENDINGn &&
               offset < L2VIC_INT_PENDINGn + 0x80) {
        L2VICA(s->int_pending, offset - L2VIC_INT_PENDINGn) = val;
    } else if (offset >= L2VIC_SOFT_INTn &&
               offset < L2VIC_INT_PENDINGn) {
        /*
         *  Need to reverse engineer the actual irq number.
         */
        int irq = find_first_bit(&val, 32);
        hwaddr wordoffset = offset - L2VIC_SOFT_INTn;
        g_assert(irq != 32);
        irq += wordoffset * 8;

        qemu_mutex_unlock(&s->active);
        return l2vic_set_irq(opaque, irq, 1);
    } else if (offset >= L2VIC_INT_GRPn_0 &&
               offset < L2VIC_INT_GRPn_1) {
        L2VICA(s->int_group_n0, offset - L2VIC_INT_GRPn_0) = val;
    } else if (offset >= L2VIC_INT_GRPn_1 &&
               offset < L2VIC_INT_GRPn_2) {
        L2VICA(s->int_group_n1, offset - L2VIC_INT_GRPn_1) = val;
    } else if (offset >= L2VIC_INT_GRPn_2 &&
               offset < L2VIC_INT_GRPn_3) {
        L2VICA(s->int_group_n2, offset - L2VIC_INT_GRPn_2) = val;
    } else if (offset >= L2VIC_INT_GRPn_3 &&
               offset < L2VIC_INT_GRPn_3 + 0x80) {
        L2VICA(s->int_group_n3, offset - L2VIC_INT_GRPn_3) = val;
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
    memset(s->int_status, 0, sizeof(s->int_status));
    memset(s->int_type, 0, sizeof(s->int_type));
    memset(s->int_group_n0, 0, sizeof(s->int_group_n0));
    memset(s->int_group_n1, 0, sizeof(s->int_group_n1));
    memset(s->int_group_n2, 0, sizeof(s->int_group_n2));
    memset(s->int_group_n3, 0, sizeof(s->int_group_n3));
    s->int_soft = 0;

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
    qdev_init_gpio_in(dev, l2vic_set_irq, 1024);
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
        VMSTATE_UINT32_ARRAY(vid_group, L2VICState, 4),
        VMSTATE_UINT32_ARRAY(int_enable, L2VICState, SLICE_MAX),
        VMSTATE_UINT32(int_enable_clear, L2VICState),
        VMSTATE_UINT32(int_enable_set, L2VICState),
        VMSTATE_UINT32_ARRAY(int_type, L2VICState, SLICE_MAX),
        VMSTATE_UINT32_ARRAY(int_status, L2VICState, SLICE_MAX),
        VMSTATE_UINT32_ARRAY(int_clear, L2VICState, SLICE_MAX),
        VMSTATE_UINT32(int_soft, L2VICState),
        VMSTATE_UINT32_ARRAY(int_pending, L2VICState, SLICE_MAX),
        VMSTATE_UINT32_ARRAY(int_group_n0, L2VICState, SLICE_MAX),
        VMSTATE_UINT32_ARRAY(int_group_n1, L2VICState, SLICE_MAX),
        VMSTATE_UINT32_ARRAY(int_group_n2, L2VICState, SLICE_MAX),
        VMSTATE_UINT32_ARRAY(int_group_n3, L2VICState, SLICE_MAX),
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
