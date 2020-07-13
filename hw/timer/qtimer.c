/*
 * Qualcomm QCT QTimer, L2VIC
 *
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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


#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/sysbus.h"
#include "hw/timer/qtimer.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "sysemu/runstate.h"


/*
 * QTimer provides an array of timers that have a common frequency
 */

#define QTIMER_DEFAULT_MAJOR_VER 1
#define QTIMER_DEFAULT_MINOR_VER 0

/* QTimer freq from reset
 * TODO: HW docs say 0 but does that match sim?
 */
#define QTIMER_DEFAULT_FREQ_HZ 19200000ULL
#define QTIMER_COUNT_MAX (0xffffffffffffffffULL)


#define QTMR_AC_CNTACR (0x40)
#define QTMR_TIMER_INDEX_MASK (0xf000)

#define QTMR_CNTPCT_LO (0x000)
#define QTMR_CNTPCT_HI (0x004)
#define QTMR_CNTP_CVAL_LO (0x020)
#define QTMR_CNTP_CVAL_HI (0x024)
#define QTMR_CNTP_TVAL (0x028)
#define QTMR_CNTP_CTL (0x02c)

#define HIGH_32(val) (0x0ffffffffULL & (val >> 32))
#define LOW_32(val) (0x0ffffffffULL & val)

static uint64_t Q_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    QTimerInfo *s = (QTimerInfo *)opaque;

    const int32_t timer_index = QTMR_TIMER_INDEX_MASK & offset;
    if (timer_index > s->timer_count)
        goto badreg;
    ptimer_state *timer = s->timers[timer_index].timer;
    offset = ~(QTMR_TIMER_INDEX_MASK)&offset;

    switch (offset) {
    case QTMR_CNTPCT_LO:
        return LOW_32(ptimer_get_count(timer));
    case QTMR_CNTPCT_HI:
        return HIGH_32(ptimer_get_count(timer));
    case QTMR_CNTP_CVAL_LO:
    case QTMR_CNTP_CVAL_HI:
    case QTMR_CNTP_TVAL:
    case QTMR_CNTP_CTL:
    case QTMR_AC_CNTACR:
        (void)s;
        fprintf(stderr, "QTimer not yet implemented\n");
        abort();
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unknown register 0x%02" HWADDR_PRIx " "
                                 ")\n",
                      __func__, offset);
        break;
    badreg:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: incorrect register 0x%02" HWADDR_PRIx " "
                      ")\n",
                      __func__, offset);
    }
    return 0;
}

static void Q_timer_write(void *opaque, hwaddr offset, uint64_t value,
                          unsigned size)
{
    QTimerInfo *s = (QTimerInfo *)opaque;

    const int32_t timer_index = QTMR_TIMER_INDEX_MASK & offset;
    if (timer_index > s->timer_count)
        goto badreg;

    ptimer_state *timer = s->timers[timer_index].timer;
    offset = ~(QTMR_TIMER_INDEX_MASK)&offset;
    switch (offset) {
    case QTMR_CNTPCT_LO:
    case QTMR_CNTPCT_HI:
        qemu_log_mask(
            LOG_GUEST_ERROR,
            "%s: attempted write to read-only register 0x%02" HWADDR_PRIx " "
            "(value 0x%08" PRIx64 ")\n",
            __func__, offset, value);
        break;

    case QTMR_CNTP_CTL:
        if ((value & 0x01) != 0) { /* Enable bit */
            ptimer_transaction_begin(timer);
            ptimer_set_count(timer, QTIMER_COUNT_MAX);
            ptimer_run(timer, 0);
            ptimer_transaction_commit(timer);
        }
        break;

    case QTMR_CNTP_CVAL_LO:
    case QTMR_CNTP_CVAL_HI:
    case QTMR_CNTP_TVAL:
    case QTMR_AC_CNTACR:
        (void)s;
        fprintf(stderr, "QTimer not yet implemented\n");
        abort();
        break;
    default:
        qemu_log_mask(LOG_UNIMP, "%s: unknown register 0x%02" HWADDR_PRIx " "
                                 "(value 0x%08" PRIx64 ")\n",
                      __func__, offset, value);
        break;
    badreg:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: incorrect register 0x%02" HWADDR_PRIx " "
                      "(value 0x%08" PRIx64 ")\n",
                      __func__, offset, value);
    }
}

