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

#include "qemu/osdep.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "qapi/error.h"
#include "hw/qdev-properties.h"
#include "fpu/softfloat-helpers.h"
#include "gdb_qreginfo.h"
#include "hmx/ext_hmx.h"
#include "trace.h"

#if !defined(CONFIG_USER_ONLY)
#include "migration/vmstate.h"
#include "macros.h"
#include "sys_macros.h"
#include "hex_mmu.h"
#include "hw/intc/l2vic.h"
#include "qemu/main-loop.h"
#include "sysemu/cpus.h"
#endif

static void hexagon_common_cpu_init(Object *obj)
{
}

static ObjectClass *hexagon_cpu_class_by_name(const char *cpu_model)
{
    ObjectClass *oc;
    char *typename;
    char **cpuname;

    cpuname = g_strsplit(cpu_model, ",", 1);
    typename = g_strdup_printf(HEXAGON_CPU_TYPE_NAME("%s"), cpuname[0]);
    oc = object_class_by_name(typename);
    g_strfreev(cpuname);
    g_free(typename);
    if (!oc || !object_class_dynamic_cast(oc, TYPE_HEXAGON_CPU) ||
        object_class_is_abstract(oc)) {
        return NULL;
    }
    return oc;
}

static Property hexagon_cpu_properties[] = {
#if !defined(CONFIG_USER_ONLY)
    DEFINE_PROP_BOOL("count-gcycle-xt", HexagonCPU, count_gcycle_xt, false),
    DEFINE_PROP_BOOL("sched-limit", HexagonCPU, sched_limit, false),
    DEFINE_PROP_STRING("usefs", HexagonCPU, usefs),
    DEFINE_PROP_UINT64("config-table-addr", HexagonCPU, config_table_addr,
        0xffffffffULL),
    DEFINE_PROP_UINT32("dsp-rev", HexagonCPU, rev_reg, /*v73:*/0x8c73),
    DEFINE_PROP_BOOL("virtual-platform-mode", HexagonCPU, vp_mode, false),
    DEFINE_PROP_UINT32("start-evb", HexagonCPU, boot_evb, 0x0),
    DEFINE_PROP_UINT32("exec-start-addr", HexagonCPU, boot_addr,
        0xffffffffULL),
    DEFINE_PROP_UINT32("l2vic-base-addr", HexagonCPU, l2vic_base_addr,
        0xffffffffULL),
    DEFINE_PROP_UINT32("qtimer-base-addr", HexagonCPU, qtimer_base_addr,
        0xffffffffULL),
#endif
    DEFINE_PROP_BOOL("lldb-compat", HexagonCPU, lldb_compat, false),
    DEFINE_PROP_UNSIGNED("lldb-stack-adjust", HexagonCPU, lldb_stack_adjust,
                         0, qdev_prop_uint32, target_ulong),
    DEFINE_PROP_END_OF_LIST()
};


const char * const hexagon_regnames[TOTAL_PER_THREAD_REGS] = {
#ifdef CONFIG_USER_ONLY
    "r0", "r1",  "r2",  "r3",  "r4",   "r5",  "r6",  "r7",
    "r8", "r9",  "r10", "r11", "r12",  "r13", "r14", "r15",
#else
    "r00", "r01",  "r02", "r03", "r04",  "r05", "r06", "r07",
    "r08", "r09",  "r10", "r11", "r12",  "r13", "r14", "r15",
#endif
    "r16", "r17", "r18", "r19", "r20",  "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28",  "r29", "r30", "r31",
    "sa0", "lc0", "sa1", "lc1", "p3_0", "c5",  "m0",  "m1",
    "usr", "pc",  "ugp", "gp",  "cs0",  "cs1", "upcyclelo", "upcyclehi",
    "framelimit", "framekey", "pktcountlo", "pktcounthi", "pkt_cnt",  "insn_cnt", "hvx_cnt", "c23",
    "c24", "c25", "c26", "c27", "c28",  "c29", "utimerlo", "utimerhi",
};

#ifndef CONFIG_USER_ONLY
const char * const hexagon_sregnames[] = {
    "sgp0",       "sgp1",       "stid",       "elr",        "badva0",
    "badva1",     "ssr",        "ccr",        "htid",       "badva",
    "imask",      "gevb",       "rsv12",      "rsv13",      "rsv14",
    "rsv15",      "evb",        "modectl",    "syscfg",     "free19",
    "ipendad",    "vid",        "vid1",       "bestwait",   "free24",
    "schedcfg",   "free26",     "cfgbase",    "diag",       "rev",
    "pcyclelo",   "pcyclehi",   "isdbst",     "isdbcfg0",   "isdbcfg1",
    "livelock",   "brkptpc0",   "brkptccfg0", "brkptpc1",   "brkptcfg1",
    "isdbmbxin",  "isdbmbxout", "isdben",     "isdbgpr",    "pmucnt4",
    "pmucnt5",    "pmucnt6",    "pmucnt7",    "pmucnt0",    "pmucnt1",
    "pmucnt2",    "pmucnt3",    "pmuevtcfg",  "pmustid0",   "pmuevtcfg1",
    "pmustid1",   "timerlo",    "timerhi",    "pmucfg",     "rsv59",
    "rsv60",      "rsv61",      "rsv62",      "rsv63"
};

const char * const hexagon_gregnames[] = {
    "g0",         "g1",         "g2",       "g3",
    "rsv4",       "rsv5",       "rsv6",     "rsv7",
    "rsv8",       "rsv9",       "rsv10",    "rsv11",
    "rsv12",      "rsv13",      "rsv14",    "rsv15,",
    "isdbmbxin",  "isdbmbxout", "rsv18",    "rsv19",
    "rsv20",      "rsv21",      "rsv22",    "rsv23",
    "gpcyclelo",  "gpcyclehi",  "gpmucnt0", "gpmucnt1",
    "gpmucnt2",   "gpmucnt3",   "rsv30",    "rsv31"
};
#endif

/*
 * One of the main debugging techniques is to use "-d cpu" and compare against
 * LLDB output when single stepping.  However, the target and qemu put the
 * stacks at different locations.  This is used to compensate so the diff is
 * cleaner.
 */
