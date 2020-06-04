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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "qapi/error.h"
#include "migration/vmstate.h"
#ifndef CONFIG_USER_ONLY
#include "macros.h"
#include "hex_mmu.h"
#endif

static void hexagon_v67_cpu_init(Object *obj)
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

const char * const hexagon_regnames[TOTAL_PER_THREAD_REGS] = {
    "r0", "r1",  "r2",  "r3",  "r4",   "r5",  "r6",  "r7",
    "r8", "r9",  "r10", "r11", "r12",  "r13", "r14", "r15",
    "r16", "r17", "r18", "r19", "r20",  "r21", "r22", "r23",
    "r24", "r25", "r26", "r27", "r28",  "r29", "r30", "r31",
    "sa0", "lc0", "sa1", "lc1", "p3_0", "c5",  "m0",  "m1",
    "usr", "pc",  "ugp", "gp",  "cs0",  "cs1", "c14", "c15",
    "c16", "c17", "c18", "c19", "pkt_cnt",  "insn_cnt", "hvx_cnt", "c23",
    "c24", "c25", "c26", "c27", "c28",  "c29", "c30", "c31",
};

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

/*
 * One of the main debugging techniques is to use "-d cpu" and compare against
 * LLDB output when single stepping.  However, the target and qemu put the
 * stacks at different locations.  This is used to compensate so the diff is
 * cleaner.
 */
static inline target_ulong hack_stack_ptrs(CPUHexagonState *env,
                                           target_ulong addr)
{
    static bool first = true;
    if (first) {
        first = false;
        env->stack_start = env->gpr[HEX_REG_SP];
        env->gpr[HEX_REG_USR] = 0x56000;

#define ADJUST_STACK 0
#if ADJUST_STACK
        /*
         * Change the two numbers below to
         *     1    qemu stack location
         *     2    hardware stack location
         * Or set to zero for normal mode (no stack adjustment)
         */
        env->stack_adjust = 0xfffeeb80 - 0xbf89f980;
#else
        env->stack_adjust = 0;
#endif
    }

    target_ulong stack_start = env->stack_start;
    target_ulong stack_size = 0x10000;
    target_ulong stack_adjust = env->stack_adjust;

    if (stack_start + 0x1000 >= addr && addr >= (stack_start - stack_size)) {
        return addr - stack_adjust;
    }
    return addr;
}

/* HEX_REG_P3_0 (aka C4) is an alias for the predicate registers */
static inline target_ulong read_p3_0(CPUHexagonState *env)
{
    int32_t control_reg = 0;
    int i;
    for (i = NUM_PREGS - 1; i >= 0; i--) {
        control_reg <<= 8;
        control_reg |= env->pred[i] & 0xff;
    }
    return control_reg;
}

#ifndef CONFIG_USER_ONLY
static void print_val(FILE *f, const char *str, unsigned val)
{
    fprintf(f, "  %s = 0x%x\n", str, val);
}
#endif

static void print_reg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong value;

    if (regnum == HEX_REG_P3_0) {
        value = read_p3_0(env);
    } else {
        value = regnum < 32 ? hack_stack_ptrs(env, env->gpr[regnum])
                            : env->gpr[regnum];
    }

    fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n",
        hexagon_regnames[regnum], value);
}

#ifndef CONFIG_USER_ONLY
static void print_greg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = env->greg[regnum];
    fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n",
        hexagon_gregnames[regnum], val);
}

static void print_sreg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = ARCH_GET_SYSTEM_REG(env, regnum);
    fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n",
        hexagon_sregnames[regnum], val);
}

static target_ulong get_badva(CPUHexagonState *env)

{
  if (ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR) & SSR_BVS) {
      return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA1);
  }
  else {
      return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA0);
  }
}
#endif

static void print_vreg(FILE *f, CPUHexagonState *env, int regnum)
{
    int i;
    fprintf(f, "  v%d = (", regnum);
    fprintf(f, "0x%02x", env->VRegs[regnum].ub[MAX_VEC_SIZE_BYTES - 1]);
    for (i = MAX_VEC_SIZE_BYTES - 2; i >= 0; i--) {
        fprintf(f, ", 0x%02x", env->VRegs[regnum].ub[i]);
    }
    fprintf(f, ")\n");
}

void hexagon_debug_vreg(CPUHexagonState *env, int regnum)
{
    print_vreg(stdout, env, regnum);
}

static void print_qreg(FILE *f, CPUHexagonState *env, int regnum)
{
    int i;
    fprintf(f, "  q%d = (", regnum);
    fprintf(f, ", 0x%02x",
                env->QRegs[regnum].ub[MAX_VEC_SIZE_BYTES / 8 - 1]);
    for (i = MAX_VEC_SIZE_BYTES / 8 - 2; i >= 0; i--) {
        fprintf(f, ", 0x%02x", env->QRegs[regnum].ub[i]);
    }
    fprintf(f, ")\n");
}

