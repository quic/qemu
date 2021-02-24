/*
 * Qualcomm QCT QTimer
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
#include "hw/hexagon/qtimer.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "hw/ptimer.h"
#include "sysemu/runstate.h"

/* Common timer implementation.  */

//#define QTIMER_DEFAULT_FREQ_HZ 19200000ULL
#define QTIMER_DEFAULT_FREQ_HZ   19200000ULL
#define QTMR_TIMER_INDEX_MASK (0xf000)
#define HIGH_32(val) (0x0ffffffffULL & (val >> 32))
#define LOW_32(val) (0x0ffffffffULL & val)

typedef struct {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
    ptimer_state *timer;
    uint64_t cntval;       /* Physical timer compare value interrupt when cntpct > cntval */
    uint64_t cntpct;       /* Physical counter */
    uint32_t control;
    uint32_t cnt_ctrl;
    uint64_t limit;
    uint32_t freq;
    uint32_t devid;
    int int_level;
    qemu_irq irq;
} hex_timer_state;

typedef struct QuTIMERState {
    SysBusDevice parent_obj;

    MemoryRegion iomem;
    uint32_t secure;
    hex_timer_state *timer[2];
    uint32_t frame_id;
    uint32_t freq;
    int level[2];
    qemu_irq irq;
} QuTIMERState;


#define TYPE_QuTIMER "qutimer"
#define TYPE_HexTIMER "hextimer"
#define QuTIMER(obj) OBJECT_CHECK(QuTIMERState, (obj), TYPE_QuTIMER)
#define HexTIMER(obj) OBJECT_CHECK(hex_timer_state, (obj), TYPE_HexTIMER)

/* Merge the IRQs from the two component devices.  */
static void qutimer_set_irq(void *opaque, int irq, int level)
{
    QuTIMERState *s = (QuTIMERState *)opaque;

    s->level[irq] = level;
    s->timer[0]->int_level = level;
    s->timer[1]->int_level = level;
    qemu_set_irq(s->irq, s->level[0] || s->level[1]);
}

/* qutimer_read/write:
 * if offset < 0x1000 read restricted registers:
 * QTMR_AC_CNTFREQ/CNTSR/CNTTID/CNTACR/CNTOFF_(LO/HI)/QTMR_VERSION
 * else if offset < 0x2000
 *     - Reference timer 1
 * else
 *     - Reference timer 2
 */
static uint64_t qutimer_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    QuTIMERState *s = (QuTIMERState *)opaque;

    if (offset < 0x1000) {
        switch (offset) {
        case QTMR_AC_CNTFRQ:
           return s->freq;
        case QTMR_AC_CNTSR:
           return s->secure;
        case QTMR_AC_CNTTID:
           return 0x11; /* Only frame 0 and frame 1 are implemented. */
        case QTMR_AC_CNTACR_0:
            return s->timer[0]->cnt_ctrl;
        case QTMR_AC_CNTACR_1:
            return s->timer[1]->cnt_ctrl;
        case QTMR_VERSION:
            return 0x10000000;
        default:
                qemu_log_mask(LOG_GUEST_ERROR,
                              "%s: QTMR_AC_CNT: Bad offset %x\n",
                              __func__, (int) offset);
            return 0x0;
        }
    }

    qemu_log_mask(LOG_GUEST_ERROR,
                  "%s: Bad offset 0x%x\n", __func__, (int)offset);
    return 0;
}

