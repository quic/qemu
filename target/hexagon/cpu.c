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
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "cpu_helper.h"
#include "internal.h"
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
#ifdef CONFIG_USER_ONLY
        env->gpr[HEX_REG_USR] = 0x56000;
#endif

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
    qemu_fprintf(f, "  %s = 0x%x\n", str, val);
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

    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n", hexagon_regnames[regnum],
                 value);
}

#ifndef CONFIG_USER_ONLY
static void print_greg(FILE *f, CPUHexagonState *env, int regnum)
{
    target_ulong val = env->greg[regnum];
    qemu_fprintf(f, "  %s = 0x" TARGET_FMT_lx "\n", hexagon_gregnames[regnum],
                 val);
}

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
#endif

static void print_vreg(FILE *f, CPUHexagonState *env, int regnum)
{
    int i;
    qemu_fprintf(f, "  v%d = (", regnum);
    qemu_fprintf(f, "0x%02x", env->VRegs[regnum].ub[MAX_VEC_SIZE_BYTES - 1]);
    for (i = MAX_VEC_SIZE_BYTES - 2; i >= 0; i--) {
        qemu_fprintf(f, ", 0x%02x", env->VRegs[regnum].ub[i]);
    }
    qemu_fprintf(f, ")\n");
}

void hexagon_debug_vreg(CPUHexagonState *env, int regnum)
{
    print_vreg(stdout, env, regnum);
}

static void print_qreg(FILE *f, CPUHexagonState *env, int regnum)
{
    int i;
    qemu_fprintf(f, "  q%d = (", regnum);
    qemu_fprintf(f, ", 0x%02x",
                 env->QRegs[regnum].ub[MAX_VEC_SIZE_BYTES / 8 - 1]);
    for (i = MAX_VEC_SIZE_BYTES / 8 - 2; i >= 0; i--) {
        qemu_fprintf(f, ", 0x%02x", env->QRegs[regnum].ub[i]);
    }
    qemu_fprintf(f, ")\n");
}

void hexagon_debug_qreg(CPUHexagonState *env, int regnum)
{
    print_qreg(stdout, env, regnum);
}

void hexagon_dump(CPUHexagonState *env, FILE *f)
{
    static target_ulong last_pc;

    /*
     * When comparing with LLDB, it doesn't step through single-cycle
     * hardware loops the same way.  So, we just skip them here
     */
    if (env->gpr[HEX_REG_PC] == last_pc) {
        return;
    }
    last_pc = env->gpr[HEX_REG_PC];
#ifdef CONFIG_USER_ONLY
    qemu_fprintf(f, "General Purpose Registers = {\n");
#else
    qemu_fprintf(f, "TID %d : General Purpose Registers = {\n", env->threadId);
#endif

    for (int i = 0; i < 32; i++) {
        print_reg(f, env, i);
    }

#ifdef CONFIG_USER_ONLY
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
    /*
     * Not modelled in user mode, print junk to minimize the diff's
     * with LLDB output
     */
    qemu_fprintf(f, "  cause = 0x000000db\n");
    qemu_fprintf(f, "  badva = 0x00000000\n");
    qemu_fprintf(f, "  cs0 = 0x00000000\n");
    qemu_fprintf(f, "  cs1 = 0x00000000\n");
#else
    print_reg(f, env, HEX_REG_SA0);
    print_reg(f, env, HEX_REG_LC0);
    print_reg(f, env, HEX_REG_SA1);
    print_reg(f, env, HEX_REG_LC1);
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
    qemu_fprintf(f, "  gevb = 0x00000000\n");
    qemu_fprintf(f, "  gelr = 0x00000000\n");
    qemu_fprintf(f, "  gsr = 0x00000000\n");
    qemu_fprintf(f, "  gosp = 0x00000000\n");
    qemu_fprintf(f, "  gbadva = 0x00000000\n");
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

/*
 * The HVX register dump takes up a ton of space in the log
 * Don't print it unless it is needed
 */
#define DUMP_HVX 0
#if DUMP_HVX
    qemu_fprintf(f, "Vector Registers = {\n");
    for (int i = 0; i < NUM_VREGS; i++) {
        print_vreg(f, env, i);
    }
    for (int i = 0; i < NUM_QREGS; i++) {
        print_qreg(f, env, i);
    }
    qemu_fprintf(f, "}\n");
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

#ifndef CONFIG_USER_ONLY
    CPUHexagonState *env = &cpu->env;
    SET_SSR_FIELD(env, SSR_EX, 1);
#endif
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

static target_ulong ssr_get_ex(CPUHexagonState *env)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    ssr = (ssr & 1<<17) >> 17;
    return ssr;
}

