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
#include "reg_fields.h"
typedef struct CPUHexagonTLBContext CPUHexagonTLBContext;
#endif

#include "hex_arch_types.h"
#include "insn.h"
#include <fenv.h>

#ifdef CONFIG_USER_ONLY
#define TARGET_PAGE_BITS 16     /* 64K pages */
#else
#define TARGET_PAGE_BITS 12     /* 4K pages */
#endif
#define TARGET_LONG_BITS 32
#define NUM_TLB_REGS(PROC) NUM_TLB_ENTRIES

#include <time.h>
#include "qemu/compiler.h"
#include "qemu-common.h"
#include "exec/cpu-defs.h"
#include "hex_regs.h"
#include "mmvec/mmvec.h"
#include "hmx/hmx.h"
#include "dma.h"

#ifndef CONFIG_USER_ONLY
#include "cpu_helper.h"
#endif

#define NUM_PREGS 4
#define NUM_SREGS 64
#define NUM_GREGS 32
#ifdef CONFIG_USER_ONLY
#define TOTAL_PER_THREAD_REGS 64
#else
#define TOTAL_PER_THREAD_REGS 64
#endif

#define THREADS_MAX 4
#define SLOTS_MAX 4
#define STORES_MAX 2
#define REG_WRITES_MAX 32
#define GREG_WRITES_MAX 32
#define SREG_WRITES_MAX 64
#define PRED_WRITES_MAX 5                   /* 4 insns + endloop */
#define CACHE_TAGS_MAX 32
#define VSTORES_MAX 2

#define TYPE_HEXAGON_CPU "hexagon-cpu"

#define HMX_OUTPUT_DEPTH       32
#define HMX_BLOCK_SIZE         2048
#define HMX_V1                 0
#define HMX_SPATIAL_SIZE       6
#define HMX_CHANNEL_SIZE       5
#define HMX_ADDR_MASK          0xfffff800
#define HMX_BLOCK_BIT          11
#define HMXDEBUGLVL            2
#define VTCM_SIZE              0x40000LL
#define VTCM_OFFSET            0x200000LL

typedef struct {
  int unused;
} rev_features_t;

typedef struct ProcessorState processor_t;
typedef struct SystemState system_t;
typedef void (*hmx_mac_fxp_callback_t) (void * sys, processor_t * proc,
    int pktid, int row_idx, int col_idx, int acc_idx, int in_idx, int wgt,
    int act, int acc, int x_tap, int y_tap, int block_idx, int deep_block_idx);
typedef void (*hmx_mac_flt_callback_t) (void * sys, processor_t * proc,
    int pktid, int row_idx, int col_idx, int acc_idx, int in_idx, int wgt,
    int act, size8u_t acc_hi, size8u_t acc_lo, int ovf, int x_tap, int y_tap,
    int block_idx, int deep_block_idx);

typedef struct {
    paddr_t l2tcm_base;
    hmx_mac_fxp_callback_t hmx_mac_fxp_callback;
    hmx_mac_flt_callback_t hmx_mac_flt_callback;
} options_struct;

typedef struct arch_proc_opt {
    int hmx_output_depth;
    int hmx_block_size;
    int hmxarray_new;                 /* 0 */
    int hmx_v1;
    int hmx_power_config;             /* 0 */
    int dmadebug_verbosity;
    int hmx_spatial_size;
    int hmx_channel_size;
    int hmx_addr_mask;
    int hmx_block_bit;
    int hmxdebuglvl;
    FILE *hmxaccpreloadfile;
    FILE *hmxdebugfile;
    int pmu_enable;
    paddr_t vtcm_size;
    paddr_t vtcm_offset;
    FILE *dmadebugfile;
    int QDSP6_MX_CHANNELS;            /* 32 */
    int QDSP6_MX_RATE;                /* 4 */
    int QDSP6_DMA_PRESENT;            /* 1 */
    int QDSP6_MX_CVT_MPY_SZ;          /* 10 */
    int QDSP6_MX_ROWS;                /* 64 */
    int QDSP6_MX_COLS;                /* 32 */
    int QDSP6_MX_FP_PRESENT;          /* 1 */
    int QDSP6_MX_FP_ACC_INT;          /* 18 */
    int QDSP6_MX_FP_ACC_FRAC;         /* 46 */
    int QDSP6_MX_FP_RATE;             /* 2 */
    int QDSP6_MX_FP_ROWS;             /* 32 */
} arch_proc_opt_t;

enum phmx_e {
  phmx_nz_act_b,
  phmx_nz_wt_b,
  phmx_array_fxp_mpy0,
  phmx_array_fxp_mpy1,
  phmx_array_fxp_mpy2,
  phmx_array_fxp_mpy3,
  phmx_array_fxp_acc,
  phmx_array_flt_mpy0,
  phmx_array_flt_mpy1,
  phmx_array_flt_mpy2,
  phmx_array_flt_mpy3,
  phmx_array_flt_acc,
};

struct ProcessorState {
    const rev_features_t *features;
    const options_struct *options;
    const arch_proc_opt_t *arch_proc_options;
    target_ulong runnable_threads_max;
    target_ulong thread_system_mask;
    CPUHexagonState *thread[THREADS_MAX];