void hexagon_debug_qreg(CPUHexagonState *env, int regnum)
{
    print_qreg(stdout, env, regnum);
}

void hexagon_dump(CPUHexagonState *env, FILE *f)
{
    static target_ulong last_pc;
    int i;

    /*
     * When comparing with LLDB, it doesn't step through single-cycle
     * hardware loops the same way.  So, we just skip them here
     */
    if (env->gpr[HEX_REG_PC] == last_pc) {
        return;
    }
    last_pc = env->gpr[HEX_REG_PC];
    fprintf(f, "TID %d : General Purpose Registers = {\n", env->threadId);
    for (i = 0; i < 32; i++) {
        print_reg(f, env, i);
    }
    print_reg(f, env, HEX_REG_SA0);
    print_reg(f, env, HEX_REG_LC0);
    print_reg(f, env, HEX_REG_SA1);
    print_reg(f, env, HEX_REG_LC1);
    print_reg(f, env, HEX_REG_M0);
    print_reg(f, env, HEX_REG_M1);
    print_reg(f, env, HEX_REG_USR);
    print_reg(f, env, HEX_REG_P3_0);
    print_reg(f, env, HEX_REG_GP);
    print_reg(f, env, HEX_REG_UGP);
    print_reg(f, env, HEX_REG_PC);
#ifdef CONFIG_USER_ONLY
    /*
     * Not modelled in user mode, print junk to minimize the diff's
     * with LLDB output
     */
    fprintf(f, "  cause = 0x000000db\n");
    fprintf(f, "  badva = 0x00000000\n");
    fprintf(f, "  cs0 = 0x00000000\n");
    fprintf(f, "  cs1 = 0x00000000\n");
#else
    print_val(f, "cause", ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR) & SSR_CAUSE);
    print_sreg(f, env, get_badva(env));
    print_reg(f, env, HEX_REG_CS0);
    print_reg(f, env, HEX_REG_CS1);

    for (int j = 0; j < NUM_PREGS ; ++j) {
      char buf[128];
      buf[0]= 0;
      sprintf(buf, "p%d", j);
      print_val(f, buf, env->pred[j]);
    }

    print_sreg(f, env, HEX_SREG_IPENDAD);
    print_sreg(f, env, HEX_SREG_IMASK);
    print_sreg(f, env, HEX_SREG_ELR);

    print_greg(f, env, HEX_GREG_G0);
    print_greg(f, env, HEX_GREG_G1);
    print_greg(f, env, HEX_GREG_G2);
    print_greg(f, env, HEX_GREG_G3);
#endif
    fprintf(f, "}\n");

/*
 * The HVX register dump takes up a ton of space in the log
 * Don't print it unless it is needed
 */
#define DUMP_HVX 0
#if DUMP_HVX
    fprintf(f, "Vector Registers = {\n");
    for (i = 0; i < NUM_VREGS; i++) {
        print_vreg(f, env, i);
    }
    for (i = 0; i < NUM_QREGS; i++) {
        print_qreg(f, env, i);
    }
    fprintf(f, "}\n");
#endif
}

static void hexagon_dump_state(CPUState *cs, FILE *f, int flags)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    hexagon_dump(env, f);
}

void hexagon_debug(CPUHexagonState *env)
{
    hexagon_dump(env, stdout);
}

static void hexagon_cpu_set_pc(CPUState *cs, vaddr value)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->gpr[HEX_REG_PC] = value;
}

static void hexagon_cpu_synchronize_from_tb(CPUState *cs, TranslationBlock *tb)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->gpr[HEX_REG_PC] = tb->pc;
}

static bool hexagon_cpu_has_work(CPUState *cs)
{
    return true;
}

void restore_state_to_opc(CPUHexagonState *env, TranslationBlock *tb,
                          target_ulong *data)
{
    env->gpr[HEX_REG_PC] = data[0];
}

static void hexagon_cpu_reset(DeviceState *dev)
{
    CPUState *cs = CPU(dev);
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    HexagonCPUClass *mcc = HEXAGON_CPU_GET_CLASS(cpu);

    mcc->parent_reset(dev);
}

static void hexagon_cpu_disas_set_info(CPUState *s, disassemble_info *info)
{
    info->print_insn = print_insn_hexagon;
}

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

    qemu_init_vcpu(cs);
    cpu_reset(cs);

    mcc->parent_realize(dev, errp);
}