static target_ulong adjust_stack_ptrs(CPUHexagonState *env, target_ulong addr)
{
    HexagonCPU *cpu = env_archcpu(env);
    target_ulong stack_adjust = cpu->lldb_stack_adjust;
    target_ulong stack_start = env->stack_start;
    target_ulong stack_size = 0x10000;
#ifdef CONFIG_USER_ONLY
    env->gpr[HEX_REG_USR] = 0x56000;
#endif

    if (stack_adjust == 0) {
        return addr;
    }

    if (stack_start + 0x1000 >= addr && addr >= (stack_start - stack_size)) {
        return addr - stack_adjust;
    }
    return addr;
}

/* HEX_REG_P3_0 (aka C4) is an alias for the predicate registers */
static target_ulong read_p3_0(CPUHexagonState *env)
{
    int32_t control_reg = 0;
    int i;
    for (i = NUM_PREGS - 1; i >= 0; i--) {
        control_reg <<= 8;
        control_reg |= env->pred[i] & 0xff;
    }
    return control_reg;
}

static void print_reg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong value;

    if (regnum == HEX_REG_P3_0) {
        value = read_p3_0(env);
    } else {
        value = regnum < 32 ? adjust_stack_ptrs(env, env->gpr[regnum])
                            : env->gpr[regnum];
    }

    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n",
                 hexagon_regnames[regnum], value);
}

#ifndef CONFIG_USER_ONLY
static target_ulong get_badva(CPUHexagonState *env)
{
  target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
  if (GET_SSR_FIELD(SSR_BVS, ssr)) {
      return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA1);
  }
  else {
      return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA0);
  }
}

static void print_sreg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = ARCH_GET_SYSTEM_REG(env, regnum);
    if (regnum == HEX_SREG_BADVA) {
        val = get_badva(env);
    }
    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n", hexagon_sregnames[regnum],
                 val);
}

static void print_greg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = hexagon_greg_read(env, regnum);
    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n", hexagon_gregnames[regnum],
                 val);
}
#endif

static void print_vreg(FILE *f, CPUHexagonState *env, int regnum,
                       bool skip_if_zero)
{
    if (skip_if_zero) {
        bool nonzero_found = false;
        for (int i = 0; i < MAX_VEC_SIZE_BYTES; i++) {
            if (env->VRegs[regnum].ub[i] != 0) {
                nonzero_found = true;
                break;
            }
        }
        if (!nonzero_found) {
            return;
        }
    }

    qemu_fprintf(f, "  v%d = ( ", regnum);
    qemu_fprintf(f, "0x%02x", env->VRegs[regnum].ub[MAX_VEC_SIZE_BYTES - 1]);
    for (int i = MAX_VEC_SIZE_BYTES - 2; i >= 0; i--) {
        qemu_fprintf(f, ", 0x%02x", env->VRegs[regnum].ub[i]);
    }
    qemu_fprintf(f, " )\n");
}

void hexagon_debug_vreg(CPUHexagonState *env, int regnum)
{
    print_vreg(stdout, env, regnum, false);
}

static void print_qreg(FILE *f, CPUHexagonState *env, int regnum,
                       bool skip_if_zero)
{
    if (skip_if_zero) {
        bool nonzero_found = false;
        for (int i = 0; i < MAX_VEC_SIZE_BYTES / 8; i++) {
            if (env->QRegs[regnum].ub[i] != 0) {
                nonzero_found = true;
                break;
            }
        }
        if (!nonzero_found) {
            return;
        }
    }

    qemu_fprintf(f, "  q%d = ( ", regnum);
    qemu_fprintf(f, "0x%02x",
                 env->QRegs[regnum].ub[MAX_VEC_SIZE_BYTES / 8 - 1]);
    for (int i = MAX_VEC_SIZE_BYTES / 8 - 2; i >= 0; i--) {
        qemu_fprintf(f, ", 0x%02x", env->QRegs[regnum].ub[i]);
    }
    qemu_fprintf(f, " )\n");
}

void hexagon_debug_qreg(CPUHexagonState *env, int regnum)
{
    print_qreg(stdout, env, regnum, false);
}

void hexagon_dump(CPUHexagonState *env, FILE *f, int flags)
{
    HexagonCPU *cpu = env_archcpu(env);

    if (cpu->lldb_compat) {
        /*
         * When comparing with LLDB, it doesn't step through single-cycle
         * hardware loops the same way.  So, we just skip them here
         */
        if (env->gpr[HEX_REG_PC] == env->last_pc_dumped) {
            return;
        }
        env->last_pc_dumped = env->gpr[HEX_REG_PC];
    }

#ifdef CONFIG_USER_ONLY
    qemu_fprintf(f, "General Purpose Registers = {\n");
#else
    qemu_fprintf(f, "TID %d : General Purpose Registers = {\n", env->threadId);
#endif

    for (int i = 0; i < 32; i++) {
        print_reg(f, env, i);
    }
    print_reg(f, env, HEX_REG_SA0);
    print_reg(f, env, HEX_REG_LC0);
    print_reg(f, env, HEX_REG_SA1);
    print_reg(f, env, HEX_REG_LC1);

#ifdef CONFIG_USER_ONLY
    print_reg(f, env, HEX_REG_M0);
    print_reg(f, env, HEX_REG_M1);
    print_reg(f, env, HEX_REG_USR);
    print_reg(f, env, HEX_REG_P3_0);
    print_reg(f, env, HEX_REG_GP);
    print_reg(f, env, HEX_REG_UGP);
    print_reg(f, env, HEX_REG_PC);
    /*
     * Not modelled in user mode, print junk to minimize the diff's
     * with LLDB output
     */
    qemu_fprintf(f, "  cause = 0x000000db\n");
    qemu_fprintf(f, "  badva = 0x00000000\n");
    qemu_fprintf(f, "  cs0 = 0x00000000\n");
    qemu_fprintf(f, "  cs1 = 0x00000000\n");
#else
    print_reg(f, env, HEX_REG_P3_0);
    print_reg(f, env, HEX_REG_M0);
    print_reg(f, env, HEX_REG_M1);
    print_reg(f, env, HEX_REG_USR);
    print_reg(f, env, HEX_REG_PC);
    print_reg(f, env, HEX_REG_UGP);
    print_reg(f, env, HEX_REG_GP);

    print_reg(f, env, HEX_REG_CS0);
    print_reg(f, env, HEX_REG_CS1);

    print_reg(f, env, HEX_REG_UPCYCLELO);
    print_reg(f, env, HEX_REG_UPCYCLEHI);
    print_reg(f, env, HEX_REG_FRAMELIMIT);
    print_reg(f, env, HEX_REG_FRAMEKEY);
    print_reg(f, env, HEX_REG_PKTCNTLO);
    print_reg(f, env, HEX_REG_PKTCNTHI);
    print_reg(f, env, HEX_REG_UTIMERLO);
    print_reg(f, env, HEX_REG_UTIMERHI);
    print_sreg(f, env, HEX_SREG_SGP0);
    print_sreg(f, env, HEX_SREG_SGP1);
    print_sreg(f, env, HEX_SREG_STID);
    print_sreg(f, env, HEX_SREG_ELR);
    print_sreg(f, env, HEX_SREG_BADVA0);
    print_sreg(f, env, HEX_SREG_BADVA1);
    print_sreg(f, env, HEX_SREG_SSR);
    print_sreg(f, env, HEX_SREG_CCR);
    print_sreg(f, env, HEX_SREG_HTID);
    print_sreg(f, env, HEX_SREG_BADVA);
    print_sreg(f, env, HEX_SREG_IMASK);
    print_sreg(f, env, HEX_SREG_IPENDAD);
    print_sreg(f, env, HEX_SREG_VID);
    print_sreg(f, env, HEX_SREG_GEVB);
    print_greg(f, env, HEX_GREG_GELR);
    print_greg(f, env, HEX_GREG_GSR);
    print_greg(f, env, HEX_GREG_GOSP);
    print_greg(f, env, HEX_GREG_GBADVA);
    qemu_fprintf(f, "  dm0 = 0x00000000\n");
    qemu_fprintf(f, "  dm1 = 0x00000000\n");
    qemu_fprintf(f, "  dm2 = 0x000002a0\n");
    qemu_fprintf(f, "  dm3 = 0x00000000\n");
    qemu_fprintf(f, "  dm4 = 0x00000000\n");
    qemu_fprintf(f, "  dm5 = 0x00000000\n");
    qemu_fprintf(f, "  dm6 = 0x00000000\n");
    qemu_fprintf(f, "  dm7 = 0x00000000\n");
#endif
    qemu_fprintf(f, "}\n");

    if (flags & CPU_DUMP_FPU) {
        qemu_fprintf(f, "Vector Registers = {\n");
        for (int i = 0; i < NUM_VREGS; i++) {
            print_vreg(f, env, i, true);
        }
        for (int i = 0; i < NUM_QREGS; i++) {
            print_qreg(f, env, i, true);
        }
        qemu_fprintf(f, "}\n");
    }
}

