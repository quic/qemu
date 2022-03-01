/*
 * libqemu
 *
 * Copyright (c) 2019 Luc Michel <luc.michel@greensocs.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/core/cpu.h"
#include "qemu/timer.h"
#include "qemu/coroutine.h"
#include "qemu/main-loop.h"
#include "qemu/rcu.h"
#include "tcg/tcg.h"
#include "block/aio.h"
#include "qapi/error.h"

#include "cpu.h"
#include "memory.h"

static __thread bool is_registered;
static __thread bool cpu_in_io;

static void stop_other_cpus(CPUState *cpu)
{
    CPUState *other_cpu;

    CPU_FOREACH(other_cpu) {
        other_cpu->soft_stopped = (cpu != other_cpu);
    }
}

void libqemu_cpu_loop(Object *obj)
{
    CPUState *cpu = CPU(obj);

    g_assert(cpu);
    g_assert(cpu->coroutine);

    if (libqemu_cpu_loop_is_busy(obj)) {
        return;
    }

    if (!qemu_tcg_mttcg_enabled()) {
        stop_other_cpus(cpu);
    }

    qemu_coroutine_enter(cpu->coroutine);

    while (cpu->coroutine_yield_info.reason == YIELD_IO) {
        cpu_in_io = true;
        libqemu_cpu_do_io();
        cpu_in_io = false;
        qemu_coroutine_enter(cpu->coroutine);
    }
}

bool libqemu_cpu_loop_is_busy(Object *obj)
{
    CPUState *cpu = CPU(obj);

    g_assert(cpu);

    return cpu_in_io || qemu_in_coroutine_cpu();
}

bool libqemu_cpu_can_run(Object *obj)
{
    CPUState *cpu = CPU(obj);

    g_assert(cpu);

    if (cpu->stop || !QSIMPLEQ_EMPTY(&cpu->work_list)) {
        return true;
    }

    if (cpu->stopped) {
        return false;
    }

    if (!cpu->halted || cpu_has_work(cpu)) {
        return true;
    }

    return false;
}

void libqemu_cpu_register_thread(Object *obj)
{
    CPUState *cpu = CPU(obj);

    g_assert(cpu);

    if (!is_registered) {
        rcu_register_thread();
        tcg_register_thread();
        is_registered = true;
    }

    qemu_thread_get_self(cpu->thread);
    cpu->thread_id = qemu_get_thread_id();
}

void libqemu_cpu_reset(Object *obj)
{
    cpu_reset(CPU(obj));
}

void libqemu_cpu_halt(Object *obj, bool halted)
{
    CPUState *cpu = CPU(obj);

    if (halted) {
        cpu->stop = true;
        qemu_cpu_kick(cpu);
    } else {
        cpu_resume(cpu);
    }
}


Object * libqemu_current_cpu_get(void)
{
    return OBJECT(current_cpu);
}

void libqemu_current_cpu_set(Object *cpu)
{
    current_cpu = CPU(cpu);
}

struct handler_arg {
    void (*handler)(void *);
    void *arg;
};

static void do_async_safe(CPUState *cpu, run_on_cpu_data data)
{
    struct handler_arg *ha = (struct handler_arg *)data.host_ptr;

    ha->handler(ha->arg);

    g_free(ha);
}

void libqemu_async_safe_run_on_cpu(Object *obj, void (*handler)(void *), void *arg)
{
    CPUState *cpu = CPU(obj);

    struct handler_arg *ha = g_new0(struct handler_arg, 1);
    ha->handler = handler;
    ha->arg = arg;

    async_safe_run_on_cpu(cpu, do_async_safe, RUN_ON_CPU_HOST_PTR(ha));
}
