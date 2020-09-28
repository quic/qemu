/*
 *  Copyright (c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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



#include <math.h>
#include "qemu/osdep.h"
#include "cpu.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#include "exec/helper-proto.h"
#else
#include "hw/boards.h"
#include "hw/hexagon/hexagon.h"
#endif
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "qemu/log.h"
#include "tcg/tcg-op.h"
#include "internal.h"
#include "macros.h"
#include "arch.h"
#include "fma_emu.h"
#include "conv_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros.h"
#ifndef CONFIG_USER_ONLY
#include "hex_mmu.h"
#endif
#include "op_helper.h"
#include "sysemu/runstate.h"


#ifndef CONFIG_USER_ONLY
static size1u_t hexagon_swi_mem_read1(CPUHexagonState *env, vaddr_t paddr)

{
    size1u_t data = 0;
    unsigned mmu_idx = cpu_mmu_index(env, false);
    data = cpu_ldub_mmuidx_ra(env, paddr, mmu_idx, GETPC());
    return data;
}

static size2u_t hexagon_swi_mem_read2(CPUHexagonState *env, vaddr_t paddr)

{
    size2u_t data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 2; i++) {
        tdata = hexagon_swi_mem_read1(env, paddr + i);
        data = ((size2u_t) tdata << (8 * i)) | data;
    }

    return data;
}

static target_ulong hexagon_swi_mem_read4(CPUHexagonState *env, vaddr_t paddr)

{
    target_ulong data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 4; i++) {
        tdata = hexagon_swi_mem_read1(env, paddr + i);
        data = ((target_ulong) tdata << (8 * i)) | data;
    }

    return data;
}


static size8u_t hexagon_swi_mem_read8(CPUHexagonState *env, vaddr_t paddr)

{
    size8u_t data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 8; i++) {
        tdata = hexagon_swi_mem_read1(env, paddr + i);
        data = ((size8u_t) tdata << (8 * i)) | data;
    }

    return data;
}

static void hexagon_swi_mem_write1(CPUHexagonState *env, vaddr_t paddr,
    size1u_t value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stb_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void hexagon_swi_mem_write2(CPUHexagonState *env, vaddr_t paddr,
    size2u_t value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stw_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void hexagon_swi_mem_write4(CPUHexagonState *env, vaddr_t paddr,
    target_ulong value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stl_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void hexagon_swi_mem_write8(CPUHexagonState *env, vaddr_t paddr,
    size8u_t value)
{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stq_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

void hexagon_tools_memory_read(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr)

{
    CPUState *cs = env_cpu(env);
    vaddr_t paddr = vaddr;

    switch (size) {
    case 1:
        (*(size1u_t *) retptr) = hexagon_swi_mem_read1(env, paddr);
        break;
    case 2:
        (*(size2u_t *) retptr) = hexagon_swi_mem_read2(env, paddr);
        break;
    case 4:
        (*(target_ulong *) retptr) = hexagon_swi_mem_read4(env, paddr);
        break;
    case 8:
        (*(size8u_t *) retptr) = hexagon_swi_mem_read8(env, paddr);
        break;
    default:
        cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __FUNCTION__, size);
    }
}

void hexagon_tools_memory_write(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data)

{
    CPUState *cs = env_cpu(env);
    paddr_t paddr = vaddr;

    switch (size) {
    case 1:
        hexagon_swi_mem_write1(env, paddr, (size1u_t) data);
        log_store32(env, paddr, (size4u_t)data, 1, 0);
        break;
    case 2:
        hexagon_swi_mem_write2(env, paddr, (size2u_t) data);
        log_store32(env, paddr, (size4u_t)data, 2, 0);
        break;
    case 4:
        hexagon_swi_mem_write4(env, paddr, (target_ulong) data);
        log_store32(env, paddr, (size4u_t)data, 4, 0);
        break;
    case 8:
        hexagon_swi_mem_write8(env, paddr, (size8u_t) data);
        log_store32(env, paddr, data, 8, 0);
        break;
    default:
        cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __FUNCTION__, size);
    }
}

int hexagon_tools_memory_read_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr)

{
    CPUState *cs = env_cpu(env);
    vaddr_t paddr = vaddr;
    int ret = 0;

    switch (size) {
    case 4:
        (*(target_ulong *) retptr) = hexagon_swi_mem_read4(env, paddr);
        break;
    case 8:
        (*(size8u_t *) retptr) = hexagon_swi_mem_read8(env, paddr);
        break;
    default:
        cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __FUNCTION__, size);
        break;
     }

    return ret;
}

int hexagon_tools_memory_write_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data)

{
    CPUState *cs = env_cpu(env);
    paddr_t paddr = vaddr;
    int ret = 0;

    switch (size) {
    case 4:
        hexagon_swi_mem_write4(env, paddr, (target_ulong) data);
        log_store32(env, vaddr, (size4u_t)data, 4, 0);
        break;
    case 8:
        hexagon_swi_mem_write8(env, paddr, (size8u_t) data);
        log_store64(env, vaddr, data, 8, 0);
        break;
    default:
        cpu_abort(cs, "%s: ERROR: bad size = %d!\n", __FUNCTION__, size);
        break;
    }

    return ret;
}

void hexagon_wait_thread(CPUHexagonState *env)

{
    if (get_exe_mode(env) != HEX_EXE_MODE_WAIT) {
        set_wait_mode(env);
    }
    ARCH_SET_THREAD_REG(env, HEX_REG_PC,
       ARCH_GET_THREAD_REG(env, HEX_REG_PC) + 4);
    cpu_stop_current();
}

void hexagon_resume_thread(CPUHexagonState *env, uint32_t ei)

{
    CPUState *cs = env_cpu(env);
    clear_wait_mode(env);
    cs = env_cpu(env);
    cs->exception_index = ei;
    cpu_resume(cs);
}

void hexagon_resume_threads(CPUHexagonState *current_env, uint32_t mask)

{
    int htid;

    for (htid = 0 ; htid < THREADS_MAX ; ++htid) {
        if (!(mask & (0x1 << htid))) {
            continue;
        }

        HexagonCPU *cpu;
        CPUHexagonState *env;
        CPUState *cs = env_cpu(current_env);
        bool found = false;
        CPU_FOREACH(cs) {
            cpu = HEXAGON_CPU(cs);
            env = &cpu->env;
            if (env->threadId == htid) {
                found = true;
                break;
            }
        }
        if (!found) {
            cpu_abort(cs,
                "Error: Hexagon: Illegal resume thread mask 0x%x", mask);
        }

        if (get_exe_mode(env) != HEX_EXE_MODE_WAIT) {
            // this thread not currently in wait mode
            continue;
        }
        hexagon_resume_thread(env, HEX_EVENT_NONE);
    }
}

void hexagon_start_threads(CPUHexagonState *current_env, uint32_t mask)

{
    int htid;

    for (htid = 0 ; htid < THREADS_MAX ; ++htid) {
        if (!(mask & (0x1 << htid))) {
            continue;
        }

        HexagonCPU *cpu;
        CPUHexagonState *env;
        CPUState *cs = env_cpu(current_env);
        bool found = false;
        CPU_FOREACH(cs) {
            cpu = HEXAGON_CPU(cs);
            env = &cpu->env;
            if (env->threadId == htid) {
                found = true;
                break;
            }
        }
        if (!found) {
            MachineState *machine = MACHINE(qdev_get_machine());
            cpu = HEXAGON_CPU(cpu_create(machine->cpu_type));
            env = &cpu->env;
            HEX_DEBUG_LOG("creating new cpu %p, tid %d, env %p\n",
                cpu, htid, env);
            env->cmdline = machine->kernel_cmdline;
            env->hex_tlb = current_env->hex_tlb;
            env->processor_ptr = current_env->processor_ptr;
            env->g_sreg = current_env->g_sreg;
            env->threadId = htid;
            ARCH_SET_SYSTEM_REG(env, HEX_SREG_HTID, htid);
            HEX_DEBUG_LOG("%s: mask 0x%x, cpu %p, g_sreg at %p\n",
                __FUNCTION__, mask, cpu, env->g_sreg);
        } else {
            HEX_DEBUG_LOG("%s: cpu %p/%d found\n",
                __FUNCTION__, cpu, env->threadId);
        }

        const uint32_t old = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_SSR, 0);
        SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, 1);
        SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, 0);
        const uint32_t new = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
        if (found) {
            hexagon_modify_ssr(env, new, old);
        }

        target_ulong evb = ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB);
        #if HEX_DEBUG
        target_ulong reset_inst;
        DEBUG_MEMORY_READ_ENV(env, evb, 4, &reset_inst);
        HEX_DEBUG_LOG("\ttid = %u, evb = 0x%x, reset = 0x%x, new PC = 0x%x\n",
            env->threadId, evb, reset_inst, evb);
        #endif

        //fCHECK_PCALIGN(addr);
        env->branch_taken = 1;
        env->next_PC = evb;

        ARCH_SET_THREAD_REG(env, HEX_REG_PC, env->next_PC);
        uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
        uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
        thread_enabled_mask |= 0x1 << env->threadId;
        SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_E,thread_enabled_mask);

        cs = env_cpu(env);
        cs->exception_index = HEX_EVENT_NONE;
        if (found) {
            cpu_resume(cs);
        }
    }
}

/*
 * When we have all threads stopped, the return
 * value to the shell is register 2 from thread 0.
 */