static void hexagon_dump_state(CPUState *cs, FILE *f, int flags)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    hexagon_dump(env, f, flags);
}

void hexagon_debug(CPUHexagonState *env)
{
    hexagon_dump(env, stdout, CPU_DUMP_FPU);
}

static void hexagon_cpu_set_pc(CPUState *cs, vaddr value)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->gpr[HEX_REG_PC] = value;
}

static void hexagon_cpu_synchronize_from_tb(CPUState *cs,
                                            const TranslationBlock *tb)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->gpr[HEX_REG_PC] = tb->pc;
}

void restore_state_to_opc(CPUHexagonState *env, TranslationBlock *tb,
                          target_ulong *data)
{
    env->gpr[HEX_REG_PC] = data[0];
}

#if !defined(CONFIG_USER_ONLY)
void hexagon_cpu_soft_reset(CPUHexagonState *env)
{
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_SSR, 0);
    hexagon_ssr_set_cause(env, 0);

    target_ulong evb = ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB);
    ARCH_SET_THREAD_REG(env, HEX_REG_PC, evb);
}
#endif

static void hexagon_cpu_reset(DeviceState *dev)
{
    CPUState *cs = CPU(dev);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    HexagonCPUClass *mcc = HEXAGON_CPU_GET_CLASS(cpu);
    CPUHexagonState *env = &cpu->env;

    mcc->parent_reset(dev);

    set_default_nan_mode(1, &env->fp_status);
    set_float_detect_tininess(float_tininess_before_rounding, &env->fp_status);
    hmx_reset(env->processor_ptr, env);

#ifndef CONFIG_USER_ONLY
    clear_wait_mode(env);

    env->tlb_lock_state = HEX_LOCK_UNLOCKED;
    env->k0_lock_state = HEX_LOCK_UNLOCKED;

    hexagon_cpu_soft_reset(env);
#endif
}

static void hexagon_cpu_disas_set_info(CPUState *s, disassemble_info *info)
{
    info->print_insn = print_insn_hexagon;
}

dma_t *dma_adapter_init(processor_t *proc, int dmanum);
const rev_features_t rev_features_v68 = {
};

const options_struct options_struct_v68 = {
    .l2tcm_base  = 0,  /* FIXME - Should be l2tcm_base ?? */
    .hmx_mac_fxp_callback = (void *)0,
    .hmx_mac_flt_callback = (void *)0,
};

const arch_proc_opt_t arch_proc_opt_v68 = {
    .vtcm_size = VTCM_SIZE,
    .vtcm_offset = VTCM_OFFSET,
    .dmadebugfile = NULL,
    .hmxdebuglvl = 0,
    .hmx_output_depth = 32,
    .hmx_spatial_size = 6,
    .hmx_channel_size = 5,
    .hmx_block_size = 2048,
    .hmx_mxmem_debug_acc_preload = 0,
    .pmu_enable = 0,
    .hmxdebugfile = 0,
    .hmx_mxmem_debug = 0,
    .hmxaccpreloadfile = 0,
    .hmxarray_new = 0,
    .hmxmpytrace = 0,
    .hmx_v1 = 0,
    .hmx_power_config = 0,
    .hmx_8x4_mpy_mode = 0,
    .hmx_group_conv_mode = 1,
    .dmadebug_verbosity = 0,
    .xfp_inexact_enable = 1,
    .xfp_cvt_frac = 13,
    .xfp_cvt_int = 3,
    .QDSP6_DMA_PRESENT     = 1,
    .QDSP6_DMA_EXTENDED_VA_PRESENT = 0,
    .QDSP6_MX_FP_PRESENT   = 1,
    .QDSP6_MX_RATE = 8,
    .QDSP6_MX_CHANNELS = 32,
    .QDSP6_MX_ROWS = 64,
    .QDSP6_MX_COLS = 32,
    .QDSP6_MX_CVT_MPY_SZ = 10,
    .QDSP6_MX_SUB_COLS = 1,
    .QDSP6_MX_ACCUM_WIDTH = 32,
    .QDSP6_MX_CVT_WIDTH = 12,
    .QDSP6_MX_FP_RATE = 4,
    .QDSP6_MX_FP_ACC_INT = 7,
    .QDSP6_MX_FP_ACC_FRAC = 22,
    .QDSP6_MX_FP_ACC_EXP = 9,
    .QDSP6_MX_FP_ROWS = 32,
    .QDSP6_MX_FP_COLS = 32,
    .QDSP6_MX_FP_ACC_NORM = 3,
    .QDSP6_MX_PARALLEL_GRPS = 8,
    .QDSP6_VX_PRESENT = 1,
    .QDSP6_VX_CONTEXTS = VECTOR_UNIT_MAX,
    .QDSP6_VX_MEM_ENTRIES = 2048,
    .QDSP6_VX_VEC_SZ = 1024,
};