    /* Useful information of the DMA engine per thread. */
    struct dma_adapter_engine_info * dma_engine_info[THREADS_MAX];
    struct dma_state *dma[DMA_MAX]; // same as dma_t
    dma_insn_checker_ptr dma_insn_checker[DMA_MAX];

    /* one hmx unit shared among all threads */
    hmx_state_t *shared_extptr;
};

/*
 * Represents the maximum number of consecutive
 * translation blocks to execute on a CPU before yielding
 * to another CPU.
 */
#define HEXAGON_TB_EXEC_PER_CPU_MAX 2000

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

struct dma_state;
typedef uint32_t (*dma_insn_checker_ptr)(struct dma_state *);

typedef struct hmx_mem_access_info {
    size4s_t dY;
    size2u_t blocks;
    size4u_t fx;
    size4u_t fy;
    size4u_t x_offset;
    size4u_t y_offset;
    size4u_t y_start;
    size4u_t y_stop;
    size4u_t x_start;
    size4u_t x_stop;
    size1u_t y_tap;
    size1u_t x_tap;
    size1u_t ch_start;
    size1u_t ch_stop;
    size1u_t block_type:3;
    size1u_t format:2;
    size1u_t acc_select:1;
    size1u_t acc_range:1;
    size1u_t flt:1;
    size1u_t x_dilate:1;
    size1u_t y_dilate:1;
    size1u_t deep:1;
    size1u_t wgt_deep:1;
    size1u_t drop:1;
    size1u_t batch:1;
    size1s_t weight_count;
    size1u_t bias_32bit;
    size1u_t weight_bits;
    size1u_t enable16x16;
    size1u_t outputselect16x16;
    size1u_t act_reuse;
} hmx_mem_access_info_t;

#include "xlate_info.h"

typedef struct {
    vaddr_t vaddr;
    vaddr_t bad_vaddr;
    paddr_t paddr;
    size4u_t range;
    size8u_t stdata;
    int cancelled;
    int tnum;
    enum mem_access_types type;
    unsigned char cdata[512];
    size4u_t width;
    size4u_t page_cross_base;
    size4u_t page_cross_sum;
    size2u_t slot;
    size1u_t check_page_crosses;
    xlate_info_t xlate_info;
    hmx_mem_access_info_t hmx_ma;
    size1u_t is_dealloc:1;
    size1u_t is_memop:1;
    size1u_t valid:1;
    size1u_t log_as_tag:1;
    size1u_t no_deriveumaptr:1;
} mem_access_info_t;

#ifndef CONFIG_USER_ONLY
#define HEX_CPU_MODE_USER    1
#define HEX_CPU_MODE_GUEST   2
#define HEX_CPU_MODE_MONITOR 3

#define HEX_EXE_MODE_OFF     1
#define HEX_EXE_MODE_RUN     2
#define HEX_EXE_MODE_WAIT    3
#define HEX_EXE_MODE_DEBUG   4
#endif

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

/* TODO: Update for Hexagon: Meanings of the ARMCPU object's four inbound GPIO lines */
#define HEXAGON_CPU_IRQ_0 0
#define HEXAGON_CPU_IRQ_1 1
#define HEXAGON_CPU_IRQ_2 2
#define HEXAGON_CPU_IRQ_3 3
#define HEXAGON_CPU_IRQ_4 4
#define HEXAGON_CPU_IRQ_5 5
#define HEXAGON_CPU_IRQ_6 6
#define HEXAGON_CPU_IRQ_7 7

#ifndef CONFIG_USER_ONLY
typedef enum {
    HEX_LOCK_UNLOCKED       = 0,
    HEX_LOCK_WAITING        = 1,
    HEX_LOCK_OWNER          = 2
} hex_lock_state_t;

#endif

struct Einfo {
  size1u_t valid;
  size1u_t type;
  size1u_t cause;
  size1u_t bvs:1;
  size1u_t bv0:1;       /* valid for badva0 */
  size1u_t bv1:1;       /* valid for badva1 */
  size4u_t badva0;
  size4u_t badva1;
  size4u_t elr;
  size2u_t diag;
  size2u_t de_slotmask;
};
typedef struct Einfo exception_info;
typedef struct Instruction insn_t;
typedef unsigned systemstate_t;

struct CPUHexagonState {
    target_ulong gpr[TOTAL_PER_THREAD_REGS];
    target_ulong pred[NUM_PREGS];
    target_ulong branch_taken;
    target_ulong next_PC;
    target_ulong cause_code;

    /* For comparing with LLDB on target - see hack_stack_ptrs function */
    target_ulong stack_start;
    target_ulong stack_adjust;

    uint8_t slot_cancelled;
    target_ulong new_value[TOTAL_PER_THREAD_REGS];

    /* some system registers are per thread and some are global */
    target_ulong t_sreg[NUM_SREGS];
    target_ulong t_sreg_new_value[NUM_SREGS];
    target_ulong t_sreg_written[NUM_SREGS];
    target_ulong *g_sreg;