static target_ulong get_thread0_r2(void)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread = &cpu->env;
        if (thread->threadId == 0) {
            return thread->gpr[2];
        }
    }
    g_assert_not_reached();
}

void hexagon_stop_thread(CPUHexagonState *env)

{
    #if HEX_DEBUG
    HexagonCPU *cpu = hexagon_env_get_cpu(env);
    HEX_DEBUG_LOG("%s: htid %d, cpu %p\n",
        __FUNCTION__, ARCH_GET_SYSTEM_REG(env, HEX_SREG_HTID), cpu);
    #endif

    uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    thread_enabled_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_E,thread_enabled_mask);
    cpu_stop_current();
    if (!thread_enabled_mask) {
        /* All threads are stopped, exit */
        exit(get_thread0_r2());
    }
}

int sys_in_monitor_mode_ssr(uint32_t ssr);
int sys_in_monitor_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) != 0) ||
       ((GET_SSR_FIELD(SSR_EX, ssr) == 0) && (GET_SSR_FIELD(SSR_UM, ssr) == 0)))
        return 1;
    return 0;
}

int sys_in_monitor_mode(CPUHexagonState *env)
{
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return sys_in_monitor_mode_ssr(ssr);
}

int sys_in_guest_mode_ssr(uint32_t ssr);
int sys_in_guest_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) == 0) &&
        (GET_SSR_FIELD(SSR_UM, ssr) != 0) &&
        (GET_SSR_FIELD(SSR_GM, ssr) != 0))
        return 1;
    return 0;
}