struct ProcessorState ProcessorStateV68 = {
    .features = &rev_features_v68,
    .options = &options_struct_v68,
    .arch_proc_options = &arch_proc_opt_v68,
    .runnable_threads_max = THREADS_MAX,
    .thread_system_mask = 0xf,
    .shared_extptr = 0,
    .timing_on = 0,
};

static void hexagon_cpu_realize(DeviceState *dev, Error **errp)
{
    CPUState *cs = CPU(dev);
    HexagonCPUClass *mcc = HEXAGON_CPU_GET_CLASS(dev);
    Error *local_err = NULL;

    cpu_exec_realizefn(cs, &local_err);
    if (local_err != NULL) {
        error_propagate(errp, local_err);
        return;
    }
    if (cs->cpu_index >= THREADS_MAX) {
        error_setg(errp, "Out of threads.  Max is: %d, wanted: %d", THREADS_MAX, cs->cpu_index);
        return;
    }

    qemu_init_vcpu(cs);

    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->threadId = cs->cpu_index;
    env->processor_ptr = &ProcessorStateV68;
    env->processor_ptr->thread[env->threadId] = env;
    env->processor_ptr->dma[env->threadId] = dma_adapter_init(
        env->processor_ptr,
        env->threadId);
    env->system_ptr = NULL;

#define HEXAGON_CFG_ADDR_BASE(addr) ((addr >> 16) & 0x0fffff)
#ifndef CONFIG_USER_ONLY
    if (cs->cpu_index == 0) {
        env->g_sreg = g_malloc0(sizeof(target_ulong) * NUM_SREGS);
        env->processor_ptr->shared_extptr = hmx_ext_palloc(env->processor_ptr, 0);
        hmx_configure_state(env);
        hex_mmu_init(env);
        env->g_sreg[HEX_SREG_EVB] = 0x0;
        env->g_sreg[HEX_SREG_CFGBASE] =
            HEXAGON_CFG_ADDR_BASE(cpu->config_table_addr);
        env->g_sreg[HEX_SREG_REV] = cpu->rev_reg;
        env->g_sreg[HEX_SREG_MODECTL] = 0x1;
    }
    else {
        CPUState *cpu0_s = NULL;
        CPUHexagonState *env0 = NULL;
        CPU_FOREACH (cpu0_s) {
            assert(cpu0_s->cpu_index == 0);
            env0 = &(HEXAGON_CPU(cpu0_s)->env);

            break;
        }
        env->g_sreg = env0->g_sreg;
        env->hex_tlb = env0->hex_tlb;
        env->cmdline = env0->cmdline;
        env->lib_search_dir = env0->lib_search_dir;
        env->processor_ptr->shared_extptr = env0->processor_ptr->shared_extptr;

        dma_t *dma_ptr = env->processor_ptr->dma[env->threadId];
        udma_ctx_t *udma_ctx = (udma_ctx_t *)dma_ptr->udma_ctx;

        dma_t *dma0_ptr = env->processor_ptr->dma[env0->threadId];
        udma_ctx_t *udma0_ctx = (udma_ctx_t *)dma0_ptr->udma_ctx;

        /* init dm2 of new thread to env_0 thread */
        udma_ctx->dm2.val = udma0_ctx->dm2.val;

        clear_wait_mode(env);
    }

    ARCH_SET_SYSTEM_REG(env, HEX_SREG_HTID, env->threadId);
#else
    env->processor_ptr->shared_extptr = hmx_ext_palloc(env->processor_ptr, 0);
    hmx_configure_state(env);
#endif
    cpu_reset(cs);
#if !defined(CONFIG_USER_ONLY)
    env->g_sreg[HEX_SREG_EVB] = cpu->boot_evb;
    env->gpr[HEX_REG_PC] = cpu->boot_addr;
#endif

    mcc->parent_realize(dev, errp);
}

#ifndef CONFIG_USER_ONLY
uint32_t hexagon_get_interrupts(CPUHexagonState *env)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    return GET_FIELD(IPENDAD_IPEND, ipendad);
}

bool hexagon_thread_is_enabled(CPUHexagonState *env) {
    target_ulong modectl = ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    bool E_bit = thread_enabled_mask & (0x1 << env->threadId);

    return E_bit;
}

static bool hexagon_cpu_has_work(CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    return hexagon_thread_is_enabled(env) &&
        (cs->interrupt_request & CPU_INTERRUPT_HARD);
}