static void qutimer_write(void *opaque, hwaddr offset,
                        uint64_t value, unsigned size)
{
    QuTIMERState *s = (QuTIMERState *)opaque;

    if (offset < 0x1000) {
        switch (offset) {
        case QTMR_AC_CNTFRQ:
            s->freq = value;
            return;
        case QTMR_AC_CNTSR:
            if (value > 3)
                qemu_log_mask(LOG_GUEST_ERROR, "%s: QTMR_AC_CNTSR: Bad value %x\n",
                              __func__, (int) value);
            else
                s->secure = value;
            return;
        case QTMR_AC_CNTACR_0:
            s->timer[0]->cnt_ctrl = value;
            return;
        case QTMR_AC_CNTACR_1:
            s->timer[1]->cnt_ctrl = value;
            return;
        default:
            qemu_log_mask(LOG_GUEST_ERROR, "%s: QTMR_AC_CNT: Bad offset %x\n",
                          __func__, (int) offset);
            return;
        }
    }
    else
        qemu_log_mask(LOG_GUEST_ERROR, "%s: Bad offset %x\n", __func__, (int)offset);
}

static const MemoryRegionOps qutimer_ops = {
    .read = qutimer_read,
    .write = qutimer_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_qutimer = {
    .name = "qutimer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_INT32_ARRAY(level, QuTIMERState, 2),
        VMSTATE_END_OF_LIST()
    }
};

static QuTIMERState *QuTimerBase;
static void qutimer_init(Object *obj)
{
    QuTIMERState *s = QuTIMER(obj);
    QuTimerBase = s;
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &qutimer_ops, s,
                          "qutimer", QTIMER_MEM_SIZE_BYTES);
    sysbus_init_mmio(sbd, &s->iomem);
}

static Property qutimer_properties[] = {
    DEFINE_PROP_UINT32("secure", QuTIMERState, secure,
                   (QTMR_AC_CNTSR_NSN_1 | QTMR_AC_CNTSR_NSN_2)),
    DEFINE_PROP_UINT32("frame_id", QuTIMERState, frame_id, 0),
    DEFINE_PROP_UINT32("freq", QuTIMERState, freq, QTIMER_DEFAULT_FREQ_HZ),
    DEFINE_PROP_END_OF_LIST(),
};

static void qutimer_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    device_class_set_props(k, qutimer_properties);
    k->vmsd = &vmstate_qutimer;
}


static const TypeInfo qutimer_info = {
    .name          = TYPE_QuTIMER,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QuTIMERState),
    .instance_init = qutimer_init,
    .class_init    = qutimer_class_init,
};

static void qutimer_timer_register_types(void)
{
    type_register_static(&qutimer_info);
}

type_init(qutimer_timer_register_types)


static void hex_timer_update(hex_timer_state *s)
{
    /* Update interrupts.  */
    if (s->int_level && (s->control & QTMR_CNTP_CTL_ENABLE)) {
        qemu_irq_raise(s->irq);
    } else {
        qemu_irq_lower(s->irq);
    }
}

static uint64_t hex_timer_read(void *opaque, hwaddr offset, unsigned size)
{
    hex_timer_state *s = (hex_timer_state *)opaque;

    switch (offset) {
        case (QTMR_CNT_FREQ): /* Ticks/Second */
            return s->freq;
        case (QTMR_CNTP_CVAL_LO): /* TimerLoad */
            return LOW_32((s->cntval));
        case (QTMR_CNTP_CVAL_HI): /* TimerLoad */
            return HIGH_32((s->cntval));
        case QTMR_CNTPCT_LO:
            return LOW_32((s->cntpct + (ptimer_get_count(s->timer))));
        case QTMR_CNTPCT_HI:
            return HIGH_32((s->cntpct + (ptimer_get_count(s->timer))));
        case (QTMR_CNTP_TVAL): /* CVAL - CNTP */
            return (s->cntval -
                    (HIGH_32((s->cntpct + (ptimer_get_count(s->timer)))) +
                     LOW_32((s->cntpct + (ptimer_get_count(s->timer))))));

        case (QTMR_CNTP_CTL): /* TimerMIS */
            return s->int_level;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            return 0;
    }
}

/*
 * Reset the timer limit after settings have changed.
 * May only be called from inside a ptimer transaction block.
 */
