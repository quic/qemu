/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
typedef struct ProcessorState processor_t;
typedef struct SystemState system_t;

#include "fpu/softfloat-types.h"

#include "qemu-common.h"
#include "exec/cpu-defs.h"
#include "hex_regs.h"
#include "mmvec/mmvec.h"
#include "hmx/hmx.h"
#include "dma/dma.h"

extern unsigned cpu_mmu_index(CPUHexagonState *env, bool ifetch);
#ifndef CONFIG_USER_ONLY
#include "cpu_helper.h"
#include "reg_fields.h"
typedef struct CPUHexagonTLBContext CPUHexagonTLBContext;
#define NUM_SREGS 64
#define NUM_GREGS 32
#define GREG_WRITES_MAX 32
#define SREG_WRITES_MAX 64
#define NUM_TLB_REGS(PROC) NUM_TLB_ENTRIES
#endif

#define TARGET_LONG_BITS 32

/* Hexagon processors have a strong memory model.
*/
#define TCG_GUEST_DEFAULT_MO      (TCG_MO_ALL)

#define NUM_PREGS 4
#define TOTAL_PER_THREAD_REGS 64

#define SLOTS_MAX 4
#define STORES_MAX 2
#define REG_WRITES_MAX 32
#define PRED_WRITES_MAX 5                   /* 4 insns + endloop */
#define VSTORES_MAX 2
#define THREADS_MAX 8
#define VECTOR_UNIT_MAX 4

#define TYPE_HEXAGON_CPU "hexagon-cpu"

#define HMX_MAX_EGY_CYCLE      512
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

#ifndef CONFIG_USER_ONLY
/*
 * Represents the maximum number of consecutive
 * translation blocks to execute on a CPU before yielding
 * to another CPU.
 */
#define HEXAGON_TB_EXEC_PER_CPU_MAX 2000
#endif

#define HEXAGON_CPU_TYPE_SUFFIX "-" TYPE_HEXAGON_CPU
#define HEXAGON_CPU_TYPE_NAME(name) (name HEXAGON_CPU_TYPE_SUFFIX)
#define CPU_RESOLVING_TYPE TYPE_HEXAGON_CPU

#define TYPE_HEXAGON_CPU_ANY HEXAGON_CPU_TYPE_NAME("any")
#define TYPE_HEXAGON_CPU_V67 HEXAGON_CPU_TYPE_NAME("v67")


typedef struct {
  int unused;
} rev_features_t;

typedef void (*hmx_mac_fxp_callback_t) (void * sys, processor_t * proc,
    int pktid, int row_idx, int col_idx, int acc_idx, int in_idx, int wgt,
    int act, int acc, int x_tap, int y_tap, int block_idx, int deep_block_idx);
typedef void (*hmx_mac_flt_callback_t) (void *, ...);
typedef void (*hmx_cvt_fxp_state_callback_t) (system_t *sys,
                                              processor_t *proc,
                                              uint32_t pktid,
                                              uint32_t row_idx,
                                              uint32_t col_idx,
                                              uint32_t acc_idx,
                                              uint32_t val);
typedef void (*hmx_cvt_state_transfer_callback_t) (void *, ...);
typedef void (*hmx_cvt_state_write_callback_t)(system_t *sys,
                                               processor_t *proc,
                                               uint32_t pktid, uint32_t age,
                                               uint32_t row_idx,
                                               uint32_t col_idx,
                                               uint32_t acc_idx,
                                               uint32_t val);
typedef void (*hmx_wgt_decomp_callback_t)(system_t *sys, processor_t *proc,
                                          int block_idx, int pktid,
                                          int lane_idx, int vector_idx,
                                          uint64_t metadata_addr,
                                          int meta_addr_valid,
                                          uint16_t meta_16bits,
                                          uint64_t val_hi_8_bytes,
                                          uint64_t val_lo_8_bytes);

typedef struct {
    uint64_t l2tcm_base;
    hmx_mac_fxp_callback_t hmx_mac_fxp_callback;
    hmx_mac_flt_callback_t hmx_mac_flt_callback;
	hmx_cvt_fxp_state_callback_t hmx_cvt_fxp_state_callback;
    hmx_cvt_state_transfer_callback_t hmx_cvt_state_transfer_callback;
    hmx_cvt_state_write_callback_t hmx_cvt_state_write_callback;
    hmx_wgt_decomp_callback_t hmx_wgt_decomp_callback;
} options_struct;