static void hexagon_cpu_set_irq(void *opaque, int irq, int level)
{
    HexagonCPU *cpu = opaque;
    CPUHexagonState *env = &cpu->env;
    CPUState *cs = CPU(cpu);

    trace_hexagon_irq_line(irq, level);

    target_ulong syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    target_ulong gbit = GET_SYSCFG_FIELD(SYSCFG_GIE, syscfg);
    if (!gbit && level) {
        hexagon_set_interrupts(env, 1 << irq);
        return;
    }

 /* Event#  CauseCode    Description
  * 16       0xC0        Interrupt 0  General interrupt. Maskable,
  *                                   highest priority general interrupt
  * 17       0xC1        Interrupt 1
  * 18       0xC2        Interrupt 2  VIC0 Interface
  * 19       0xC3        Interrupt 3  VIC1 Interface
  * 20       0xC4        Interrupt 4  VIC2 Interface
  * 21       0xC5        Interrupt 5  VIC3 Interface
  * 22       0xC6        Interrupt 6
  * 23       0xC7        Interrupt 7  Lowest priority interrupt
  */
    static const int mask[] = {
        [HEXAGON_CPU_IRQ_0] = CPU_INTERRUPT_HARD,
        [HEXAGON_CPU_IRQ_1] = CPU_INTERRUPT_HARD,
        [HEXAGON_CPU_IRQ_2] = CPU_INTERRUPT_HARD,
        [HEXAGON_CPU_IRQ_3] = CPU_INTERRUPT_HARD,
        [HEXAGON_CPU_IRQ_4] = CPU_INTERRUPT_HARD,
        [HEXAGON_CPU_IRQ_5] = CPU_INTERRUPT_HARD,
        [HEXAGON_CPU_IRQ_6] = CPU_INTERRUPT_HARD,
        [HEXAGON_CPU_IRQ_7] = CPU_INTERRUPT_HARD,
    };


    /* IRQ 2, event number: 18, Cause Code: 0xc2 (VIC0 interface) */
    if (level) {
        HexagonCPU *threads[THREADS_MAX];
        size_t threads_count = 0;
        memset(threads, 0, sizeof(threads));
        hexagon_find_int_threads(env, irq, threads, &threads_count);
        if (threads_count < 1) {
            trace_hexagon_irq_thread_candidates(0, -1, -1);
            /*
             * No unmasked thread is available for this interrupt.  We
             * must pend it and assert it later.
             */
           hexagon_set_interrupts(env, 1 << irq);
           /*
            * FIXME: or should we make this method the general method
            * and not one that's only used when threads are occupied?
            */

           return;
        }

        HexagonCPU *int_thread =
            hexagon_find_lowest_prio_any_thread(threads, threads_count, irq, NULL);
        env = &int_thread->env;
        cs = CPU(int_thread);

        target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
        target_ulong ex = GET_SSR_FIELD(SSR_EX, ssr);
        trace_hexagon_irq_thread_candidates(threads_count, cs->cpu_index, !ex);

        if (ex) {
            assert(get_exe_mode(env) != HEX_EXE_MODE_WAIT);
            /*
             * This thread is not interruptible.  We
             * must pend it and assert it later.
             */
           hexagon_set_interrupts(env, 1 << irq);

           /*
            * FIXME: or should we make this method the general method
            * and not one that's only used when threads are occupied?
            */
           return;
        }
        env->cause_code = HEX_CAUSE_INT0 + irq;
    }
    switch (irq) {
    case HEXAGON_CPU_IRQ_0 ... HEXAGON_CPU_IRQ_7:
        HEX_DEBUG_LOG("%s: IRQ irq:%d, level:%d\n", __func__, irq, level);
        if (level) {
            if (get_exe_mode(env) == HEX_EXE_MODE_WAIT) {
                hexagon_resume_thread(env, HEX_EVENT_INT0 + irq);
            }
            hexagon_disable_int(env, irq);
            cpu_interrupt(cs, mask[irq]);
        } else {
            //FIXME: do something better for the level=0 path
            //The following line was always clearing CPU_INTERRUPT_HARD
            //on the thread 0. At least some thread look up must be done.
            //cpu_reset_interrupt(cs, mask[irq]);
        }
        break;
    default:
        g_assert_not_reached();
    }
}
#endif

static void hexagon_cpu_init(Object *obj)
{
    HexagonCPU *cpu = HEXAGON_CPU(obj);

    cpu_set_cpustate_pointers(cpu);
#if !defined(CONFIG_USER_ONLY)
    qdev_init_gpio_in(DEVICE(cpu), hexagon_cpu_set_irq, 8);
#endif
}


#ifndef CONFIG_USER_ONLY


void hexagon_set_interrupts(CPUHexagonState *env, uint32_t mask)
{
    /* Assert all of the interrupts in the `mask` input: */
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong ipendad_ipend = GET_FIELD(IPENDAD_IPEND, ipendad);
    ipendad_ipend |= mask;
    SET_SYSTEM_FIELD(env, HEX_SREG_IPENDAD, IPENDAD_IPEND, ipendad_ipend);
}

void hexagon_raise_interrupt(CPUHexagonState *env, HexagonCPU *thread,
                             uint32_t int_num, int vid_int_pending)
{
    hexagon_raise_interrupt_resume(env, thread, int_num, vid_int_pending, 0);
}

static void hexagon_clear_l2vic_pending(CPUHexagonState *env, int int_num)
{
    uint32_t val;
    target_ulong vidval;
    uint32_t slice = (int_num / 32) * 4;
    val = 1 << (int_num % 32);

    CPUState *cs = env_cpu(env);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    const hwaddr addr = cpu->l2vic_base_addr + L2VIC_INT_PENDINGn;
    cpu_physical_memory_read(addr + slice, &vidval, sizeof(vidval));
    vidval &= ~val;
    cpu_physical_memory_write(addr + slice, &vidval, sizeof(vidval));
}
void hexagon_raise_interrupt_resume(CPUHexagonState *env, HexagonCPU *thread,
                                    uint32_t int_num, int vid_int_pending,
                                    target_ulong resume_pc)
{
    // This logic is for interrupt numbers 0-15 only
    assert(int_num <= 15);

    CPUHexagonState *thread_env = &(HEXAGON_CPU(thread)->env);
    assert(!hexagon_thread_is_busy(thread_env));
    assert(hexagon_thread_ints_enabled(thread_env));
    assert(hexagon_thread_is_interruptible(thread_env, int_num));

    CPUState *cs = CPU(thread);
    cs->exception_index = HEX_EVENT_INT0 + int_num;
    thread_env->cause_code = HEX_CAUSE_INT0 + int_num;
    if (vid_int_pending != L2VIC_NO_PENDING) {
        hexagon_clear_l2vic_pending(env, vid_int_pending);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_VID, vid_int_pending);
    }

    if (get_exe_mode(thread_env) == HEX_EXE_MODE_WAIT) {
        hexagon_resume_thread(thread_env, cs->exception_index);
    }
    hexagon_disable_int(env, int_num);
}

void hexagon_find_asserted_interrupts(CPUHexagonState *env, uint32_t *ints,
                                      size_t list_capacity, size_t *list_size)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong ipendad_ipend = GET_FIELD(IPENDAD_IPEND, ipendad);
    /* find interrupts asserted but not auto disabled */
    unsigned intnum;
    target_ulong ipendad_iad = GET_FIELD(IPENDAD_IAD, ipendad);
    HEX_DEBUG_LOG("%s: ipendad_iad 0x%x\n", __FUNCTION__, ipendad_iad);

    assert(list_capacity >= reg_field_info[IPENDAD_IPEND].width);
    *list_size = 0;
    for (intnum = 0; intnum < reg_field_info[IPENDAD_IPEND].width &&
                     *list_size <= list_capacity;
         ++intnum) {
        unsigned mask = 0x1 << intnum;
        if ((ipendad_ipend & mask) && (!(ipendad_iad & mask))) {
            ints[(*list_size)++] = intnum;
        }
    }
}