    target_ulong greg[NUM_GREGS];
    target_ulong greg_new_value[NUM_GREGS];
    target_ulong greg_written[NUM_GREGS];

    /*
     * Only used when HEX_DEBUG is on, but unconditionally included
     * to reduce recompile time when turning HEX_DEBUG on/off.
     */
    target_ulong this_PC;
    target_ulong reg_written[TOTAL_PER_THREAD_REGS];

    target_ulong new_pred_value[NUM_PREGS];
    target_ulong pred_written;

    struct MemLog mem_log_stores[STORES_MAX];

#ifndef CONFIG_USER_ONLY
    int slot;                    /* Needed for exception generation */
#endif

    target_ulong dczero_addr;

    fenv_t fenv;

    target_ulong llsc_addr;
    target_ulong llsc_val;
    uint64_t     llsc_val_i64;

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
    size1u_t bq_on;

    mmvector_t temp_vregs[TEMP_VECTORS_MAX];
    mmqreg_t temp_qregs[TEMP_VECTORS_MAX];

    target_ulong cache_tags[CACHE_TAGS_MAX];
    unsigned int timing_on;

    systemstate_t systemstate;
    systemstate_t *system_ptr;
    insn_t insn_env;
    size8u_t pktid;
    unsigned int threadId;
    processor_t *processor_ptr;
#ifndef CONFIG_USER_ONLY
    const char *cmdline;
    CPUHexagonTLBContext *hex_tlb;
    target_ulong imprecise_exception;
    hex_lock_state_t tlb_lock_state;
    hex_lock_state_t k0_lock_state;
    uint16_t nmi_threads;
    uint32_t last_cpu;
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

#ifndef CONFIG_USER_ONLY
    bool count_gcycle_xt;
#endif
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
#else
    *flags = cpu_mmu_index(env, false);
#endif
}


#ifndef CONFIG_USER_ONLY

/*
 * Fill @a ints with the interrupt numbers that are currently asserted.
 * @param list_size will be written with the count of interrupts found.
 */
void hexagon_find_asserted_interrupts(CPUHexagonState *env, uint32_t *ints,
                                      size_t list_capacity, size_t *list_size);
void hexagon_find_int_threads(CPUHexagonState *env, uint32_t int_num,
                              HexagonCPU *threads[], size_t *list_size);
HexagonCPU *hexagon_find_lowest_prio(CPUHexagonState *env, uint32_t int_num);
HexagonCPU *hexagon_find_lowest_prio_any_thread(HexagonCPU *threads[],
                                                size_t list_size,
                                                uint32_t int_num,
                                                uint32_t *low_prio);
HexagonCPU *hexagon_find_lowest_prio_waiting_thread(HexagonCPU *threads[],
                                                    size_t list_size,
                                                    uint32_t int_num,
                                                    uint32_t *low_prio);

/*
 * @return true if @a thread_env is in an interruptible state.
 */
bool hexagon_thread_is_interruptible(CPUHexagonState *thread_env, uint32_t int_num);

/*
 * @return true if the @a thread_env hardware thread has interrupts
 * enabled.
 */
bool hexagon_thread_ints_enabled(CPUHexagonState *thread_env);

/*
 * @return true if interrupt number @a int_num is disabled.
 */
bool hexagon_int_disabled(CPUHexagonState *global_env, uint32_t int_num);

/*
 * Disable interrupt number @a int_num for the @a thread_env hardware thread.
 */
void hexagon_disable_int(CPUHexagonState *global_env, uint32_t int_num);

/*
 * Set the interrupt pending bits in the mask.
 */
void hexagon_set_interrupts(CPUHexagonState *global_env, uint32_t mask);

/*
 * Get the interrupt pending bits.
 */
uint32_t hexagon_get_interrupts(CPUHexagonState *global_env);

/*
 * @return true if thread_env is busy with an interrupt or one is
 * queued.
 */
bool hexagon_thread_is_busy(CPUHexagonState *thread_env);

/*
 * @return pointer to the lowest priority thread.
 * @a only_waiters if true, only consider threads in the WAIT state.
 */
HexagonCPU *hexagon_find_lowest_prio_thread(HexagonCPU *threads[],
                                            size_t list_size,
                                            uint32_t int_num,
                                            bool only_waiters,
                                            uint32_t *low_prio);
void hexagon_raise_interrupt_resume(CPUHexagonState *env, HexagonCPU *thread,
                                    uint32_t int_num, uint32_t vid_int_pending,
                                    uint32_t resume_pc);

void hexagon_raise_interrupt(CPUHexagonState *env, HexagonCPU *thread,
                             uint32_t int_num, uint32_t vid_int_pending);

uint32_t hexagon_greg_read(CPUHexagonState *env, uint32_t reg);
#endif

typedef struct CPUHexagonState CPUArchState;
typedef HexagonCPU ArchCPU;

void hexagon_translate_init(void);

extern void hexagon_cpu_do_interrupt(CPUState *cpu);
extern void register_trap_exception(CPUHexagonState *env, uintptr_t next_pc, int traptype, int imm);

#include "exec/cpu-all.h"

#endif /* HEXAGON_CPU_H */