typedef struct arch_proc_opt {
    int hmxdebuglvl;
    int hmx_output_depth;
    int hmx_spatial_size;
    int hmx_channel_size;
    int hmx_block_size;
    int hmx_mxmem_debug_acc_preload;
    int hmx_mac_channels;
    int pmu_enable;
    FILE *hmxmpytrace;
    FILE *hmxdebugfile;
    FILE *dmadebugfile;
    int hmx_mxmem_debug;
    FILE *hmxaccpreloadfile;
    int hmxarray_new;
    int hmx_v1;
    int hmx_power_config;
    int hmx_8x4_mpy_mode;
    int hmx_group_conv_mode;
    int dmadebug_verbosity;
    int xfp_inexact_enable;
    int xfp_cvt_frac;
    int xfp_cvt_int;
    uint64_t vtcm_size;
    uint64_t vtcm_offset;
    int vtcm_original_mem_entries;
    int QDSP6_DMA_PRESENT;
    int QDSP6_DMA_EXTENDED_VA_PRESENT;
    int QDSP6_MX_FP_PRESENT;
    int QDSP6_MX_RATE;
    int QDSP6_MX_CHANNELS;
    int QDSP6_MX_ROWS;
    int QDSP6_MX_COLS;
    int QDSP6_MX_CVT_MPY_SZ;
    int QDSP6_MX_SUB_COLS;
    int QDSP6_MX_ACCUM_WIDTH;
    int QDSP6_MX_CVT_WIDTH;
    int QDSP6_MX_FP_RATE;
    int QDSP6_MX_FP_ACC_INT;
    int QDSP6_MX_FP_ACC_FRAC;
    int QDSP6_MX_FP_ACC_EXP;
    int QDSP6_MX_FP_ROWS;
    int QDSP6_MX_FP_COLS;
    int QDSP6_MX_FP_ACC_NORM;
    int QDSP6_MX_PARALLEL_GRPS;
    int QDSP6_VX_PRESENT;
    int QDSP6_VX_CONTEXTS;
    int QDSP6_VX_MEM_ENTRIES;
    int QDSP6_VX_VEC_SZ;
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
  phmx_array_fxp_cvt,
  phmx_array_flt_cvt,
};

enum mem_access_types {
	access_type_INVALID = 0,
	access_type_unknown = 1,
	access_type_load = 2,
	access_type_store = 3,
	access_type_fetch = 4,
	access_type_dczeroa = 5,
	access_type_dccleana = 6,
	access_type_dcinva = 7,
	access_type_dccleaninva = 8,
	access_type_icinva = 9,
	access_type_ictagr = 10,
	access_type_ictagw = 11,
	access_type_icdatar = 12,
	access_type_dcfetch = 13,
	access_type_l2fetch = 14,
	access_type_l2cleanidx = 15,
	access_type_l2cleaninvidx = 16,
	access_type_l2tagr = 17,
	access_type_l2tagw = 18,
	access_type_dccleanidx = 19,
	access_type_dcinvidx = 20,
	access_type_dccleaninvidx = 21,
	access_type_dctagr = 22,
	access_type_dctagw = 23,
	access_type_k0unlock = 24,
	access_type_l2locka = 25,
	access_type_l2unlocka = 26,
	access_type_l2kill = 27,
	access_type_l2gclean = 28,
	access_type_l2gcleaninv = 29,
	access_type_l2gunlock = 30,
	access_type_synch = 31,
	access_type_isync = 32,
	access_type_pause = 33,
	access_type_load_phys = 34,
	access_type_load_locked = 35,
	access_type_store_conditional = 36,
	access_type_barrier = 37,
#ifdef CLADE
	access_type_clade = 38,
#endif
	access_type_memcpy_load = 39,
	access_type_memcpy_store = 40,
#ifdef CLADE2
	access_type_clade2 = 41,
#endif
  access_type_hmx_load_act = 42,
  access_type_hmx_load_wei = 43,
  access_type_hmx_load_bias = 44,
  access_type_hmx_store = 45,
  access_type_hmx_store_bias = 46,
  access_type_hmx_swap_acc = 47,
  access_type_hmx_acc_cvt = 48,
  access_type_hmx_store_cvt_state = 49,
  access_type_hmx_poly_cvt = 50,
  access_type_udma_load = 51,
  access_type_udma_store = 52,
  access_type_unpause = 53,
	NUM_CORE_ACCESS_TYPES
};