void hexagon_find_int_threads(CPUHexagonState *env, uint32_t int_num,
                              HexagonCPU *threads[], size_t *list_size)
{
    // This logic is for interrupt numbers 0-15 only
    assert(int_num <= 15);

    *list_size = 0;

    target_ulong syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    target_ulong gbit = GET_SYSCFG_FIELD(SYSCFG_GIE, syscfg);
    if (!gbit) {
        HEX_DEBUG_LOG("%s: IE disabled (syscfg 0x%x), exiting\n", __FUNCTION__,
                      syscfg);
        return;
    }

    /* find threads that can handle this interrupt */
    CPUState *cs = NULL;
    CPU_FOREACH (cs) {
        HexagonCPU *chk_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *chk_env = &chk_cpu->env;
        if (!hexagon_thread_is_interruptible(chk_env, int_num)) {
            HEX_DEBUG_LOG("%s: imask & mask: continue\n", __FUNCTION__);
            continue;
        }
        HEX_DEBUG_LOG("%s: adding tid %d to list\n", __FUNCTION__,
                      chk_env->threadId);
        threads[(*list_size)++] = chk_cpu;
    }
}

HexagonCPU *hexagon_find_lowest_prio(CPUHexagonState *env, uint32_t int_num) {
    HexagonCPU *threads[THREADS_MAX];
    memset(threads, 0, sizeof(threads));
    size_t i = 0;
    CPUState *cs = env_cpu(env);
    CPU_FOREACH(cs) {
        threads[i++] = HEXAGON_CPU(cs);
    }
    return hexagon_find_lowest_prio_any_thread(threads, i, int_num, NULL);
}

HexagonCPU *hexagon_find_lowest_prio_any_thread(HexagonCPU *threads[],
                                                size_t list_size,
                                                uint32_t int_num,
                                                uint32_t *low_prio)
{
    return hexagon_find_lowest_prio_thread(threads, list_size, int_num, false, low_prio);
}

HexagonCPU *hexagon_find_lowest_prio_waiting_thread(HexagonCPU *threads[],
                                                    size_t list_size,
                                                    uint32_t int_num,
                                                    uint32_t *low_prio)
{
    return hexagon_find_lowest_prio_thread(threads, list_size, int_num, true, low_prio);
}

HexagonCPU *hexagon_find_lowest_prio_thread(HexagonCPU *threads[],
                                            size_t list_size,
                                            uint32_t int_num,
                                            bool only_waiters,
                                            uint32_t *low_prio)
{
    assert(list_size >= 1);
    target_ulong syscfg = ARCH_GET_SYSTEM_REG(&threads[0]->env,
        HEX_SREG_SYSCFG);
    target_ulong gbit = GET_SYSCFG_FIELD(SYSCFG_GIE, syscfg);
    if (!gbit) {
        return NULL;
    }

    uint32_t lowest_th_prio = 0;
    size_t lowest_prio_index = 0;
    for (size_t i = 0; i < list_size; i++) {
        CPUHexagonState *thread_env = &(threads[i]->env);
        uint32_t th_prio = GET_FIELD(
            STID_PRIO, ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_STID));
        const bool waiting = (get_exe_mode(thread_env) == HEX_EXE_MODE_WAIT);
        const bool is_candidate = ((only_waiters && waiting) || !only_waiters) &&
            hexagon_thread_is_interruptible(thread_env, int_num);
        if (!is_candidate) {
            continue;
        }

        /* 0 is the greatest priority for a thread, track the values
         * that are lower priority (w/greater value).
         */
        if (lowest_th_prio < th_prio) {
            HEX_DEBUG_LOG("\tlowest prio now tid %d, prio %03x\n",
                          thread_env->threadId, th_prio);
            lowest_prio_index = i;
            lowest_th_prio = th_prio;
        } else {
            HEX_DEBUG_LOG("\tlowest prio still %03x, this one was %03x\n",
                          lowest_th_prio, th_prio);
        }
    }

    HEX_DEBUG_LOG("\tselected lowest prio tid %d, prio %03x (of %d)\n",
                  (&(threads[lowest_prio_index]->env))->threadId,
                  lowest_th_prio, (int)list_size);
    if (low_prio) {
        *low_prio = lowest_th_prio;
    }
    return threads[lowest_prio_index];
}


bool hexagon_thread_ints_enabled(CPUHexagonState *thread_env)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_SSR);
    target_ulong ie = GET_SSR_FIELD(SSR_IE, ssr);
    target_ulong syscfg = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_SYSCFG);
    target_ulong gie = GET_SYSCFG_FIELD(SYSCFG_GIE, syscfg);
    return ie && gie;
}

bool hexagon_int_disabled(CPUHexagonState *global_env, uint32_t int_num)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(global_env, HEX_SREG_IPENDAD);
    target_ulong iad = GET_FIELD(IPENDAD_IAD, ipendad);

    return iad & (1 << int_num);
}

void hexagon_disable_int(CPUHexagonState *env, uint32_t int_num)
{
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong ipendad_iad = GET_FIELD(IPENDAD_IAD, ipendad);
    fSET_FIELD(ipendad, IPENDAD_IAD, ipendad_iad | (1 << int_num));
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_IPENDAD, ipendad);
}

bool hexagon_thread_is_interruptible(CPUHexagonState *thread_env, uint32_t int_num)
{
    target_ulong imask = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_IMASK);
    target_ulong imask_mask = GET_FIELD(IMASK_MASK, imask);

    const uint32_t int_mask = 1 << int_num;

    uint32_t int_is_masked = (imask_mask & int_mask) != 0;

    return hexagon_thread_ints_enabled(thread_env)
        && !hexagon_thread_is_busy(thread_env)
        && !hexagon_int_disabled(thread_env, int_num)
        && !int_is_masked;
}

bool hexagon_thread_is_busy(CPUHexagonState *thread_env)
{
    CPUState *thread_cs = env_cpu(thread_env);

    target_ulong ssr = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_SSR);
    target_ulong ex = GET_SSR_FIELD(SSR_EX, ssr);
    return ex || thread_cs->exception_index != HEX_EVENT_NONE;
}

static bool get_physical_address(CPUHexagonState *env, hwaddr *phys,
                                int *prot, int *size, int32_t *excp,
                                target_ulong address,
                                MMUAccessType access_type, int mmu_idx)

