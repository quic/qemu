/*
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

#ifndef HEXAGON_CPU_H
#define HEXAGON_CPU_H

/* Forward declaration needed by some of the header files */
typedef struct CPUHexagonState CPUHexagonState;
#ifndef CONFIG_USER_ONLY
typedef struct CPUHexagonTLBContext CPUHexagonTLBContext;
#endif

#include <fenv.h>

#ifdef CONFIG_USER_ONLY
#define TARGET_PAGE_BITS 16     /* 64K pages */
#else
#define TARGET_PAGE_BITS 12     /* 4K pages */
#endif
#define TARGET_LONG_BITS 32

#include "qemu/compiler.h"
#include "qemu-common.h"
#include "exec/cpu-defs.h"
#include "hex_regs.h"
#include "mmvec/mmvec.h"

#define NUM_PREGS 4
#define NUM_SREGS 64
#ifdef CONFIG_USER_ONLY
#define TOTAL_PER_THREAD_REGS 64
#else
#define TOTAL_PER_THREAD_REGS 64
#endif

#define SLOTS_MAX 4
#define STORES_MAX 2
#define REG_WRITES_MAX 32
#define SREG_WRITES_MAX 64
#define PRED_WRITES_MAX 5                   /* 4 insns + endloop */
#define CACHE_TAGS_MAX 32
#define VSTORES_MAX 2

#define TYPE_HEXAGON_CPU "hexagon-cpu"

#define HEXAGON_CPU_TYPE_SUFFIX "-" TYPE_HEXAGON_CPU
#define HEXAGON_CPU_TYPE_NAME(name) (name HEXAGON_CPU_TYPE_SUFFIX)
#define CPU_RESOLVING_TYPE TYPE_HEXAGON_CPU

#define TYPE_HEXAGON_CPU_V67 HEXAGON_CPU_TYPE_NAME("v67")

struct MemLog {
    target_ulong va;
    uint8_t width;
    uint32_t data32;
    uint64_t data64;
};

typedef struct {
    target_ulong va;
    int size;
    mmvector_t mask;
    mmvector_t data;
} vstorelog_t;

typedef struct {
    unsigned char cdata[256];
    uint32_t range;
    uint8_t format;
} mem_access_info_t;

#define HEX_CPU_MODE_USER    1
#define HEX_CPU_MODE_GUEST   2
#define HEX_CPU_MODE_MONITOR 3
#define MMU_USER_IDX         0
#define MMU_GUEST_IDX        1
#define MMU_KERNEL_IDX       2

#define EXEC_STATUS_OK          0x0000
#define EXEC_STATUS_STOP        0x0002
#define EXEC_STATUS_REPLAY      0x0010
#define EXEC_STATUS_LOCKED      0x0020
#define EXEC_STATUS_EXCEPTION   0x0100


#define EXCEPTION_DETECTED      (env->status & EXEC_STATUS_EXCEPTION)
#define REPLAY_DETECTED         (env->status & EXEC_STATUS_REPLAY)
#define CLEAR_EXCEPTION         (env->status &= (~EXEC_STATUS_EXCEPTION))
#define SET_EXCEPTION           (env->status |= EXEC_STATUS_EXCEPTION)

/* This needs to be large enough for all the reads and writes in a packet */
#define TEMP_VECTORS_MAX        25

enum {
  SYSCFG_M       = 1u << 0,
  SYSCFG_I       = 1u << 1,
  SYSCFG_D       = 1u << 2,
  SYSCFG_T       = 1u << 3,
  SYSCFG_G       = 1u << 4,
  SYSCFG_R       = 1u << 5,
  SYSCFG_C       = 1u << 6,
  SYSCFG_V2X     = 1u << 7,
  SYSCFG_IDA     = 1u << 8,
  SYSCFG_PM      = 1u << 9,
  SYSCFG_TL      = 1u << 11,
  SYSCFG_KL      = 1u << 12,
  SYSCFG_BQ      = 1u << 13,
  SYSCFG_PRIO    = 1u << 14,
  SYSCFG_DMT     = 1u << 15,
  SYSCFG_L2CFG   = 7u << 16,
  SYSCFG_ITCM    = 1u << 19,
  SYSCFG_CCE     = 1u << 20,
  SYSCFG_L2NWA   = 1u << 21,
  SYSCFG_L2NRA   = 1u << 22,
  SYSCFG_L2WB    = 1u << 23,
  SYSCFG_L2P     = 1u << 24,
  SYSCFG_SLVCTL0 = 3u << 25,
  SYSCFG_SLVCTL1 = 3u << 27,
  SYSCFG_L2PART  = 3u << 29,
  SYSCFG_L2GCA   = 1u << 31,
};