typedef struct {
    target_ulong va;
    uint8_t width;
    uint32_t data32;
    uint64_t data64;
} MemLog;

typedef struct {
    target_ulong va;
    int size;
    DECLARE_BITMAP(mask, MAX_VEC_SIZE_BYTES) QEMU_ALIGNED(16);
    MMVector data QEMU_ALIGNED(16);
#ifndef CONFIG_USER_ONLY
    uint64_t pa;
#endif
} VStoreLog;

struct dma_state;
typedef uint32_t (*dma_insn_checker_ptr)(struct dma_state *);

struct ProcessorState {
    const rev_features_t *features;
    const options_struct *options;
    const arch_proc_opt_t *arch_proc_options;
    target_ulong runnable_threads_max;
    target_ulong thread_system_mask;
    CPUHexagonState *thread[THREADS_MAX];

    /* Useful information of the DMA engine per thread. */
    struct dma_adapter_engine_info * dma_engine_info[THREADS_MAX];
    struct dma_state *dma[DMA_MAX]; /* same as dma_t */
    dma_insn_checker_ptr dma_insn_checker[DMA_MAX];
    uint64_t monotonic_pcycles; /* never reset */

    /* one hmx unit shared among all threads */
    hmx_state_t *shared_extptr;
    int timing_on;
};

typedef struct CPUHexagonState CPUArchState;
typedef struct HexagonCPU ArchCPU;


typedef struct hmx_mem_access_info {
    int32_t dY;
    uint16_t blocks;
    uint32_t fx;
    uint32_t fy;
    uint32_t x_offset;
    uint32_t y_offset;
    uint32_t tile_x_mask;
    uint32_t tile_y_mask;
    uint32_t tile_y_inc;
    uint32_t y_start;
    uint32_t y_stop;
    uint32_t x_start;
    uint32_t x_stop;
    uint8_t y_tap;
    uint8_t x_tap;
    uint8_t ch_start;
    uint8_t ch_stop;
    uint8_t group_size;
    uint8_t group_count;
    uint8_t group_count_ratio;
    uint8_t block_type:3;
    uint8_t format:2;
    uint8_t acc_select:1;
    uint8_t acc_range:1;
    uint8_t flt:1;
    uint8_t x_dilate:1;
    uint8_t y_dilate:1;
    uint8_t deep:1;
    uint8_t wgt_deep:1;
    uint8_t drop:1;
    uint8_t batch:1;
    int8_t weight_count;
    uint8_t bias_32bit;
    uint8_t cvt_wr_only:1;
    uint8_t weight_bits;
    uint8_t enable16x16;
    uint8_t outputselect16x16;
    uint8_t act_reuse;
    uint32_t wgt_size;
    uint16_t egy_mpy_acc[HMX_MAX_EGY_CYCLE];
    uint8_t egy_cvt;
} hmx_mem_access_info_t;

#include "xlate_info.h"