static void hexagon_cpu_set_irq(void *opaque, int irq, int level)
{
    HexagonCPU *cpu = opaque;
    CPUHexagonState *env = &cpu->env;
    CPUState *cs = CPU(cpu);


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
            [HEXAGON_CPU_IRQ_0] = 0x0,
            [HEXAGON_CPU_IRQ_1] = 0x1,
            [HEXAGON_CPU_IRQ_2] = 0x2,
            [HEXAGON_CPU_IRQ_3] = 0x3,
            [HEXAGON_CPU_IRQ_4] = 0x4,
            [HEXAGON_CPU_IRQ_5] = 0x5,
            [HEXAGON_CPU_IRQ_6] = 0x6,
            [HEXAGON_CPU_IRQ_7] = 0x7,
    };

    if (level && (irq == HEXAGON_CPU_IRQ_2)) {
/*      printf("XXX: L2VIC: irq = %d, level = %d\n", irq, level); */
        while (ssr_get_ex(env)) {rcu_read_lock(); printf ("x"); rcu_read_unlock();};
        /* NOTE: when level is not zero it is equal to the external IRQ # */
        env->g_sreg[HEX_SREG_VID] = level;
        env->cause_code = HEX_CAUSE_INT2;
    }

    /*
     * XXX_SM: The VID must match the IRQ number must match the one
     *         in hexagon_testboard.c when setting up the "qutimer"
     */

    switch (irq) {
    case HEXAGON_CPU_IRQ_0 ... HEXAGON_CPU_IRQ_7:
      //  printf ("%s: IRQ irq:%d, level:%d\n", __func__, irq, level);
        if (level) {
            cpu_interrupt(cs, mask[irq]);
        } else {
            cpu_reset_interrupt(cs, mask[irq]);
        }
        break;
    default:
        g_assert_not_reached();
    }
}

static void hexagon_cpu_init(Object *obj)
{
    HexagonCPU *cpu = HEXAGON_CPU(obj);

    cpu_set_cpustate_pointers(cpu);
    // At he the moment only qtimer XXX_SM
    qdev_init_gpio_in(DEVICE(cpu), hexagon_cpu_set_irq, 8);
}


#ifndef CONFIG_USER_ONLY

void hexagon_raise_interrupt(CPUHexagonState *env, HexagonCPU *thread,
                             uint32_t int_num)
{
    hexagon_raise_interrupt_resume(env, thread, int_num, 0);
}

void hexagon_raise_interrupt_resume(CPUHexagonState *env, HexagonCPU *thread,
                                    uint32_t int_num, target_ulong resume_pc)
{
    // This logic is for interrupt numbers 0-15 only
    assert(int_num <= 15);

    const uint32_t int_mask = 1 << int_num;
    CPUHexagonState *thread_env = &(HEXAGON_CPU(thread)->env);
    CPUState *cs = CPU(thread);
    cs->exception_index = HEX_EVENT_INT0 + int_num;
    thread_env->cause_code = HEX_CAUSE_INT0 + int_num;

    if (get_exe_mode(thread_env) == HEX_EXE_MODE_WAIT) {
        clear_wait_mode(thread_env);
        cpu_loop_exit_restore(cs, env->gpr[HEX_REG_PC]);
    }
    target_ulong ipendad = ARCH_GET_SYSTEM_REG(env, HEX_SREG_IPENDAD);
    target_ulong ipendad_iad = GET_FIELD(IPENDAD_IAD, ipendad) | int_mask;
    HEX_DEBUG_LOG("%s: tid %d handling interrupt %d, ipend_iad 0x%x\n",
                  __FUNCTION__, thread_env->threadId, int_num, ipendad_iad);
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
            ints[*list_size++] = intnum;
        }
    }
}