int sys_in_guest_mode(CPUHexagonState *env)
{
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return sys_in_guest_mode_ssr(ssr);
}

int sys_in_user_mode_ssr(uint32_t ssr);
int sys_in_user_mode_ssr(uint32_t ssr)
{
    if ((GET_SSR_FIELD(SSR_EX, ssr) == 0) &&
        (GET_SSR_FIELD(SSR_UM, ssr) != 0) &&
        (GET_SSR_FIELD(SSR_GM, ssr) == 0))
        return 1;
   return 0;
}

int sys_in_user_mode(CPUHexagonState *env)
{
    uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    return sys_in_user_mode_ssr(ssr);
}

int get_cpu_mode(CPUHexagonState *env)

{
  // from table 4-2
  uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);

  if (sys_in_monitor_mode_ssr(ssr)) {
    return HEX_CPU_MODE_MONITOR;
  } else if (sys_in_guest_mode_ssr(ssr)) {
    return HEX_CPU_MODE_GUEST;
  } else if (sys_in_user_mode_ssr(ssr)) {
    return HEX_CPU_MODE_USER;
  }
  return HEX_CPU_MODE_MONITOR;
}

const char *get_sys_ssr_str(uint32_t ssr)

{
  if (sys_in_monitor_mode_ssr(ssr)) {
      return "monitor";
  } else if (sys_in_guest_mode_ssr(ssr)) {
      return "guest";
  } else if (sys_in_user_mode_ssr(ssr)) {
      return "user";
  }
  return "unknown";
}

const char *get_sys_str(CPUHexagonState *env)

{
  if (sys_in_monitor_mode(env)) {
      return "monitor";
  } else if (sys_in_guest_mode(env)) {
      return "guest";
  } else if (sys_in_user_mode(env)) {
      return "user";
  }
  return "unknown";
}