typedef struct {
    target_ulong vaddr;
    target_ulong bad_vaddr;
    uint64_t paddr;
    uint32_t range;
    uint64_t lddata;
    uint64_t stdata;
    int cancelled;
    int tnum;
    enum mem_access_types type;
    unsigned char cdata[512];
    uint32_t width;
    uint32_t page_cross_base;
    uint32_t page_cross_sum;
    uint16_t slot;
    uint8_t check_page_crosses;
    xlate_info_t xlate_info;
    hmx_mem_access_info_t hmx_ma;
    uint8_t is_dealloc:1;
    uint8_t is_memop:1;
    uint8_t valid:1;
    uint8_t log_as_tag:1;
    uint8_t no_deriveumaptr:1;
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
#ifndef CONFIG_USER_ONLY
#define MMU_GUEST_IDX        1
#define MMU_KERNEL_IDX       2
#endif

#define EXEC_STATUS_OK          0x0000
#define EXEC_STATUS_STOP        0x0002
#define EXEC_STATUS_REPLAY      0x0010
#define EXEC_STATUS_LOCKED      0x0020
#define EXEC_STATUS_EXCEPTION   0x0100


#define EXCEPTION_DETECTED      (env->status & EXEC_STATUS_EXCEPTION)
#define REPLAY_DETECTED         (env->status & EXEC_STATUS_REPLAY)
#define CLEAR_EXCEPTION         (env->status &= (~EXEC_STATUS_EXCEPTION))
#define SET_EXCEPTION           (env->status |= EXEC_STATUS_EXCEPTION)

/* Maximum number of vector temps in a packet */
#define VECTOR_TEMPS_MAX            4

#ifndef CONFIG_USER_ONLY
/* TODO: Update for Hexagon: Meanings of the ARMCPU object's four inbound GPIO lines */
#define HEXAGON_CPU_IRQ_0 0
#define HEXAGON_CPU_IRQ_1 1
#define HEXAGON_CPU_IRQ_2 2
#define HEXAGON_CPU_IRQ_3 3
#define HEXAGON_CPU_IRQ_4 4
#define HEXAGON_CPU_IRQ_5 5
#define HEXAGON_CPU_IRQ_6 6
#define HEXAGON_CPU_IRQ_7 7

typedef enum {
    HEX_LOCK_UNLOCKED       = 0,
    HEX_LOCK_WAITING        = 1,
    HEX_LOCK_OWNER          = 2
} hex_lock_state_t;

#endif

struct Einfo {
  uint8_t valid;
  uint8_t type;
  uint8_t cause;
  uint8_t bvs:1;
  uint8_t bv0:1;       /* valid for badva0 */
  uint8_t bv1:1;       /* valid for badva1 */
  uint32_t badva0;
  uint32_t badva1;
  uint32_t elr;
  uint16_t diag;
  uint16_t de_slotmask;
};
typedef struct Einfo exception_info;
typedef struct Instruction Insn;
typedef unsigned systemstate_t;

struct CPUHexagonState {
    target_ulong gpr[TOTAL_PER_THREAD_REGS];
    target_ulong pred[NUM_PREGS];
    target_ulong branch_taken;
    target_ulong next_PC;
    target_ulong cause_code;

    /* For comparing with LLDB on target - see adjust_stack_ptrs function */
    target_ulong last_pc_dumped;
    target_ulong stack_start;

    uint8_t slot_cancelled;
    target_ulong new_value[TOTAL_PER_THREAD_REGS];

#ifndef CONFIG_USER_ONLY
    /* some system registers are per thread and some are global */
    target_ulong t_sreg[NUM_SREGS];
    target_ulong t_sreg_new_value[NUM_SREGS];
    target_ulong t_sreg_written[NUM_SREGS];
    target_ulong *g_sreg;

    target_ulong greg[NUM_GREGS];
    target_ulong greg_new_value[NUM_GREGS];
    target_ulong greg_written[NUM_GREGS];
#endif

    /*
     * Only used when HEX_DEBUG is on, but unconditionally included
     * to reduce recompile time when turning HEX_DEBUG on/off.
     */
    target_ulong this_PC;
    target_ulong reg_written[TOTAL_PER_THREAD_REGS];

    target_ulong new_pred_value[NUM_PREGS];
    target_ulong pred_written;

    MemLog mem_log_stores[STORES_MAX];
    target_ulong dczero_addr;

    float_status fp_status;

    target_ulong llsc_addr;
    target_ulong llsc_val;
    uint64_t     llsc_val_i64;

    MMVector VRegs[NUM_VREGS] QEMU_ALIGNED(16);
    MMVector future_VRegs[VECTOR_TEMPS_MAX] QEMU_ALIGNED(16);
    MMVector tmp_VRegs[VECTOR_TEMPS_MAX] QEMU_ALIGNED(16);

    VRegMask VRegs_updated;

    MMQReg QRegs[NUM_QREGS] QEMU_ALIGNED(16);
    MMQReg future_QRegs[NUM_QREGS] QEMU_ALIGNED(16);
    QRegMask QRegs_updated;

    /* Temporaries used within instructions */
    MMVectorPair VuuV QEMU_ALIGNED(16);
    MMVectorPair VvvV QEMU_ALIGNED(16);
    MMVectorPair VxxV QEMU_ALIGNED(16);
    MMVector     vtmp QEMU_ALIGNED(16);
    MMQReg       qtmp QEMU_ALIGNED(16);

    VStoreLog vstore[VSTORES_MAX];
    uint8_t store_pending[VSTORES_MAX];          /* FIXME - Is this needed? */
    target_ulong vstore_pending[VSTORES_MAX];
    bool vtcm_pending;
    VTCMStoreLog vtcm_log;
    mem_access_info_t mem_access[SLOTS_MAX];

    int status;
    uint8_t bq_on;

    unsigned int timing_on;

    uint64_t pktid;
    processor_t *processor_ptr;
    unsigned int threadId;
    system_t *system_ptr;
    FILE *fp_hmx_debug;
#ifndef CONFIG_USER_ONLY
    int slot;                    /* Needed for exception generation */
    systemstate_t systemstate;
    const char *cmdline;
    CPUHexagonTLBContext *hex_tlb;
    target_ulong imprecise_exception;
    hex_lock_state_t tlb_lock_state;
    hex_lock_state_t k0_lock_state;
    uint16_t nmi_threads;
    uint32_t last_cpu;
    GList *dir_list;
    uint32_t exe_arch;
    gchar *lib_search_dir;
#endif
};
#define mmvecx_t CPUHexagonState

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

#if !defined(CONFIG_USER_ONLY)
    bool count_gcycle_xt;
    bool sched_limit;
    gchar *usefs;
    uint64_t config_table_addr;
    uint32_t rev_reg;
    bool vp_mode;
    uint32_t boot_addr;
    uint32_t boot_evb;
    uint32_t l2vic_base_addr;
    uint32_t qtimer_base_addr;
#endif
    bool lldb_compat;
    target_ulong lldb_stack_adjust;
} HexagonCPU;