static void hex_timer_recalibrate(hex_timer_state *s, int reload)
{
    uint64_t limit;
    /* Periodic.  */
    limit = s->limit;
    ptimer_set_limit(s->timer, limit, reload);
}

static void hex_timer_write(void *opaque, hwaddr offset,
                            uint64_t value, unsigned size)
{
    hex_timer_state *s = (hex_timer_state *)opaque;
    HEX_TIMER_LOG("\ta timer write: %lu, %lu\n", offset, value);

    switch (offset) {
        case (QTMR_CNTP_CVAL_LO): /* TimerLoad */
            /*HEX_TIMER_LOG ("A s->limit        = %d\n", s->limit);
              HEX_TIMER_LOG ("B s->limit        = %d\n", s->limit);
              HEX_TIMER_LOG ("value             = %d\n",value);
              HEX_TIMER_LOG ("s->cntpct         = %d\n", s->cntpct);
              HEX_TIMER_LOG ("s->cntcval        = %d\n", s->cntval);
              HEX_TIMER_LOG ("value - cntcval   = %d\n", value - s->cntval);
             */
            HEX_TIMER_LOG("value(%ld) - cntcval(%ld) = %ld\n",
                           value, s->cntval, value - s->cntval);
            s->int_level = 0;
            s->cntval = value;
            ptimer_transaction_begin(s->timer);
            if (s->control & QTMR_CNTP_CTL_ENABLE) {
                /* Pause the timer if it is running.  This may cause some
                   inaccuracy due to rounding, but avoids other issues. */
                ptimer_stop(s->timer);
            }
            hex_timer_recalibrate(s, 1);
            if (s->control & QTMR_CNTP_CTL_ENABLE) {
                ptimer_run(s->timer, 0);
            }
            ptimer_transaction_commit(s->timer);
            break;
        case (QTMR_CNTP_CVAL_HI):
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: QTMR_CNTP_CVAL_HI is read-only\n", __func__);
            break;
        case (QTMR_CNTP_CTL): /* Timer control register */
            HEX_TIMER_LOG("\tctl write: %lu\n", value);
            ptimer_transaction_begin(s->timer);
            if (s->control & QTMR_CNTP_CTL_ENABLE) {
                /* Pause the timer if it is running.  This may cause some
                   inaccuracy due to rounding, but avoids other issues. */
                ptimer_stop(s->timer);
            }
            s->control = value;
            hex_timer_recalibrate(s, s->control & QTMR_CNTP_CTL_ENABLE);
            ptimer_set_freq(s->timer, s->freq);
            ptimer_set_period(s->timer, 1);
            if (s->control & QTMR_CNTP_CTL_ENABLE) {
                ptimer_run(s->timer, 0);
            }
            ptimer_transaction_commit(s->timer);
            break;
        case (QTMR_CNTP_TVAL): /* CVAL - CNTP */
            ptimer_transaction_begin(s->timer);
            if (s->control & QTMR_CNTP_CTL_ENABLE) {
                /* Pause the timer if it is running.  This may cause some
                   inaccuracy due to rounding, but avoids other issues. */
                ptimer_stop(s->timer);
            }
            s->cntval = s->cntpct + value;
            ptimer_set_freq(s->timer, s->freq);
            ptimer_set_period(s->timer, 1);
            if (s->control & QTMR_CNTP_CTL_ENABLE) {
                ptimer_run(s->timer, 0);
            }
            ptimer_transaction_commit(s->timer);
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                          "%s: Bad offset %x\n", __func__, (int)offset);
            break;
    }
    hex_timer_update(s);
}

static void hex_timer_tick(void *opaque)
{
    hex_timer_state *s = (hex_timer_state *)opaque;
    s->cntpct += s->limit;
    if (s->cntpct >= s->cntval) {
        s->int_level = 1;
        hex_timer_update(s);
        ptimer_stop(s->timer);
        HEX_TIMER_LOG("\nFIRE!!! %ld\n", s->cntpct);
    }
}