{
    if (hexagon_cpu_mmu_enabled(env)) {
        return hex_tlb_find_match(env, address, access_type, phys, prot,
                                  size, excp, mmu_idx);
    } else {
        *phys = address & 0xFFFFFFFF;
        *prot = PAGE_VALID | PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        *size = TARGET_PAGE_SIZE;
        return true;
    }
}

/* qemu seems to only want to know about TARGET_PAGE_SIZE pages */
static void find_qemu_subpage(vaddr *addr, hwaddr *phys,
                                     int page_size)
{
    vaddr page_start = *addr & ~((vaddr)(page_size - 1));
    vaddr offset = ((*addr - page_start) / TARGET_PAGE_SIZE) *
        TARGET_PAGE_SIZE;
    *addr = page_start + offset;
    *phys += offset;
}

#ifndef CONFIG_USER_ONLY
static hwaddr hexagon_cpu_get_phys_page_debug(CPUState *cs, vaddr addr)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    hwaddr phys_addr;
    int prot;
    int page_size = 0;
    int32_t excp = 0;
    int mmu_idx = MMU_KERNEL_IDX;

    if (get_physical_address(env, &phys_addr, &prot, &page_size, &excp,
                             addr, 0, mmu_idx)) {
        find_qemu_subpage(&addr, &phys_addr, page_size);
        return phys_addr;
    }

    return -1;
}
#endif

#define INVALID_BADVA                                      0xbadabada

static void set_badva_regs(CPUHexagonState *env, target_ulong VA, int slot,
                           MMUAccessType access_type)
{
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA, VA);

    if (access_type == MMU_INST_FETCH || slot == 0) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA0, VA);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA1, INVALID_BADVA);
        SET_SSR_FIELD(env, SSR_V0, 1);
        SET_SSR_FIELD(env, SSR_V1, 0);
        SET_SSR_FIELD(env, SSR_BVS, 0);
    } else if (slot == 1) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA0, INVALID_BADVA);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA1, VA);
        SET_SSR_FIELD(env, SSR_V0, 0);
        SET_SSR_FIELD(env, SSR_V1, 1);
        SET_SSR_FIELD(env, SSR_BVS, 1);
    } else {
        g_assert_not_reached();
    }
}
static void raise_tlbmiss_exception(CPUState *cs, target_ulong VA, int slot,
                                MMUAccessType access_type)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    set_badva_regs(env, VA, slot, access_type);

    switch (access_type) {
    case MMU_INST_FETCH:
        cs->exception_index = HEX_EVENT_TLB_MISS_X;
        if ((VA & ~TARGET_PAGE_MASK) == 0) {
          env->cause_code = HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE;
        }
        else {
          env->cause_code = HEX_CAUSE_TLBMISSX_CAUSE_NORMAL;
        }
        break;
    case MMU_DATA_LOAD:
        cs->exception_index = HEX_EVENT_TLB_MISS_RW;
        env->cause_code = HEX_CAUSE_TLBMISSRW_CAUSE_READ;
        break;
    case MMU_DATA_STORE:
        cs->exception_index = HEX_EVENT_TLB_MISS_RW;
        env->cause_code = HEX_CAUSE_TLBMISSRW_CAUSE_WRITE;
        break;
    }
}

static void raise_perm_exception(CPUState *cs, target_ulong VA, int slot,
                                 MMUAccessType access_type, int32_t excp)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    set_badva_regs(env, VA, slot, access_type);
    cs->exception_index = excp;
}

static const char *access_type_names[] = {
    "MMU_DATA_LOAD ",
    "MMU_DATA_STORE",
    "MMU_INST_FETCH"
};

static const char *mmu_idx_names[] = {
    "MMU_USER_IDX",
    "MMU_GUEST_IDX",
    "MMU_KERNEL_IDX"
};
#endif

#if !defined(CONFIG_USER_ONLY)
static bool hexagon_tlb_fill(CPUState *cs, vaddr address, int size,
                             MMUAccessType access_type, int mmu_idx,
                             bool probe, uintptr_t retaddr)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
#ifdef CONFIG_USER_ONLY
    switch (access_type) {
    case MMU_INST_FETCH:
        cs->exception_index = HEX_EVENT_PRECISE;
        env->cause_code = HEX_CAUSE_FETCH_NO_UPAGE;
        break;
    case MMU_DATA_LOAD:
        cs->exception_index = HEX_EVENT_PRECISE;
        env->cause_code = HEX_CAUSE_PRIV_NO_UREAD;
        break;
    case MMU_DATA_STORE:
        cs->exception_index = HEX_EVENT_PRECISE;
        env->cause_code = HEX_CAUSE_PRIV_NO_UWRITE;
        break;
    }
    cpu_loop_exit_restore(cs, retaddr);
#else
    int slot = env->slot;
    hwaddr phys;
    int prot = 0;
    int page_size = 0;
    int32_t excp = 0;
    bool ret = 0;

    qemu_log_mask(CPU_LOG_MMU,
                  "%s: tid = 0x%x, pc = 0x%08" PRIx32
                  ", vaddr = 0x%08" VADDR_PRIx
                  ", size = %d, %s,\tprobe = %d, %s\n",
                  __func__, env->threadId, env->gpr[HEX_REG_PC], address, size,
                  access_type_names[access_type],
                  probe, mmu_idx_names[mmu_idx]);
    ret = get_physical_address(env, &phys, &prot, &page_size, &excp,
                               address, access_type, mmu_idx);
    if (ret) {
        if (!excp) {
            find_qemu_subpage(&address, &phys, page_size);
            tlb_set_page(cs, address, phys, prot,
                         mmu_idx, TARGET_PAGE_SIZE);
            return ret;
        } else {
            raise_perm_exception(cs, address, slot, access_type, excp);
            do_raise_exception_err(env, cs->exception_index, retaddr);
        }
    }

    raise_tlbmiss_exception(cs, address, slot, access_type);
    do_raise_exception_err(env, cs->exception_index, retaddr);
#endif
}
#endif

#ifndef CONFIG_USER_ONLY
#include "hw/core/sysemu-cpu-ops.h"

static const struct SysemuCPUOps hexagon_sysemu_ops = {
    .get_phys_page_debug = hexagon_cpu_get_phys_page_debug,
};
#endif

#include "hw/core/tcg-cpu-ops.h"

