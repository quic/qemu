/*
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "qemu/log.h"
#include "qemu/main-loop.h"
#include "cpu.h"
#include "hex_interrupts.h"
#include "macros.h"
#include "sys_macros.h"

static bool get_syscfg_gie(CPUHexagonState *env)
{
    target_ulong syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    return GET_SYSCFG_FIELD(SYSCFG_GIE, syscfg);
}

static bool get_ssr_ex(CPUHexagonState *env)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return GET_SSR_FIELD(SSR_EX, ssr);
}

static bool get_ssr_ie(CPUHexagonState *env)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return GET_SSR_FIELD(SSR_IE, ssr);
}

static inline uint32_t get_ssr_cause(CPUHexagonState *env)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return GET_SSR_FIELD(SSR_CAUSE, ssr);
}

/* Do these together so we only have to call hexagon_modify_ssr once */
static void set_ssr_ex_cause(CPUHexagonState *env, int ex, uint32_t cause)
{
    target_ulong old = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, ex);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, cause);
    target_ulong new = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    hexagon_modify_ssr(env, new, old);
}

static bool get_iad_bit(CPUHexagonState *env, int int_num)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong iad = GET_FIELD(IPENDAD_IAD, ipendad);
    return extract32(iad, int_num, 1);
}

static void set_iad_bit(CPUHexagonState *env, int int_num, int val)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong iad = GET_FIELD(IPENDAD_IAD, ipendad);
    iad = deposit32(iad, int_num, 1, val);
    fSET_FIELD(ipendad, IPENDAD_IAD, iad);
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_IPENDAD, ipendad);
}

static uint32_t get_ipend(CPUHexagonState *env)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    return GET_FIELD(IPENDAD_IPEND, ipendad);
}

static inline bool get_ipend_bit(CPUHexagonState *env, int int_num)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong ipend = GET_FIELD(IPENDAD_IPEND, ipendad);
    return extract32(ipend, int_num, 1);
}

static void set_ipend(CPUHexagonState *env, uint32_t mask)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong ipend = GET_FIELD(IPENDAD_IPEND, ipendad);
    ipend |= mask;
    fSET_FIELD(ipendad, IPENDAD_IPEND, ipend);
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_IPENDAD, ipendad);
}

static void set_ipend_bit(CPUHexagonState *env, int int_num, int val)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong ipend = GET_FIELD(IPENDAD_IPEND, ipendad);
    ipend = deposit32(ipend, int_num, 1, val);
    fSET_FIELD(ipendad, IPENDAD_IPEND, ipend);
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_IPENDAD, ipendad);
}

static bool get_imask_bit(CPUHexagonState *env, int int_num)
{
    target_ulong imask = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IMASK);
    return extract32(imask, int_num, 1);
}

static uint32_t get_prio(CPUHexagonState *env)
{
    target_ulong stid = ARCH_GET_SYSTEM_REG(env, HEX_SREG_STID);
    return extract32(stid, reg_field_info[STID_PRIO].offset,
                     reg_field_info[STID_PRIO].width);
}

static void set_elr(CPUHexagonState *env, target_ulong val)
{
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_ELR, val);
}

static bool get_schedcfgen(CPUHexagonState *env)
{
    target_ulong schedcfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SCHEDCFG);
    return extract32(schedcfg, reg_field_info[SCHEDCFG_EN].offset,
                     reg_field_info[SCHEDCFG_EN].width);
}

static bool is_lowest_prio(CPUHexagonState *env, int int_num)
{
    uint32_t my_prio = get_prio(env);
    CPUState *cs;

    CPU_FOREACH(cs) {
        HexagonCPU *hex_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *hex_env = &hex_cpu->env;
        if (!hex_is_qualified_for_int(hex_env, int_num)) {
            continue;
        }

        /* Note that lower values indicate *higher* priority */
        if (my_prio < get_prio(hex_env)) {
            return false;
        }
    }
    return true;
}

bool hex_is_qualified_for_int(CPUHexagonState *env, int int_num)
{
    bool syscfg_gie = get_syscfg_gie(env);
    bool iad = get_iad_bit(env, int_num);
    bool ssr_ie = get_ssr_ie(env);
    bool ssr_ex = get_ssr_ex(env);
    bool imask = get_imask_bit(env, int_num);

    return syscfg_gie && !iad && ssr_ie && !ssr_ex && !imask;
}

void hex_accept_int(CPUHexagonState *env, int int_num)
{
    CPUState *cs = env_cpu(env);
    target_ulong evb = ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB);

    set_ipend_bit(env, int_num, 0);
    set_iad_bit(env, int_num, 1);
    set_ssr_ex_cause(env, 1, HEX_CAUSE_INT0 | int_num);
    cs->exception_index = HEX_EVENT_INT0 + int_num;
    env->cause_code = HEX_EVENT_INT0 + int_num;
    set_elr(env, env->gpr[HEX_REG_PC]);
    env->gpr[HEX_REG_PC] = evb | (cs->exception_index << 2);
    if (get_ipend(env) == 0) {
        cs->interrupt_request &= ~(CPU_INTERRUPT_HARD | CPU_INTERRUPT_SWI);
    }
}

bool hex_check_interrupts(CPUHexagonState *env)
{
    bool retval = false;
    int max_ints;
    bool need_lock;
    bool schedcfgen;

    /* Early exit if nothing pending */
    if (get_ipend(env) == 0) {
        CPUState *cs = env_cpu(env);
        cs->interrupt_request &= ~(CPU_INTERRUPT_HARD | CPU_INTERRUPT_SWI);
        return false;
    }

    max_ints = reg_field_info[IPENDAD_IPEND].width;
    need_lock = !qemu_mutex_iothread_locked();
    if (need_lock) {
        qemu_mutex_lock_iothread();
    }
    /* Only check priorities when schedcfgen is set */
    schedcfgen = get_schedcfgen(env);
    for (int i = 0; i < max_ints; i++) {
        if (!get_iad_bit(env, i) && get_ipend_bit(env, i)) {
            if (hex_is_qualified_for_int(env, i) &&
                (!schedcfgen || is_lowest_prio(env, i))) {
                hex_accept_int(env, i);
                retval = true;
                break;
            }
        }
    }
    if (need_lock) {
        qemu_mutex_unlock_iothread();
    }

    return retval;
}

void hex_raise_interrupts(CPUHexagonState *env, uint32_t mask, uint32_t type)
{
    bool need_lock;
    CPUState *cs;

    if (mask == 0) {
        return;
    }

    /*
     * Notify all CPUs that the interrupt has happened
     */
    need_lock = !qemu_mutex_iothread_locked();
    if (need_lock) {
        qemu_mutex_lock_iothread();
    }
    CPU_FOREACH(cs) {
        cs->interrupt_request |= type;
    }

    set_ipend(env, mask);

    if (need_lock) {
        qemu_mutex_unlock_iothread();
    }

    if (get_exe_mode(env) == HEX_EXE_MODE_WAIT) {
        qemu_log_mask(CPU_LOG_INT, "%s: thread %d in WAIT mode\n",
                      __func__, env->threadId);
        CPUState *cs = env_cpu(env);
        clear_wait_mode(env);
        cpu_resume(cs);
    }
}