void hexagon_find_int_threads(CPUHexagonState *env, uint32_t int_num,
                              HexagonCPU *threads[], size_t *list_size)
{
    // This logic is for interrupt numbers 0-15 only
    assert(int_num <= 15);
    const uint32_t interrupt_mask = 1 << int_num;

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
        target_ulong ssr = ARCH_GET_SYSTEM_REG(chk_env, HEX_SREG_SSR);
        target_ulong ie = GET_SSR_FIELD(SSR_IE, ssr);
        target_ulong ex = GET_SSR_FIELD(SSR_EX, ssr);
        HEX_DEBUG_LOG("%s: checking tid = %d, cpu %p, env %p "
                      "ssr 0x%x ie %u, ex %u\n",
                      __FUNCTION__, chk_env->threadId, chk_cpu, chk_env, syscfg,
                      ie, ex);
        if (!ie || ex) {
            HEX_DEBUG_LOG("%s: !ie %d || ex %d: tid %d: "
                          "pc 0x%x: continue\n",
                          __FUNCTION__, ie, ex, chk_env->threadId,
                          ARCH_GET_THREAD_REG(chk_env, HEX_REG_PC));
            continue;
        }
        target_ulong imask = ARCH_GET_SYSTEM_REG(chk_env, HEX_SREG_IMASK);
        target_ulong imask_mask = GET_FIELD(IMASK_MASK, imask);
        HEX_DEBUG_LOG("%s: tid %d: imask 0x%x, "
                      "imask_mask 0x%x, mask 0x%x\n",
                      __FUNCTION__, chk_env->threadId, imask, imask_mask,
                      interrupt_mask);
        if (imask_mask & interrupt_mask) {
            HEX_DEBUG_LOG("%s: imask & mask: continue\n", __FUNCTION__);
            continue;
        }
        HEX_DEBUG_LOG("%s: adding tid %d to list\n", __FUNCTION__,
                      chk_env->threadId);
        threads[*list_size++] = chk_cpu;
    }
}

HexagonCPU *hexagon_find_lowest_prio_any_thread(HexagonCPU *threads[],
                                                size_t list_size)
{
    return hexagon_find_lowest_prio_thread(threads, list_size, false);
}

HexagonCPU *hexagon_find_lowest_prio_waiting_thread(HexagonCPU *threads[],
                                                    size_t list_size)
{
    return hexagon_find_lowest_prio_thread(threads, list_size, true);
}

HexagonCPU *hexagon_find_lowest_prio_thread(HexagonCPU *threads[],
                                            size_t list_size, bool only_waiters)
{
    assert(list_size >= 1);
    uint32_t lowest_th_prio = 0;
    size_t lowest_prio_index = 0;
    for (size_t i = 0; i < list_size; i++) {
        CPUHexagonState *thread_env = &(threads[i]->env);
        uint32_t th_prio = GET_FIELD(
            STID_PRIO, ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_STID));
        const bool waiting = (get_exe_mode(thread_env) == HEX_EXE_MODE_WAIT);
        const bool is_candidate = (only_waiters && waiting) || !only_waiters;
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
    return threads[lowest_prio_index];
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
        cs->exception_index = HEX_EVENT_TLB_MISS_X;
        env->cause_code = HEX_CAUSE_TLBMISSX_CAUSE_NORMAL;
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


#ifndef CONFIG_USER_ONLY
/* XXX_SM: */
static bool hexagon_cpu_exec_interrupt(CPUState *cs, int interrupt_request)
{
    CPUClass *cc = CPU_GET_CLASS(cs);
    cs->exception_index = HEX_EVENT_INT2;
    cs->interrupt_request &= ~interrupt_request;
    cc->do_interrupt(cs);
    return true;
}
#endif

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
    cc->cpu_exec_interrupt = hexagon_cpu_exec_interrupt;
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