const char *get_exe_mode_str(CPUHexagonState *env)

{
    int exe_mode = get_exe_mode(env);

    if (exe_mode == HEX_EXE_MODE_OFF)
        return "off";
    else if (exe_mode == HEX_EXE_MODE_RUN)
        return "run";
    else if (exe_mode == HEX_EXE_MODE_WAIT)
        return "wait";
    else if (exe_mode == HEX_EXE_MODE_DEBUG)
        return "debug";
    return "unknown";
}

int get_exe_mode(CPUHexagonState *env)
{
    target_ulong modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
#if 1
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    bool E_bit = thread_enabled_mask & (0x1 << env->threadId);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    bool W_bit = thread_wait_mask & (0x1 << env->threadId);
#else
    bool E_bit = (modectl >> env->threadId) & 0x1;
    bool W_bit = (modectl >> (env->threadId + 16)) & 0x1;
#endif
    target_ulong isdbst = ARCH_GET_SYSTEM_REG(env, HEX_SREG_ISDBST);
    uint32_t debugmode = GET_FIELD(ISDBST_DEBUGMODE, isdbst);
    bool D_bit = debugmode & (0x1 << env->threadId);
//    bool D_bit = (isdbst >> (env->threadId + 8)) & 0x1;

    /* Figure 4-2 */
    if (!D_bit && !W_bit && !E_bit) {
        return HEX_EXE_MODE_OFF;
    }
    if (!D_bit && !W_bit && E_bit) {
        return HEX_EXE_MODE_RUN;
    }
    if (!D_bit && W_bit && E_bit) {
        return HEX_EXE_MODE_WAIT;
    }
    if (D_bit && !W_bit && E_bit) {
        return HEX_EXE_MODE_DEBUG;
    }
    g_assert_not_reached();
}

void set_wait_mode(CPUHexagonState *env)

{
    uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    thread_wait_mask |= 0x1 << env->threadId;
    SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_W,thread_wait_mask);
}

void clear_wait_mode(CPUHexagonState *env)

{
    uint32_t modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_wait_mask = GET_FIELD(MODECTL_W, modectl);
    thread_wait_mask &= ~(0x1 << env->threadId);
    SET_SYSTEM_FIELD(env,HEX_SREG_MODECTL,MODECTL_W,thread_wait_mask);
}

unsigned cpu_mmu_index(CPUHexagonState *env, bool ifetch)
{
    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    uint8_t mmuen = GET_SYSCFG_FIELD(SYSCFG_MMUEN, syscfg);
    if (!mmuen) {
      return MMU_KERNEL_IDX;
    }

    int cpu_mode = get_cpu_mode(env);
    if (cpu_mode == HEX_CPU_MODE_MONITOR) {
      return MMU_KERNEL_IDX;
    }
    else if (cpu_mode == HEX_CPU_MODE_GUEST) {
      return MMU_GUEST_IDX;
    }

    return MMU_USER_IDX;
}

void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause)
{
    const uint32_t old = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_EX, 1);
    SET_SYSTEM_FIELD(env, HEX_SREG_SSR, SSR_CAUSE, cause);
    const uint32_t new = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);

    hexagon_modify_ssr(env, new, old);
}

void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    target_ulong old_EX = GET_SSR_FIELD(SSR_EX, old);
    target_ulong old_UM = GET_SSR_FIELD(SSR_UM, old);
    target_ulong old_GM = GET_SSR_FIELD(SSR_GM, old);
    target_ulong new_EX = GET_SSR_FIELD(SSR_EX, new);
    target_ulong new_UM = GET_SSR_FIELD(SSR_UM, new);
    target_ulong new_GM = GET_SSR_FIELD(SSR_GM, new);

    if ((old_EX != new_EX) ||
        (old_UM != new_UM) ||
        (old_GM != new_GM)) {
        hex_mmu_mode_change(env);
    }

    uint8_t old_asid = GET_SSR_FIELD(SSR_ASID, old);
    uint8_t new_asid = GET_SSR_FIELD(SSR_ASID, new);
    if (new_asid != old_asid) {
        CPUState *cs = CPU(hexagon_env_get_cpu(env));
        tlb_flush(cs);
    }
}

#endif