#include "cpu_bits.h"

#define cpu_signal_handler cpu_hexagon_signal_handler
extern int cpu_hexagon_signal_handler(int host_signum, void *pinfo, void *puc);

#ifndef CONFIG_USER_ONLY
enum {
    PCYCLE_ENABLED_FLAGS_BIT = 3,
};

static inline void set_pcycle_enabled_flag(uint32_t *flags)
{
    *flags |= (1 << PCYCLE_ENABLED_FLAGS_BIT);
}

static inline bool get_pcycle_enabled_flag(uint32_t flags)
{
    return (flags >> PCYCLE_ENABLED_FLAGS_BIT) & 1;
}
#endif

static inline void cpu_get_tb_cpu_state(CPUHexagonState *env, target_ulong *pc,
                                        target_ulong *cs_base, uint32_t *flags)
{
    *pc = env->gpr[HEX_REG_PC];
    *cs_base = 0;
#ifdef CONFIG_USER_ONLY
    *flags = 0;
#else
    target_ulong syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    bool pcycle_enabled = extract32(syscfg,
                                    reg_field_info[SYSCFG_PCYCLEEN].offset,
                                    reg_field_info[SYSCFG_PCYCLEEN].width);

    *flags = cpu_mmu_index(env, false);

    if (pcycle_enabled) {
        set_pcycle_enabled_flag(flags);
    }
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
 * @return true if the @a thread_env hardware thread is
 * not stopped.
 */
bool hexagon_thread_is_enabled(CPUHexagonState *thread_env);
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
                                    uint32_t int_num, int vid_int_pending,
                                    uint32_t resume_pc);

void hexagon_raise_interrupt(CPUHexagonState *env, HexagonCPU *thread,
                             uint32_t int_num, int vid_int_pending);

uint32_t hexagon_greg_read(CPUHexagonState *env, uint32_t reg);
#endif

void hexagon_translate_init(void);
void hexagon_cpu_soft_reset(CPUHexagonState *env);

extern void hexagon_cpu_do_interrupt(CPUState *cpu);
extern void register_trap_exception(CPUHexagonState *env, uintptr_t next_pc, int traptype, int imm);

#include "exec/cpu-all.h"

#endif /* HEXAGON_CPU_H */