enum {
  SSR_CAUSE = 0xFFu << 0,
  SSR_ASID  = 0x7Fu << 8,
  SSR_UM    = 1u << 16,
  SSR_EX    = 1u << 17,
  SSR_IE    = 1u << 18,
  SSR_GM    = 1u << 19,
  SSR_V0    = 1u << 20,
  SSR_V1    = 1u << 21,
  SSR_BVS   = 1u << 22,
  SSR_CE    = 1u << 23,
  SSR_PE    = 1u << 24,
  SSR_BP    = 1u << 25,
  SSR_XE2   = 1u << 26,
  SSR_XA    = 7u << 27,
  SSR_SS    = 1u << 30,
  SSR_XE    = 1u << 31,
};

struct CPUHexagonState {
    target_ulong gpr[TOTAL_PER_THREAD_REGS];
    target_ulong pred[NUM_PREGS];
    target_ulong branch_taken;
    target_ulong next_PC;

    /* For comparing with LLDB on target - see hack_stack_ptrs function */
    target_ulong stack_start;
    target_ulong stack_adjust;

    uint8_t slot_cancelled;
    target_ulong new_value[TOTAL_PER_THREAD_REGS];

    target_ulong sreg[NUM_SREGS];
    target_ulong new_sreg_value[NUM_SREGS];
    target_ulong sreg_written[NUM_SREGS];

    /*
     * Only used when HEX_DEBUG is on, but unconditionally included
     * to reduce recompile time when turning HEX_DEBUG on/off.
     */
    target_ulong this_PC;
    target_ulong reg_written[TOTAL_PER_THREAD_REGS];

    target_ulong new_pred_value[NUM_PREGS];
    target_ulong pred_written;

    struct MemLog mem_log_stores[STORES_MAX];

    target_ulong dczero_addr;

    fenv_t fenv;

    target_ulong llsc_addr;
    target_ulong llsc_val;
    uint64_t     llsc_val_i64;
    target_ulong llsc_newval;
    uint64_t     llsc_newval_i64;
    target_ulong llsc_reg;

    target_ulong is_gather_store_insn;
    target_ulong gather_issued;

    mmvector_t VRegs[NUM_VREGS];
    mmvector_t future_VRegs[NUM_VREGS];
    mmvector_t tmp_VRegs[NUM_VREGS];

    VRegMask VRegs_updated_tmp;
    VRegMask VRegs_updated;
    VRegMask VRegs_select;

    mmqreg_t QRegs[NUM_QREGS];
    mmqreg_t future_QRegs[NUM_QREGS];
    QRegMask QRegs_updated;

    vstorelog_t vstore[VSTORES_MAX];
    uint8_t store_pending[VSTORES_MAX];
    uint8_t vstore_pending[VSTORES_MAX];
    uint8_t vtcm_pending;
    vtcm_storelog_t vtcm_log;
    mem_access_info_t mem_access[SLOTS_MAX];

    int status;

    mmvector_t temp_vregs[TEMP_VECTORS_MAX];
    mmqreg_t temp_qregs[TEMP_VECTORS_MAX];

    target_ulong cache_tags[CACHE_TAGS_MAX];
    int timing_on;
    const char *cmdline;
#ifndef CONFIG_USER_ONLY
    CPUHexagonTLBContext *hex_tlb;
    target_ulong imprecise_exception;
#endif
};