static void hexagon_cpu_init(Object *obj)
{
    HexagonCPU *cpu = HEXAGON_CPU(obj);

    cpu_set_cpustate_pointers(cpu);
}

#ifndef CONFIG_USER_ONLY
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
static inline void find_qemu_subpage(vaddr *addr, hwaddr *phys,
                                     int page_size)
{
    vaddr page_start = *addr & ~((vaddr)(page_size - 1));
    vaddr offset = ((*addr - page_start) / TARGET_PAGE_SIZE) *
        TARGET_PAGE_SIZE;
    *addr = page_start + offset;
    *phys += offset;
}

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

static void raise_tlbmiss_exception(CPUState *cs, target_ulong VA,
                                MMUAccessType access_type)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA, VA);

    switch (access_type) {
    case MMU_INST_FETCH:
        cs->exception_index = HEX_CAUSE_TLBMISSX_CAUSE_NORMAL;
        break;
    case MMU_DATA_LOAD:
        cs->exception_index = HEX_CAUSE_TLBMISSRW_CAUSE_READ;
        break;
    case MMU_DATA_STORE:
        cs->exception_index = HEX_CAUSE_TLBMISSRW_CAUSE_WRITE;
        break;
    }
}

static void raise_perm_exception(CPUState *cs, target_ulong VA, int32_t excp)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA, VA);
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

static bool hexagon_tlb_fill(CPUState *cs, vaddr address, int size,
                             MMUAccessType access_type, int mmu_idx,
                             bool probe, uintptr_t retaddr)
{
#ifdef CONFIG_USER_ONLY
    switch (access_type) {
    case MMU_INST_FETCH:
        cs->exception_index = HEX_CAUSE_FETCH_NO_UPAGE;
        break;
    case MMU_DATA_LOAD:
        cs->exception_index = HEX_CAUSE_PRIV_NO_UREAD;
        break;
    case MMU_DATA_STORE:
        cs->exception_index = HEX_CAUSE_PRIV_NO_UWRITE;
        break;
    }
    cpu_loop_exit_restore(cs, retaddr);
#else
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
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
            raise_perm_exception(cs, address, excp);
            do_raise_exception_err(env, cs->exception_index, retaddr);
        }
    }

    raise_tlbmiss_exception(cs, address, access_type);
    do_raise_exception_err(env, cs->exception_index, retaddr);
#endif
}

static const VMStateDescription vmstate_hexagon_cpu = {
    .name = "cpu",
    .unmigratable = 1,
};

static void hexagon_cpu_class_init(ObjectClass *c, void *data)
{
    HexagonCPUClass *mcc = HEXAGON_CPU_CLASS(c);
    CPUClass *cc = CPU_CLASS(c);
    DeviceClass *dc = DEVICE_CLASS(c);

    device_class_set_parent_realize(dc, hexagon_cpu_realize,
                                    &mcc->parent_realize);

    device_class_set_parent_reset(dc, hexagon_cpu_reset, &mcc->parent_reset);

    cc->class_by_name = hexagon_cpu_class_by_name;
    cc->has_work = hexagon_cpu_has_work;
#ifndef CONFIG_USER_ONLY
    cc->do_interrupt = hexagon_cpu_do_interrupt;
#endif
    cc->dump_state = hexagon_dump_state;
    cc->set_pc = hexagon_cpu_set_pc;
    cc->synchronize_from_tb = hexagon_cpu_synchronize_from_tb;
    cc->gdb_read_register = hexagon_gdb_read_register;
    cc->gdb_write_register = hexagon_gdb_write_register;
#ifdef CONFIG_USER_ONLY
    cc->gdb_num_core_regs = TOTAL_PER_THREAD_REGS + NUM_VREGS + NUM_QREGS;
#else
    cc->gdb_num_core_regs = TOTAL_PER_THREAD_REGS + NUM_VREGS + NUM_QREGS + NUM_SREGS + NUM_GREGS;
#endif
    cc->gdb_stop_before_watchpoint = true;
    cc->disas_set_info = hexagon_cpu_disas_set_info;
#ifndef CONFIG_USER_ONLY
    cc->get_phys_page_debug = hexagon_cpu_get_phys_page_debug;
#endif
#ifdef CONFIG_TCG
    cc->tcg_initialize = hexagon_translate_init;
    cc->tlb_fill = hexagon_tlb_fill;
#endif
    /* For now, mark unmigratable: */
    cc->vmsd = &vmstate_hexagon_cpu;
}

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
    DEFINE_CPU(TYPE_HEXAGON_CPU_V67,              hexagon_v67_cpu_init),
};

DEFINE_TYPES(hexagon_cpu_type_infos)