#define CHECK_EX 0
#ifndef CONFIG_USER_ONLY
/* XXX_SM: */
static bool hexagon_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    CPUClass *cc = CPU_GET_CLASS(cs);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    if (interrupt_request & CPU_INTERRUPT_HARD && !hexagon_thread_is_busy(env)) {
        qemu_log_mask(CPU_LOG_INT,
                "got a hard int, full mask is %08x|%d\n",
                (int)interrupt_request, (int)interrupt_request);

        cs->exception_index = HEX_EVENT_INT2;
        cc->tcg_ops->do_interrupt(cs);
        cs->interrupt_request &= ~CPU_INTERRUPT_HARD;
        return true;
    }
    return false;
}

static void QEMU_NORETURN hexagon_cpu_do_unaligned_access(CPUState *cs, vaddr addr,
                                        MMUAccessType access_type,
                                        int mmu_idx,
                                        uintptr_t retaddr)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    cs->exception_index = HEX_EVENT_PRECISE;
    switch (access_type) {
    case MMU_DATA_LOAD:
        env->cause_code = HEX_CAUSE_MISALIGNED_LOAD;
        break;
    case MMU_DATA_STORE:
        env->cause_code = HEX_CAUSE_MISALIGNED_STORE;
        break;
    case MMU_INST_FETCH:
        env->cause_code = HEX_CAUSE_PC_NOT_ALIGNED;
        break;
    default:
        g_assert_not_reached();
    }

    qemu_log_mask(CPU_LOG_MMU,
        "unaligned access %08x from %08x\n", (int)addr, (int)retaddr);

    set_badva_regs(env, addr, 0, access_type);
    do_raise_exception_err(env, cs->exception_index, retaddr);
}

#endif

#ifdef CONFIG_TCG
static struct TCGCPUOps hexagon_tcg_ops = {
    .initialize = hexagon_translate_init,
    .synchronize_from_tb = hexagon_cpu_synchronize_from_tb,

#if !defined(CONFIG_USER_ONLY)
    .tlb_fill = hexagon_tlb_fill,
    .cpu_exec_interrupt = hexagon_cpu_exec_interrupt,
    .do_interrupt = hexagon_cpu_do_interrupt,
    .do_unaligned_access = hexagon_cpu_do_unaligned_access,
#endif /* !CONFIG_USER_ONLY */
};
#endif

static void hexagon_cpu_class_init(ObjectClass *c, void *data)
{
    HexagonCPUClass *mcc = HEXAGON_CPU_CLASS(c);
    CPUClass *cc = CPU_CLASS(c);
    DeviceClass *dc = DEVICE_CLASS(c);

    device_class_set_parent_realize(dc, hexagon_cpu_realize,
                                    &mcc->parent_realize);

    device_class_set_props(dc, hexagon_cpu_properties);
    device_class_set_parent_reset(dc, hexagon_cpu_reset, &mcc->parent_reset);

    cc->class_by_name = hexagon_cpu_class_by_name;
#if !defined(CONFIG_USER_ONLY)
    cc->has_work = hexagon_cpu_has_work;
#endif
    cc->dump_state = hexagon_dump_state;
    cc->set_pc = hexagon_cpu_set_pc;
    cc->gdb_read_register = hexagon_gdb_read_register;
    cc->gdb_write_register = hexagon_gdb_write_register;
    cc->gdb_qreg_info_lines = (const char **)hexagon_qreg_descs;
    cc->gdb_qreg_info_line_count = ARRAY_SIZE(hexagon_qreg_descs);
#if defined(CONFIG_USER_ONLY)
    cc->gdb_num_core_regs = TOTAL_PER_THREAD_REGS + NUM_VREGS + NUM_QREGS;
#else
    cc->gdb_num_core_regs = TOTAL_PER_THREAD_REGS + NUM_VREGS + NUM_QREGS + NUM_SREGS + NUM_GREGS;
#endif
    cc->gdb_stop_before_watchpoint = true;
    cc->disas_set_info = hexagon_cpu_disas_set_info;
#ifndef CONFIG_USER_ONLY
    cc->sysemu_ops = &hexagon_sysemu_ops;
#endif
#ifdef CONFIG_TCG
    cc->tcg_ops = &hexagon_tcg_ops;
#endif
}

#ifndef CONFIG_USER_ONLY
uint32_t hexagon_greg_read(CPUHexagonState *env, uint32_t reg)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    int ssr_ce = GET_SSR_FIELD(SSR_CE, ssr);
    int ssr_pe = GET_SSR_FIELD(SSR_PE, ssr);
    int off;

    if (reg <= HEX_GREG_G3) {
      return env->greg[reg];
    }
    switch (reg) {
    case HEX_GREG_GCYCLE_1T:
    case HEX_GREG_GCYCLE_2T:
    case HEX_GREG_GCYCLE_3T:
    case HEX_GREG_GCYCLE_4T:
    case HEX_GREG_GCYCLE_5T:
    case HEX_GREG_GCYCLE_6T:
        off = reg - HEX_GREG_GCYCLE_1T;
        return ssr_pe ? ARCH_GET_SYSTEM_REG(env, HEX_SREG_GCYCLE_1T + off) : 0;

    case HEX_GREG_GPCYCLELO:
        return ssr_ce ? ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLELO) : 0;

    case HEX_GREG_GPCYCLEHI:
        return ssr_ce ? ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLEHI) : 0;

    case HEX_GREG_GPMUCNT0:
    case HEX_GREG_GPMUCNT1:
    case HEX_GREG_GPMUCNT2:
    case HEX_GREG_GPMUCNT3:
        off = HEX_GREG_GPMUCNT3 - reg;
        return ARCH_GET_SYSTEM_REG(env, HEX_SREG_PMUCNT0 + off);
    default:
        return 0;
    }
}
#endif

#define DEFINE_CPU(type_name, initfn)      \
    {                                      \
        .name = type_name,                 \
        .parent = TYPE_HEXAGON_CPU,        \
        .instance_init = initfn            \
    }

static const TypeInfo hexagon_cpu_type_infos[] = {
    {
        .name = TYPE_HEXAGON_CPU,
        .parent = TYPE_CPU,
        .instance_size = sizeof(HexagonCPU),
        .instance_init = hexagon_cpu_init,
        .abstract = true,
        .class_size = sizeof(HexagonCPUClass),
        .class_init = hexagon_cpu_class_init,
    },
    DEFINE_CPU(TYPE_HEXAGON_CPU_GENERIC,          hexagon_common_cpu_init),
    DEFINE_CPU(TYPE_HEXAGON_CPU_V67,              hexagon_common_cpu_init),
};

DEFINE_TYPES(hexagon_cpu_type_infos)
