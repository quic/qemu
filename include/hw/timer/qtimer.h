/*
 * Qualcomm QCT QTimer, L2VIC
 *
 * QEMU interface:
 * + sysbus MMIO regions 0: control registers
 * + sysbus irq[]
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
#ifndef QTIMER_TIMER_H
#define QTIMER_TIMER_H

#include "hw/ptimer.h"
#include "hw/sysbus.h"
#include "qemu/timer.h"
#define TYPE_Q_TIMER "Qtimer"
#define Q_TIMER(obj) OBJECT_CHECK(QTimerInfo, (obj), TYPE_Q_TIMER)
#define QTIMER_MEM_REGION_SIZE_BYTES 0x00001000

#define QTIMER_MAX_TIMERS 4

typedef struct QTimerInfo QTimerInfo;

typedef struct {
    qemu_irq irq;
    ptimer_state *timer;
    uint32_t index;
    QTimerInfo *info;
} QTimer;


struct QTimerInfo {
    SysBusDevice parent_obj;

    MemoryRegion iomem;

    uint32_t timer_count;
    QTimer timers[QTIMER_MAX_TIMERS];
    uint32_t irq_enabled_mask;
    uint32_t major_ver;
    uint32_t minor_ver;
};


#endif