static const MemoryRegionOps hex_timer_ops = {
        .read = hex_timer_read,
        .write = hex_timer_write,
        .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_hex_timer = {
    .name = "hex_timer",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(control, hex_timer_state),
        VMSTATE_UINT32(cnt_ctrl, hex_timer_state),
        VMSTATE_UINT64(cntpct, hex_timer_state),
        VMSTATE_UINT64(cntval, hex_timer_state),
        VMSTATE_UINT64(limit, hex_timer_state),
        VMSTATE_INT32(int_level, hex_timer_state),
        VMSTATE_PTIMER(timer, hex_timer_state),
        VMSTATE_END_OF_LIST()
    }
};

static void hex_instance_init(Object *obj) {
    hex_timer_state *s = HexTIMER(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    sysbus_init_irq(sbd, &s->irq);
    memory_region_init_io(&s->iomem, obj, &hex_timer_ops, s,
                          "hextimer", QTIMER_MEM_REGION_SIZE_BYTES);
    sysbus_init_mmio(sbd, &s->iomem);
    if (QuTimerBase) {
        if (QuTimerBase->timer[0] == NULL) {
            QuTimerBase->timer[0] = s;
            s->devid = 0;
        }
        else {
            QuTimerBase->timer[1] = s;
            s->devid = 1;
        }
    }
}

static hex_timer_state *hex_timer_init(hex_timer_state *s, uint32_t freq)
{
    s->freq = freq;
    s->control = QTMR_CNTP_CTL_ENABLE;
    s->cnt_ctrl = (QTMR_AC_CNTACR_RWPT | QTMR_AC_CNTACR_RWVT |
                   QTMR_AC_CNTACR_RVOFF | QTMR_AC_CNTACR_RFRQ |
                   QTMR_AC_CNTACR_RPVCT | QTMR_AC_CNTACR_RPCT);

    s->timer = ptimer_init(hex_timer_tick, s, PTIMER_POLICY_DEFAULT);
    vmstate_register(NULL, VMSTATE_INSTANCE_ID_ANY, &vmstate_hex_timer, s);
#if 0
    /* auto start qtimer */
    hex_timer_write(s, QTMR_CNTP_TVAL, 27428, 0);
    hex_timer_write(s, QTMR_CNTP_CTL, 1, 0);
#endif

    return s;
}

static void hex_timer_realize(DeviceState *dev, Error **errp)
{
    hex_timer_state *s = HexTIMER(dev);

    s = hex_timer_init(s, QTIMER_DEFAULT_FREQ_HZ);
    s->irq = qemu_allocate_irq(qutimer_set_irq, s, 0);
}

static Property hex_timer_properties[] = {
    DEFINE_PROP_UINT32("control", hex_timer_state, control, 0),
    DEFINE_PROP_UINT32("cnt_ctrl", hex_timer_state, cnt_ctrl, 0),
    DEFINE_PROP_UINT64("cntval", hex_timer_state, cntval, 0),
    DEFINE_PROP_UINT64("cntpct", hex_timer_state, cntpct, 0),
    DEFINE_PROP_UINT64("limit", hex_timer_state, limit, 1),
    DEFINE_PROP_INT32("int_level", hex_timer_state, int_level, 0),
    DEFINE_PROP_END_OF_LIST(),
};

static void hex_timer_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    k->realize = hex_timer_realize;
    device_class_set_props(k, hex_timer_properties);
    k->vmsd = &vmstate_hex_timer;
}

static const TypeInfo hex_timer_info = {
        .name          = TYPE_HexTIMER,
        .parent        = TYPE_SYS_BUS_DEVICE,
        .instance_size = sizeof(hex_timer_state),
        .instance_init = hex_instance_init,
        .class_init    = hex_timer_class_init,
};

static void hex_timer_timer_register_types(void)
{
    type_register_static(&hex_timer_info);
}

type_init(hex_timer_timer_register_types)