static const MemoryRegionOps Q_timer_ops = {
    .read = Q_timer_read,
    .write = Q_timer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static void qtimer_expired(void *opaque)
{
    const QTimer *t = opaque;
    QTimerInfo *i = t->info;

    if (i->irq_enabled_mask & (1 << t->index)) {
        //      i->events |= 1 << t->index;
        qemu_irq_pulse(t->irq);
    }
}

static int qtimer_post_load(void *opaque, int version_id)
{
    return 0;
}

static void Q_timer_init(Object *obj)
{
    QTimerInfo *s = Q_TIMER(obj);
    SysBusDevice *dev = SYS_BUS_DEVICE(obj);

    s->major_ver = 0;
    s->minor_ver = 0;
    s->irq_enabled_mask = 0; // TODO: this is in L2VIC instead?
    memory_region_init_io(&s->iomem, OBJECT(s), &Q_timer_ops, s, "Q-timer",
                          QTIMER_MEM_REGION_SIZE_BYTES);
    sysbus_init_mmio(dev, &s->iomem);
}

static void Q_timer_realize(DeviceState *dev, Error **errp)
{
    QTimerInfo *s = Q_TIMER(dev);
    SysBusDevice *sbd = SYS_BUS_DEVICE(dev);
    int i;

    s->timer_count = QTIMER_MAX_TIMERS;
    for (i = 0; i < s->timer_count; i++) {
        s->timers[i].info = s;
        s->timers[i].index = i;
        s->timers[i].timer =
            ptimer_init(qtimer_expired, &(s->timers[i]), PTIMER_POLICY_DEFAULT);
        ptimer_transaction_begin(s->timers[i].timer);
        ptimer_set_freq(s->timers[i].timer, QTIMER_DEFAULT_FREQ_HZ);
        ptimer_transaction_commit(s->timers[i].timer);

        sysbus_init_irq(sbd, &(s->timers[i].irq));
        qdev_init_gpio_out(dev, &(s->timers[i].irq), i);
    }
}

static const VMStateDescription vmstate_Q_timer_regs = {
    .name = "Qtimer",
    .version_id = 1,
    .minimum_version_id = 1,
    .post_load = qtimer_post_load,
    .fields = (VMStateField[]){ VMSTATE_END_OF_LIST() }
};

static Property qtimer_dev_properties[] = {
    DEFINE_PROP_UINT32("major_ver", QTimerInfo, major_ver,
                       QTIMER_DEFAULT_MAJOR_VER),
    DEFINE_PROP_UINT32("minor_ver", QTimerInfo, minor_ver,
                       QTIMER_DEFAULT_MINOR_VER),
    DEFINE_PROP_END_OF_LIST(),
};

static void qtimer_dev_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->desc = "Q timer";
    device_class_set_props(dc, qtimer_dev_properties);
}

#ifndef __clang__
static const TypeInfo q_timer_dev_info = {
    .name = "qtimer",
    .parent = TYPE_Q_TIMER,
    .instance_size = sizeof(QTimerInfo),
    .class_init = qtimer_dev_class_init,
};
#endif


static void Q_timer_class_init(ObjectClass *oc, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(oc);

    dc->realize = Q_timer_realize;
    dc->vmsd = &vmstate_Q_timer_regs;
}

static const TypeInfo Q_timer_type_info = {
    .name = TYPE_Q_TIMER,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QTimerInfo),
    .instance_init = Q_timer_init,
    .abstract = false,
    .class_init = Q_timer_class_init,
};

static void Q_timer_register_types(void)
{
    type_register_static(&Q_timer_type_info);
}

type_init(Q_timer_register_types)