#define HEXAGON_CPU_CLASS(klass) \
    OBJECT_CLASS_CHECK(HexagonCPUClass, (klass), TYPE_HEXAGON_CPU)
#define HEXAGON_CPU(obj) \
    OBJECT_CHECK(HexagonCPU, (obj), TYPE_HEXAGON_CPU)
#define HEXAGON_CPU_GET_CLASS(obj) \
    OBJECT_GET_CLASS(HexagonCPUClass, (obj), TYPE_HEXAGON_CPU)

typedef struct HexagonCPUClass {
    /*< private >*/
    CPUClass parent_class;
    /*< public >*/
    DeviceRealize parent_realize;
    DeviceReset parent_reset;
} HexagonCPUClass;

typedef struct HexagonCPU {
    /*< private >*/
    CPUState parent_obj;
    /*< public >*/
    CPUNegativeOffsetState neg;
    CPUHexagonState env;
} HexagonCPU;

static inline HexagonCPU *hexagon_env_get_cpu(CPUHexagonState *env)
{
    return container_of(env, HexagonCPU, env);
}

#include "cpu_bits.h"

#define cpu_signal_handler cpu_hexagon_signal_handler
extern int cpu_hexagon_signal_handler(int host_signum, void *pinfo, void *puc);

static inline void cpu_get_tb_cpu_state(CPUHexagonState *env, target_ulong *pc,
                                        target_ulong *cs_base, uint32_t *flags)
{
    *pc = env->gpr[HEX_REG_PC];
    *cs_base = 0;
#ifdef CONFIG_USER_ONLY
    *flags = 0;
#endif
}


#ifndef CONFIG_USER_ONLY

#define GET_SSR_FIELD(BIT) (env->sreg[HEX_SREG_SSR] & (BIT))
static inline int sys_in_monitor_mode(CPUHexagonState *env)
{
    return ((GET_SSR_FIELD(SSR_UM) == 0)
         || (GET_SSR_FIELD(SSR_EX) != 0));
}

static inline int sys_in_guest_mode(CPUHexagonState *env)
{
  if (sys_in_monitor_mode(env)) return 0;

  if (GET_SSR_FIELD(SSR_GM)) return 1;
  else return 0;
}

static inline int sys_in_user_mode(CPUHexagonState *env)
{
  if (sys_in_monitor_mode(env)) return 0;
  if (GET_SSR_FIELD(SSR_GM)) return 0;
  else return 1;
}

static inline int get_cpu_mode(CPUHexagonState *env)

{
  // from table 4-2
#if 0
  printf("%s:%d: SSR 0x%x: EX %d, GM %d, UM %d\n",
    __FUNCTION__, __LINE__,
    env->sreg[HEX_SREG_SSR],
    (env->sreg[HEX_SREG_SSR] & SSR_EX),
    (env->sreg[HEX_SREG_SSR] & SSR_GM),
    (env->sreg[HEX_SREG_SSR] & SSR_UM));
#endif

  if (GET_SSR_FIELD(SSR_EX)) {
    return HEX_CPU_MODE_MONITOR;
  } else if (GET_SSR_FIELD(SSR_GM) && GET_SSR_FIELD(SSR_UM)) {
    return HEX_CPU_MODE_GUEST;
  } else if (GET_SSR_FIELD(SSR_UM)) {
    return HEX_CPU_MODE_USER;
  }
  return HEX_CPU_MODE_MONITOR;
}

static inline int cpu_mmu_index(CPUHexagonState *env, bool ifetch)
{
  if (!(env->sreg[HEX_SREG_SYSCFG] & SYSCFG_M)) {
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
#endif

typedef struct CPUHexagonState CPUArchState;
typedef HexagonCPU ArchCPU;

void hexagon_translate_init(void);

extern void hexagon_cpu_do_interrupt(CPUState *cpu);
extern void register_trap_exception(CPUHexagonState *env, uintptr_t next_pc, int traptype, int imm);

#include "exec/cpu-all.h"

#endif /* HEXAGON_CPU_H */
