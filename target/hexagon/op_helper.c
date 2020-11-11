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
#include "stall.h"
#include "macros.h"
#include "arch.h"
#include "fma_emu.h"
#include "conv_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros.h"
#include "arch_options_calc.h"
#include "hmx/hmx.h"
#include "hmx/macros.h"
#include "hmx/ext_hmx.h"
#include "hmx/mpy_hmx.h"
#include "system.h"
#include "dma_adapter.h"
#include "fma_emu.h"
#include "myfenv.h"
#include "conv_emu.h"
#ifndef CONFIG_USER_ONLY
#include "hex_mmu.h"
#endif
#include "op_helper.h"

#if COUNT_HEX_HELPERS
#include "opcodes.h"

typedef struct {
    int count;
    const char *tag;
} helper_count_t;

helper_count_t helper_counts[] = {
#define OPCODE(TAG)    { 0, #TAG },
#include "opcodes_def_generated.h"
#undef OPCODE
    { 0, NULL }
};

#define COUNT_HELPER(TAG) \
    do { \
        helper_counts[(TAG)].count++; \
    } while (0)

void print_helper_counts(void)
{
    helper_count_t *p;

    printf("HELPER COUNTS\n");
    for (p = helper_counts; p->tag; p++) {
        if (p->count) {
            printf("\t%d\t\t%s\n", p->count, p->tag);
        }
    }
}
#else
#define COUNT_HELPER(TAG)              /* Nothing */
#endif

/* Exceptions processing helpers */
void QEMU_NORETURN do_raise_exception_err(CPUHexagonState *env,
                                          uint32_t exception,
                                          uintptr_t pc)
{
    CPUState *cs = CPU(hexagon_env_get_cpu(env));
    qemu_log_mask(CPU_LOG_INT, "%s: %d, @ %08x | %08lx\n", __func__, exception,
            env->gpr[HEX_REG_PC],
        pc);

#if 0
    if (cs->exception_index != HEX_EVENT_NONE)
        fprintf(stderr, "old exception_index: 0x%02x, new exception_index 0x%02x, cc: 0x%02x\n",
            cs->exception_index, exception,
            env->cause_code);
    assert(cs->exception_index == HEX_EVENT_NONE ||
           cs->exception_index == 6);
#endif

    cs->exception_index = exception;
    cpu_loop_exit_restore(cs, pc);
#ifndef CONFIG_USER_ONLY
    while(1)
      ;
#endif
}

void HELPER(raise_exception)(CPUHexagonState *env, uint32_t exception)
{
    do_raise_exception_err(env, exception, 0);
}

#ifndef CONFIG_USER_ONLY
void HELPER(raise_stack_overflow)(CPUHexagonState *env, uint32_t slot,
                                  target_ulong badva)
{
    /*
     * Per section 7.3.1 of the V67 Programmer's Reference,
     * stack limit exception isn't raised in monotor mode.
     */
    if (sys_in_monitor_mode(env)) {
        return;
    }

    CPUState *cs = env_cpu(env);
    cs->exception_index = HEX_EVENT_PRECISE;
    env->cause_code = HEX_CAUSE_STACK_LIMIT;

    if (slot == 0) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA0, badva);
        SET_SSR_FIELD(env, SSR_V0, 1);
        SET_SSR_FIELD(env, SSR_V1, 0);
        SET_SSR_FIELD(env, SSR_BVS, 0);
    } else if (slot == 1) {
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_BADVA1, badva);
        SET_SSR_FIELD(env, SSR_V0, 0);
        SET_SSR_FIELD(env, SSR_V1, 1);
        SET_SSR_FIELD(env, SSR_BVS, 1);
    } else {
        g_assert_not_reached();
    }
    cpu_loop_exit_restore(cs, 0);
}
#endif

static inline void log_reg_write(CPUHexagonState *env, int rnum,
                                 target_ulong val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_reg_write[%d] = " TARGET_FMT_ld
      " (0x" TARGET_FMT_lx ")", rnum, val, val);
    if (env->slot_cancelled & (1 << slot)) {
        HEX_DEBUG_LOG(" CANCELLED");
    }
    if (val == env->gpr[rnum]) {
        HEX_DEBUG_LOG(" NO CHANGE");
    }
    HEX_DEBUG_LOG("\n");
    if (!(env->slot_cancelled & (1 << slot))) {
        HEX_DEBUG_LOG("\treg register set to 0x%x\n", val);
        env->new_value[rnum] = val;
#if HEX_DEBUG
        /* Do this so HELPER(debug_commit_end) will know */
        env->reg_written[rnum] = 1;
#endif
    }
}

static __attribute__((unused))
inline void log_reg_write_pair(CPUHexagonState *env, int rnum,
                                      int64_t val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_reg_write_pair[%d:%d] = %ld\n", rnum + 1, rnum, val);
    log_reg_write(env, rnum, val & 0xFFFFFFFF, slot);
    log_reg_write(env, rnum + 1, (val >> 32) & 0xFFFFFFFF, slot);
}

#ifndef CONFIG_USER_ONLY
static inline void log_sreg_write(CPUHexagonState *env, int rnum,
                                 target_ulong val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_sreg_write[%s/%d] = " TARGET_FMT_ld
        " (0x" TARGET_FMT_lx ")", hexagon_sregnames[rnum], rnum, val, val);
    if (env->slot_cancelled & (1 << slot)) {
        HEX_DEBUG_LOG(" CANCELLED");
    }

    target_ulong crnt_val = ARCH_GET_SYSTEM_REG(env, rnum);
    if (val == crnt_val) {
        HEX_DEBUG_LOG(" NO CHANGE");
    }
    HEX_DEBUG_LOG("\n");
    if (!(env->slot_cancelled & (1 << slot))) {
        HEX_DEBUG_LOG("\tsreg register set to 0x%x\n", val);
        if (rnum < HEX_SREG_GLB_START) {
            env->t_sreg_new_value[rnum] = val;
        } else {
            env->g_sreg[rnum] = val;
        }
    }
    else
      HEX_DEBUG_LOG("\tsreg register not set\n");
}

#endif

static inline void log_pred_write(CPUHexagonState *env, int pnum,
                                  target_ulong val)
{
    HEX_DEBUG_LOG("log_pred_write[%d] = " TARGET_FMT_ld
                  " (0x" TARGET_FMT_lx ")\n",
                  pnum, val, val);

    /* Multiple writes to the same preg are and'ed together */
    if (env->pred_written & (1 << pnum)) {
        env->new_pred_value[pnum] &= val & 0xff;
    } else {
        env->new_pred_value[pnum] = val & 0xff;
        env->pred_written |= 1 << pnum;
    }
}

static inline void write_new_pc(CPUHexagonState *env, target_ulong addr)
{
    HEX_DEBUG_LOG("write_new_pc(0x" TARGET_FMT_lx ")\n", addr);

    /*
     * If more than one branch is taken in a packet, only the first one
     * is actually done.
     */
    if (env->branch_taken) {
        HEX_DEBUG_LOG("INFO: multiple branches taken in same packet, "
                      "ignoring the second one\n");
    } else {
        fCHECK_PCALIGN(addr);
        env->branch_taken = 1;
        env->next_PC = addr;
    }
}

/* Handy place to set a breakpoint */
void HELPER(debug_start_packet)(CPUHexagonState *env)
{
    HEX_DEBUG_LOG("Start packet: pc = 0x" TARGET_FMT_lx " tid = %d\n",
                  env->gpr[HEX_REG_PC], env->threadId);

    int i;
    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        env->reg_written[i] = 0;
    }
#if !defined(CONFIG_USER_ONLY) && HEX_DEBUG
    for (i = 0; i < NUM_GREGS; i++) {
        env->greg_written[i] = 0;
    }

    for (i = 0; i < NUM_SREGS; i++) {
        if (i < HEX_SREG_GLB_START)
            env->t_sreg_written[i] = 0;
    }
#endif
}

static inline int32_t new_pred_value(CPUHexagonState *env, int pnum)
{
    return env->new_pred_value[pnum];
}

/* Checks for bookkeeping errors between disassembly context and runtime */
void HELPER(debug_check_store_width)(CPUHexagonState *env, int slot, int check)
{
    if (env->mem_log_stores[slot].width != check) {
        HEX_DEBUG_LOG("ERROR: %d != %d\n",
                      env->mem_log_stores[slot].width, check);
        g_assert_not_reached();
    }
}

void hexagon_load_byte(CPUHexagonState *env, uint8_t *load_byte,
    target_ulong src_vaddr)

{
#ifdef CONFIG_USER_ONLY
  uint8_t B;
  get_user_u8(B, src_vaddr);
  *load_byte = B;
#else
#if 1
  unsigned mmu_idx = cpu_mmu_index(env, false);
  *load_byte = cpu_ldub_mmuidx_ra(env, src_vaddr, mmu_idx, GETPC());
#else
  unsigned mmu_idx = cpu_mmu_index(env, false);
  uint8_t *hostaddr = (uint_8_t *)tlb_vaddr_to_host(env,
                                                    src_vaddr,
                                                    MMU_DATA_LOAD,
                                                    mmu_idx);
  if (hostaddr)
    *load_byte = *hostaddr;
  else {
    TCGMemOpIdx oi = make_memop_idx(MO_UB, mmu_idx);
    *load_byte = helper_ret_ldub_mmu(env, src_vaddr, oi, GETPC());
  }
#endif
#endif
}

void hexagon_store_byte(CPUHexagonState *env, uint8_t store_byte,
    target_ulong dst_vaddr)

{
#ifdef CONFIG_USER_ONLY
  put_user_u8(store_byte, dst_vaddr);
#else
#if 1
  unsigned mmu_idx = cpu_mmu_index(env, false);
  cpu_stb_mmuidx_ra(env, dst_vaddr, store_byte, mmu_idx, GETPC());
#else
  unsigned mmu_idx = cpu_mmu_index(env, false);
  uint8_t *hostaddr = (uint_8_t *)tlb_vaddr_to_host(env,
                                                    dst_vaddr,
                                                    MMU_DATA_STORE,
                                                    mmu_idx);
  if (hostaddr)
    *hostaddr = store_byte;
  else {
    TCGMemOpIdx oi = make_memop_idx(MO_UB, mmu_idx);
    helper_ret_stb_mmu(env, dst_vaddr, store_byte, oi, GETPC());
  }
#endif
#endif
}


void HELPER(commit_hvx_stores)(CPUHexagonState *env)
{
    int i;

    /* Normal (possibly masked) vector store */
    for (i = 0; i < VSTORES_MAX; i++) {
        if (env->vstore_pending[i]) {
            env->vstore_pending[i] = 0;
            target_ulong va = env->vstore[i].va;
            int size = env->vstore[i].size;
            for (int j = 0; j < size; j++) {
                if (env->vstore[i].mask.ub[j]) {
                    hexagon_store_byte(env, env->vstore[i].data.ub[j], va + j);
                }
            }
        }
    }

    /* Scatter store */
    if (env->vtcm_pending) {
        env->vtcm_pending = 0;
        if (env->vtcm_log.op) {
            /* Need to perform the scatter read/modify/write at commit time */
            if (env->vtcm_log.op_size == 2) {
                SCATTER_OP_WRITE_TO_MEM(size2u_t);
            } else if (env->vtcm_log.op_size == 4) {
                /* Word Scatter += */
                SCATTER_OP_WRITE_TO_MEM(size4u_t);
            } else {
                g_assert_not_reached();
            }
        } else {
            for (int i = 0; i < env->vtcm_log.size; i++) {
                if (env->vtcm_log.mask.ub[i] != 0) {
                    hexagon_store_byte(env, env->vtcm_log.data.ub[i],
                        env->vtcm_log.va[i]);
                    env->vtcm_log.mask.ub[i] = 0;
                    env->vtcm_log.data.ub[i] = 0;
                    env->vtcm_log.offsets.ub[i] = 0;
                }

            }
        }
    }
}

static void print_store(CPUHexagonState *env, int slot)
{
    if (!(env->slot_cancelled & (1 << slot))) {
        size1u_t width = env->mem_log_stores[slot].width;
        if (width == 1) {
            size4u_t data = env->mem_log_stores[slot].data32 & 0xff;
            HEX_DEBUG_LOG("\tmemb[0x" TARGET_FMT_lx "] = %d (0x%02x)\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 2) {
            size4u_t data = env->mem_log_stores[slot].data32 & 0xffff;
            HEX_DEBUG_LOG("\tmemh[0x" TARGET_FMT_lx "] = %d (0x%04x)\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 4) {
            size4u_t data = env->mem_log_stores[slot].data32;
            HEX_DEBUG_LOG("\tmemw[0x" TARGET_FMT_lx "] = %d (0x%08x)\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 8) {
            HEX_DEBUG_LOG("\tmemd[0x" TARGET_FMT_lx "] = %lu (0x%016lx)\n",
                          env->mem_log_stores[slot].va,
                          env->mem_log_stores[slot].data64,
                          env->mem_log_stores[slot].data64);
        } else {
            HEX_DEBUG_LOG("\tBad store width %d\n", width);
            g_assert_not_reached();
        }
    }
}

/* This function is a handy place to set a breakpoint */
void HELPER(debug_commit_end)(CPUHexagonState *env, int has_st0, int has_st1)
{
    bool reg_printed = false;
    bool pred_printed = false;
    int i;

    HEX_DEBUG_LOG("Packet committed: tid = %d, pc = 0x" TARGET_FMT_lx "\n",
                  env->threadId, env->this_PC);
    if (env->slot_cancelled)
      HEX_DEBUG_LOG("slot_cancelled = %d\n", env->slot_cancelled);

    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        if (env->reg_written[i]) {
            if (!reg_printed) {
                HEX_DEBUG_LOG("Regs written\n");
                reg_printed = true;
            }
            HEX_DEBUG_LOG("\tr%d = " TARGET_FMT_ld " (0x" TARGET_FMT_lx " )\n",
                          i, env->new_value[i], env->new_value[i]);
        }
    }

#if !defined(CONFIG_USER_ONLY) && HEX_DEBUG
    bool greg_printed = false;
    for (i = 0; i < NUM_GREGS; i++) {
        if (env->greg_written[i]) {
            if (!greg_printed) {
                HEX_DEBUG_LOG("GRegs written\n");
                greg_printed = true;
            }
            HEX_DEBUG_LOG("\tset g%d (%s) = " TARGET_FMT_ld
                " (0x" TARGET_FMT_lx " )\n",
                i, hexagon_gregnames[i], env->greg_new_value[i],
                env->greg_new_value[i]);
        }
    }

    bool sreg_printed = false;
    for (i = 0; i < NUM_SREGS; i++) {
        if (i < HEX_SREG_GLB_START) {
            if (env->t_sreg_written[i]) {
                if (!sreg_printed) {
                    HEX_DEBUG_LOG("SRegs written\n");
                    sreg_printed = true;
                }
                HEX_DEBUG_LOG("\tset s%d (%s) = " TARGET_FMT_ld
                    " (0x" TARGET_FMT_lx " )\n",
                    i, hexagon_sregnames[i], env->t_sreg_new_value[i],
                    env->t_sreg_new_value[i]);
            }
        }
    }
#endif

    for (i = 0; i < NUM_PREGS; i++) {
        if (env->pred_written & (1 << i)) {
            if (!pred_printed) {
                HEX_DEBUG_LOG("Predicates written\n");
                pred_printed = true;
            }
            HEX_DEBUG_LOG("\tp%d = 0x" TARGET_FMT_lx "\n",
                          i, env->new_pred_value[i]);
        }
    }

    if (has_st0 || has_st1) {
        HEX_DEBUG_LOG("Stores\n");
        if (has_st0) {
            print_store(env, 0);
        }
        if (has_st1) {
            print_store(env, 1);
        }
    }

    HEX_DEBUG_LOG("tid[%d], Next PC = 0x%x\n", env->threadId, env->next_PC);
    HEX_DEBUG_LOG("Exec counters: pkt = " TARGET_FMT_lx
                  ", insn = " TARGET_FMT_lx
                  ", hvx = " TARGET_FMT_lx "\n",
                  env->gpr[HEX_REG_QEMU_PKT_CNT],
                  env->gpr[HEX_REG_QEMU_INSN_CNT],
                  env->gpr[HEX_REG_QEMU_HVX_CNT]);

}

static int32_t fcircadd_v4(int32_t RxV, int32_t offset, int32_t M, int32_t CS)
{
    int32_t length = M & 0x0001ffff;
    uint32_t new_ptr = RxV + offset;
    uint32_t start_addr = CS;
    uint32_t end_addr = start_addr + length;

    if (new_ptr >= end_addr) {
        new_ptr -= length;
    } else if (new_ptr < start_addr) {
        new_ptr += length;
    }

    return new_ptr;
}

int32_t HELPER(fcircadd)(int32_t RxV, int32_t offset, int32_t M, int32_t CS)
{
    int32_t K_const = (M >> 24) & 0xf;
    int32_t length = M & 0x1ffff;
    int32_t mask = (1 << (K_const + 2)) - 1;
    uint32_t new_ptr = RxV + offset;
    uint32_t start_addr = RxV & (~mask);
    uint32_t end_addr = start_addr | length;

    if (K_const == 0 && length >= 4) {
        return fcircadd_v4(RxV, offset, M, CS);
    }

    if (new_ptr >= end_addr) {
        new_ptr -= length;
    } else if (new_ptr < start_addr) {
        new_ptr += length;
    }

    return new_ptr;
}

/*
 * sfrecipa, sfinvsqrta, vacsh have two results
 *     r0,p0=sfrecipa(r1,r2)
 *     r0,p0=sfinvsqrta(r1)
 *     r1:0,p0=vacsh(r3:2,r5:4)
 * Since helpers can only return a single value, we have two helpers
 * for each of these. They each contain basically the same code (copy/pasted
 * from the arch library), but one returns the register and the other
 * returns the predicate.
 */
int32_t HELPER(sfrecipa_val)(CPUHexagonState *env, int32_t RsV, int32_t RtV)
{
    fFPOP_START();
    /* int32_t PeV; Not needed to compute value */
    int32_t RdV;
    fHIDE(int idx;)
    fHIDE(int adjust;)
    fHIDE(int mant;)
    fHIDE(int exp;)
    if (fSF_RECIP_COMMON(RsV, RtV, RdV, adjust)) {
        /* PeV = adjust; Not needed to compute value */
        idx = (RtV >> 16) & 0x7f;
        mant = (fSF_RECIP_LOOKUP(idx) << 15) | 1;
        exp = fSF_BIAS() - (fSF_GETEXP(RtV) - fSF_BIAS()) - 1;
        RdV = fMAKESF(fGETBIT(31, RtV), exp, mant);
    }
    fFPOP_END();
    return RdV;
}

int32_t HELPER(sfrecipa_pred)(CPUHexagonState *env, int32_t RsV, int32_t RtV)
{
    fFPOP_START();
    int32_t PeV = 0;
    int32_t RdV;
    fHIDE(int idx;)
    fHIDE(int adjust;)
    fHIDE(int mant;)
    fHIDE(int exp;)
    if (fSF_RECIP_COMMON(RsV, RtV, RdV, adjust)) {
        PeV = adjust;
        idx = (RtV >> 16) & 0x7f;
        mant = (fSF_RECIP_LOOKUP(idx) << 15) | 1;
        exp = fSF_BIAS() - (fSF_GETEXP(RtV) - fSF_BIAS()) - 1;
        RdV = fMAKESF(fGETBIT(31, RtV), exp, mant);
    }
    fFPOP_END();
    return PeV;
}

int32_t HELPER(sfinvsqrta_val)(CPUHexagonState *env, int32_t RsV)
{
    fFPOP_START();
    /* int32_t PeV; Not needed for val version */
    int32_t RdV;
    fHIDE(int idx;)
    fHIDE(int adjust;)
    fHIDE(int mant;)
    fHIDE(int exp;)
    if (fSF_INVSQRT_COMMON(RsV, RdV, adjust)) {
        /* PeV = adjust; Not needed for val version */
        idx = (RsV >> 17) & 0x7f;
        mant = (fSF_INVSQRT_LOOKUP(idx) << 15);
        exp = fSF_BIAS() - ((fSF_GETEXP(RsV) - fSF_BIAS()) >> 1) - 1;
        RdV = fMAKESF(fGETBIT(31, RsV), exp, mant);
    }
    fFPOP_END();
    return RdV;
}

int32_t HELPER(sfinvsqrta_pred)(CPUHexagonState *env, int32_t RsV)
{
    fFPOP_START();
    int32_t PeV = 0;
    int32_t RdV;
    fHIDE(int idx;)
    fHIDE(int adjust;)
    fHIDE(int mant;)
    fHIDE(int exp;)
    if (fSF_INVSQRT_COMMON(RsV, RdV, adjust)) {
        PeV = adjust;
        idx = (RsV >> 17) & 0x7f;
        mant = (fSF_INVSQRT_LOOKUP(idx) << 15);
        exp = fSF_BIAS() - ((fSF_GETEXP(RsV) - fSF_BIAS()) >> 1) - 1;
        RdV = fMAKESF(fGETBIT(31, RsV), exp, mant);
    }
    fFPOP_END();
    return PeV;
}

int64_t HELPER(vacsh_val)(CPUHexagonState *env,
                           int64_t RxxV, int64_t RssV, int64_t RttV)
{
    int32_t PeV = 0;
    fHIDE(int i;)
    fHIDE(int xv;)
    fHIDE(int sv;)
    fHIDE(int tv;)
    for (i = 0; i < 4; i++) {
        xv = (int)fGETHALF(i, RxxV);
        sv = (int)fGETHALF(i, RssV);
        tv = (int)fGETHALF(i, RttV);
        xv = xv + tv;
        sv = sv - tv;
        fSETBIT(i * 2, PeV, (xv > sv));
        fSETBIT(i * 2 + 1, PeV, (xv > sv));
        fSETHALF(i, RxxV, fSATH(fMAX(xv, sv)));
    }
    return RxxV;
}

int32_t HELPER(vacsh_pred)(CPUHexagonState *env,
                           int64_t RxxV, int64_t RssV, int64_t RttV)
{
    int32_t PeV = 0;
    fHIDE(int i;)
    fHIDE(int xv;)
    fHIDE(int sv;)
    fHIDE(int tv;)
    for (i = 0; i < 4; i++) {
        xv = (int)fGETHALF(i, RxxV);
        sv = (int)fGETHALF(i, RssV);
        tv = (int)fGETHALF(i, RttV);
        xv = xv + tv;
        sv = sv - tv;
        fSETBIT(i * 2, PeV, (xv > sv));
        fSETBIT(i * 2 + 1, PeV, (xv > sv));
        fSETHALF(i, RxxV, fSATH(fMAX(xv, sv)));
    }
    return PeV;
}

/*
 * Handle mem_noshuf
 *
 * This occurs when there is a load that might need data forwarded
 * from an inflight store in slot 1.  Note that the load and store
 * might have different sizes, so we can't simply compare the
 * addresses.  We merge only the bytes that overlap (if any).
 */
static int64_t merge_bytes(CPUHexagonState *env, target_ulong load_addr,
                           int64_t load_data, uint32_t load_width)
{
    /* Don't do anything if slot 1 was cancelled */
    const int store_slot = 1;
    if (env->slot_cancelled & (1 << store_slot)) {
        return load_data;
    }

    int store_width = env->mem_log_stores[store_slot].width;
    target_ulong store_addr = env->mem_log_stores[store_slot].va;
    union {
        uint8_t bytes[8];
        uint32_t data32;
        uint64_t data64;
    } retdata, storedata;
    int bigmask = ((-load_width) & (-store_width));
    if ((load_addr & bigmask) != (store_addr & bigmask)) {
        /* no overlap */
        return load_data;
    }
    retdata.data64 = load_data;

    if (store_width == 1 || store_width == 2 || store_width == 4) {
        storedata.data32 = env->mem_log_stores[store_slot].data32;
    } else if (store_width == 8) {
        storedata.data64 = env->mem_log_stores[store_slot].data64;
    } else {
        g_assert_not_reached();
    }
    int i, j;
    i = store_addr & (load_width - 1);
    j = load_addr & (store_width - 1);
    while ((i < load_width) && (j < store_width)) {
        retdata.bytes[i] = storedata.bytes[j];
        i++;
        j++;
    }
    return retdata.data64;
}

#define MERGE_INFLIGHT(NAME, RET, IN_TYPE, OUT_TYPE, SIZE) \
RET HELPER(NAME)(CPUHexagonState *env, int32_t addr, IN_TYPE data) \
{ \
    return (OUT_TYPE)merge_bytes(env, addr, data, SIZE); \
}

MERGE_INFLIGHT(merge_inflight_store1s, int32_t, int32_t,  int8_t,  1)
MERGE_INFLIGHT(merge_inflight_store1u, int32_t, int32_t, uint8_t,  1)
MERGE_INFLIGHT(merge_inflight_store2s, int32_t, int32_t,  int16_t, 2)
MERGE_INFLIGHT(merge_inflight_store2u, int32_t, int32_t, uint16_t, 2)
MERGE_INFLIGHT(merge_inflight_store4s, int32_t, int32_t,  int32_t, 4)
MERGE_INFLIGHT(merge_inflight_store4u, int32_t, int32_t, uint32_t, 4)
MERGE_INFLIGHT(merge_inflight_store8u, int64_t, int64_t,  int64_t, 8)

#ifndef CONFIG_USER_ONLY
void HELPER(modify_ssr)(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    hexagon_modify_ssr(env, new, old);
}

void HELPER(checkforguest)(CPUHexagonState *env)
{
    if (!(sys_in_guest_mode(env) || sys_in_monitor_mode(env))) {
        env->cause_code = HEX_CAUSE_PRIV_USER_NO_GINSN;
        helper_raise_exception(env, HEX_EVENT_PRECISE);
    }
}

void HELPER(checkforpriv)(CPUHexagonState *env)
{
    if (!sys_in_monitor_mode(env)) {
        env->cause_code = HEX_CAUSE_PRIV_USER_NO_SINSN;
        helper_raise_exception(env, HEX_EVENT_PRECISE);
    }
}

static inline void probe_store(CPUHexagonState *env, int slot, int mmu_idx)
{
    if (!(env->slot_cancelled & (1 << slot))) {
        size1u_t width = env->mem_log_stores[slot].width;
        target_ulong va = env->mem_log_stores[slot].va;
        probe_write(env, va, width, mmu_idx, env->gpr[HEX_REG_PC]);
    }
}

static inline void probe_hvx_stores(CPUHexagonState *env, int mmu_idx)
{
    uintptr_t retaddr = env->gpr[HEX_REG_PC];
    int i;

    /* Normal (possibly masked) vector store */
    for (i = 0; i < VSTORES_MAX; i++) {
        if (env->vstore_pending[i]) {
            target_ulong va = env->vstore[i].va;
            int size = env->vstore[i].size;
            for (int j = 0; j < size; j++) {
                if (env->vstore[i].mask.ub[j]) {
                    probe_write(env, va + j, 1, mmu_idx, retaddr);
                }
            }
        }
    }

    /* Scatter store */
    if (env->vtcm_pending) {
        if (env->vtcm_log.op) {
            /* Need to perform the scatter read/modify/write at commit time */
            if (env->vtcm_log.op_size == 2) {
                SCATTER_OP_PROBE_MEM(size2u_t, mmu_idx, retaddr);
            } else if (env->vtcm_log.op_size == 4) {
                /* Word Scatter += */
                SCATTER_OP_PROBE_MEM(size4u_t, mmu_idx, retaddr);
            } else {
                g_assert_not_reached();
            }
        } else {
            for (int i = 0; i < env->vtcm_log.size; i++) {
                if (env->vtcm_log.mask.ub[i] != 0) {
                    probe_write(env, env->vtcm_log.va[i], 1, mmu_idx, retaddr);
                }

            }
        }
    }
}

void HELPER(probe_pkt_stores)(CPUHexagonState *env, int has_st0, int has_st1,
                              int has_dczeroa, int has_hvx_stores, int mmu_idx)
{
    if (has_st0 && !has_dczeroa) {
        probe_store(env, 0, mmu_idx);
    }
    if (has_st1 && !has_dczeroa) {
        probe_store(env, 1, mmu_idx);
    }
    if (has_dczeroa) {
        /* Probe 32 bytes starting at (dczero_addr & ~0x1f) */
        target_ulong va = env->dczero_addr & ~0x1f;
        probe_write(env, va +  0, 8, mmu_idx, env->gpr[HEX_REG_PC]);
        probe_write(env, va +  8, 8, mmu_idx, env->gpr[HEX_REG_PC]);
        probe_write(env, va + 16, 8, mmu_idx, env->gpr[HEX_REG_PC]);
        probe_write(env, va + 24, 8, mmu_idx, env->gpr[HEX_REG_PC]);
    }
    if (has_hvx_stores) {
        probe_hvx_stores(env, mmu_idx);
    }
}

#if HEX_DEBUG
static inline void print_thread(const char *str, CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *thread = &cpu->env;
    bool is_stopped = cpu_is_stopped(cs);
    int exe_mode = get_exe_mode(thread);
    hex_lock_state_t lock_state = thread->k0_lock_state;
    HEX_DEBUG_LOG("%s: threadId = %d: %s, exe_mode = %s, k0_lock_state = %s\n",
           str,
           thread->threadId,
           is_stopped ? "stopped" : "running",
           exe_mode == HEX_EXE_MODE_OFF ? "off" :
           exe_mode == HEX_EXE_MODE_RUN ? "run" :
           exe_mode == HEX_EXE_MODE_WAIT ? "wait" :
           exe_mode == HEX_EXE_MODE_DEBUG ? "debug" :
           "unknown",
           lock_state == HEX_LOCK_UNLOCKED ? "unlocked" :
           lock_state == HEX_LOCK_WAITING ? "waiting" :
           lock_state == HEX_LOCK_OWNER ? "owner" :
           "unknown");
}

static inline void print_thread_states(const char *str)
{
    CPUState *cs;
    CPU_FOREACH(cs) {
        print_thread(str, cs);
    }
}
#else
static inline void print_thread(const char *str, CPUState *cs)
{
}
static inline void print_thread_states(const char *str)
{
}
#endif

static void hex_k0_lock(CPUHexagonState *env)
{
    HEX_DEBUG_LOG("Before hex_k0_lock: %d\n", env->threadId);
    print_thread_states("\tThread");

    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    if (GET_SYSCFG_FIELD(SYSCFG_K0LOCK, syscfg)) {
        if (env->k0_lock_state == HEX_LOCK_OWNER) {
            HEX_DEBUG_LOG("Already the owner\n");
            return;
        }
        HEX_DEBUG_LOG("\tWaiting\n");
        env->k0_lock_state = HEX_LOCK_WAITING;
        cpu_stop_current();
    } else {
        HEX_DEBUG_LOG("\tAcquired\n");
        env->k0_lock_state = HEX_LOCK_OWNER;
        SET_SYSCFG_FIELD(env, SYSCFG_K0LOCK, 1);
    }

    HEX_DEBUG_LOG("After hex_k0_lock: %d\n", env->threadId);
    print_thread_states("\tThread");
}

static void hex_k0_unlock(CPUHexagonState *env)
{
    HEX_DEBUG_LOG("Before hex_k0_unlock: %d\n", env->threadId);
    print_thread_states("\tThread");

    /* Nothing to do if the k0 isn't locked by this thread */
    uint32_t syscfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG);
    if ((GET_SYSCFG_FIELD(SYSCFG_K0LOCK, syscfg) == 0) ||
         (env->k0_lock_state != HEX_LOCK_OWNER)) {
        HEX_DEBUG_LOG("\tNot owner\n");
        return;
    }

    env->k0_lock_state = HEX_LOCK_UNLOCKED;
    SET_SYSCFG_FIELD(env, SYSCFG_K0LOCK, 0);

    /* Look for a thread to unlock */
    unsigned int this_threadId = env->threadId;
    CPUHexagonState *unlock_thread = NULL;
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread = &cpu->env;

        /*
         * The hardware implements round-robin fairness, so we look for threads
         * starting at env->threadId + 1 and incrementing modulo the number of
         * threads.
         *
         * To implement this, we check if thread is a earlier in the modulo
         * sequence than unlock_thread.
         *     if unlock thread is higher than this thread
         *         thread must be between this thread and unlock_thread
         *     else
         *         thread higher than this thread is ahead of unlock_thread
         *         thread must be lower then unlock thread
         */
        if (thread->k0_lock_state == HEX_LOCK_WAITING) {
            if (!unlock_thread) {
                unlock_thread = thread;
            } else if (unlock_thread->threadId > this_threadId) {
                if (this_threadId < thread->threadId &&
                    thread->threadId < unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            } else {
                if (thread->threadId > this_threadId) {
                    unlock_thread = thread;
                }
                if (thread->threadId < unlock_thread->threadId) {
                    unlock_thread = thread;
                }
            }
        }
    }
    if (unlock_thread) {
        cs = CPU(hexagon_env_get_cpu(unlock_thread));
        print_thread("\tWaiting thread found", cs);
        unlock_thread->k0_lock_state = HEX_LOCK_OWNER;
        SET_SYSCFG_FIELD(unlock_thread, SYSCFG_K0LOCK, 1);
        cpu_resume(cs);
    }

    HEX_DEBUG_LOG("After hex_k0_unlock: %d\n", env->threadId);
    print_thread_states("\tThread");
}
#endif

/* Helpful for printing intermediate values within instructions */
void HELPER(debug_value)(CPUHexagonState *env, int32_t value)
{
    HEX_DEBUG_LOG("value = 0x%x\n", value);
}

void HELPER(debug_value_i64)(CPUHexagonState *env, int64_t value)
{
    HEX_DEBUG_LOG("value_i64 = 0x%lx\n", value);
}

/* Log a write to HVX vector */
static inline void log_vreg_write(CPUHexagonState *env, int num, void *var,
                                      int vnew, uint32_t slot)
{
    HEX_DEBUG_LOG("log_vreg_write[%d]", num);
    if (env->slot_cancelled & (1 << slot)) {
        HEX_DEBUG_LOG(" CANCELLED");
    }
    HEX_DEBUG_LOG("\n");

    if (!(env->slot_cancelled & (1 << slot))) {
        VRegMask regnum_mask = ((VRegMask)1) << num;
        env->VRegs_updated |=      (vnew != EXT_TMP) ? regnum_mask : 0;
        env->VRegs_select |=       (vnew == EXT_NEW) ? regnum_mask : 0;
        env->VRegs_updated_tmp  |= (vnew == EXT_TMP) ? regnum_mask : 0;
        env->future_VRegs[num] = *(mmvector_t *)var;
        if (vnew == EXT_TMP) {
            env->tmp_VRegs[num] = env->future_VRegs[num];
        }
    }
}

static inline void log_mmvector_write(CPUHexagonState *env, int num,
                                      mmvector_t var, int vnew, uint32_t slot)
{
    log_vreg_write(env, num, &var, vnew, slot);
}

static void cancel_slot(CPUHexagonState *env, uint32_t slot)
{
    HEX_DEBUG_LOG("Slot %d cancelled\n", slot);
    env->slot_cancelled |= (1 << slot);
}

#ifndef CONFIG_USER_ONLY
void HELPER(fwait)(CPUHexagonState *env, uint32_t mask)

{
    /* mask is unsued */
    hexagon_wait_thread(env);
}

void HELPER(fresume)(CPUHexagonState *env, uint32_t mask)

{
    hexagon_resume_threads(env, mask);
}

void HELPER(fstart)(CPUHexagonState *env, uint32_t mask)

{
    hexagon_start_threads(env, mask);
}

void HELPER(clear_run_mode)(CPUHexagonState *env, uint32_t mask)

{
    /* mask is unused */
    hexagon_stop_thread(env);
}

void HELPER(pause)(CPUHexagonState *env, uint32_t val)

{
    CPUState *cs = CPU(hexagon_env_get_cpu(env));

    /* Just let another CPU run.  */
    cs->exception_index = EXCP_INTERRUPT;
    cpu_loop_exit(cs);
}

void HELPER(iassignw)(CPUHexagonState *env, uint32_t src)

{
    HEX_DEBUG_LOG("%s: tid %d, src 0x%x\n",
        __FUNCTION__, env->threadId, src);
    uint32_t modectl =
        ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    CPUState *cpu = NULL;
    CPU_FOREACH (cpu) {
        CPUHexagonState *thread_env = &(HEXAGON_CPU(cpu)->env);
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if (thread_enabled_mask & thread_id_mask) {
            uint32_t imask = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_IMASK);
            //uint32_t imask_mask = GET_FIELD(IMASK_MASK, imask);
            uint32_t intbitpos = (src >> 16) & 0xF;
            fINSERT_BITS(imask, 1, intbitpos,
                (src >> thread_env->threadId) & 0x1);
            ARCH_SET_SYSTEM_REG(thread_env, HEX_SREG_IMASK, imask);
            HEX_DEBUG_LOG("%s: tid %d, read imask 0x%x, intbitpos %u, "
              "set bitval %u, new imask 0x%x\n",
              __FUNCTION__,
              thread_env->threadId, imask, intbitpos,
              (src >> thread_env->threadId) & 0x1,
              ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_IMASK));
        }
    }
}

uint32_t HELPER(iassignr)(CPUHexagonState *env, uint32_t src)

{
    uint32_t modectl =
        ARCH_GET_SYSTEM_REG(env, HEX_SREG_MODECTL);
    uint32_t thread_enabled_mask = GET_FIELD(MODECTL_E, modectl);
    /* src fields are in same position as modectl, but mean different things */
    uint32_t intbitpos = GET_FIELD(MODECTL_W, src);
    uint32_t dest_reg = 0;
    CPUState *cpu = NULL;
    CPU_FOREACH (cpu) {
        CPUHexagonState *thread_env = &(HEXAGON_CPU(cpu)->env);
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if (thread_enabled_mask & thread_id_mask) {
            uint32_t imask = ARCH_GET_SYSTEM_REG(thread_env, HEX_SREG_IMASK);
            dest_reg |= ((imask >> intbitpos) & 0x1) << thread_env->threadId;
        }
    }
    return dest_reg;
}

uint32_t HELPER(creg_read)(CPUHexagonState *env, uint32_t reg)
{
    uint32_t low, high;
    switch (reg) {
    case HEX_REG_PKTCNTLO:
        low = ARCH_GET_THREAD_REG(env, HEX_REG_QEMU_PKT_CNT);
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTLO, low);
        return low;
    case HEX_REG_PKTCNTHI:
        high = 0;
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTHI, high);
        return high;
    case HEX_REG_UTIMERLO:
        hexagon_read_timer(&low, &high);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERLO, low);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERHI, high);
        return low;
    case HEX_REG_UTIMERHI:
        hexagon_read_timer(&low, &high);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERLO, low);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERHI, high);
        return high;
    default:
        g_assert_not_reached();
    }
    return 0;
}
uint64_t HELPER(creg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    uint32_t low, high;
    if (reg == HEX_REG_PKTCNTLO) {
        low = ARCH_GET_THREAD_REG(env, HEX_REG_QEMU_PKT_CNT);
        high = 0;
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTLO, low);
        ARCH_SET_THREAD_REG(env, HEX_REG_PKTCNTHI, high);
        return low | (uint64_t)high << 32;
    } else if (reg == HEX_REG_UTIMERLO) {
        hexagon_read_timer(&low, &high);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERLO, low);
        ARCH_SET_THREAD_REG(env, HEX_REG_UTIMERHI, high);
        return low | (uint64_t)high << 32;
    } else {
        g_assert_not_reached();
    }
}

uint32_t HELPER(sreg_read)(CPUHexagonState *env, uint32_t reg)
{
    if ((reg == HEX_SREG_TIMERLO) || (reg == HEX_SREG_TIMERHI)) {
        uint32_t low = 0;
        uint32_t high = 0;
        hexagon_read_timer(&low, &high);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERLO, low);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERHI, high);
    } else if (reg == HEX_SREG_BADVA) {
        target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
        if (GET_SSR_FIELD(SSR_BVS, ssr)) {
            return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA1);
        }
        return ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA0);
    }
    return ARCH_GET_SYSTEM_REG(env, reg);
}

uint64_t HELPER(sreg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    if (reg == HEX_SREG_TIMERLO) {
        uint32_t low = 0;
        uint32_t high = 0;
        hexagon_read_timer(&low, &high);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERLO, low);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_TIMERHI, high);
    }
    return   (uint64_t)ARCH_GET_SYSTEM_REG(env, reg) |
           (((uint64_t)ARCH_GET_SYSTEM_REG(env, reg+1)) << 32);
}

static inline void modify_syscfg(CPUHexagonState *env, uint32_t val)
{
    /* Check for change in MMU enable */
    target_ulong old_mmu_enable =
        GET_SYSCFG_FIELD(SYSCFG_MMUEN, env->g_sreg[HEX_SREG_SYSCFG]);
    target_ulong new_mmu_enable =
        GET_SYSCFG_FIELD(SYSCFG_MMUEN, val);
    if (new_mmu_enable && !old_mmu_enable) {
        hex_mmu_on(env);
    } else if (!new_mmu_enable && old_mmu_enable) {
        hex_mmu_off(env);
    }

    /* Changing pcycle enable from 0 to 1 resets the counters */
    uint32_t old = env->g_sreg[HEX_SREG_SYSCFG];
    uint8_t old_en = GET_SYSCFG_FIELD(SYSCFG_PCYCLEEN, old);
    uint8_t new_en = GET_SYSCFG_FIELD(SYSCFG_PCYCLEEN, val);
    if (old_en == 0 && new_en == 1) {
        env->g_sreg[HEX_SREG_PCYCLELO] = 0;
        env->g_sreg[HEX_SREG_PCYCLEHI] = 0;
    }
}

void HELPER(sreg_write)(CPUHexagonState *env, uint32_t reg, uint32_t val)

{
    if (reg == HEX_SREG_SYSCFG) {
        modify_syscfg(env, val);
    } else if (reg == HEX_SREG_IMASK) {
        val = GET_FIELD(IMASK_MASK, val);
    }
    ARCH_SET_SYSTEM_REG(env, reg, val);
}

void HELPER(sreg_write_pair)(CPUHexagonState *env, uint32_t reg, uint64_t val)

{
    ARCH_SET_SYSTEM_REG(env, reg, val & 0xFFFFFFFF);
    ARCH_SET_SYSTEM_REG(env, reg+1, val >> 32);
}

uint32_t HELPER(greg_read)(CPUHexagonState *env, uint32_t reg)

{
    return hexagon_greg_read(env, reg);
}

uint64_t HELPER(greg_read_pair)(CPUHexagonState *env, uint32_t reg)

{
    int off;

    if (reg == HEX_GREG_G0 || reg == HEX_GREG_G2) {
        return   (uint64_t)(env->greg[reg]) |
               (((uint64_t)(env->greg[reg+1])) << 32);
    }
    switch (reg) {
        case HEX_GREG_GPCYCLELO:
        {
            target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
            int ssr_ce = GET_SSR_FIELD(SSR_CE, ssr);
            target_ulong pcyclelo = ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLELO);
            target_ulong pcyclehi = ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLEHI);
            return ssr_ce ? (uint64_t)pcyclelo | (uint64_t)pcyclehi << 32 : 0;
        }
        case HEX_GREG_GPMUCNT0:
        case HEX_GREG_GPMUCNT2:
            off = HEX_GREG_GPMUCNT3 - reg;
            return (uint64_t)ARCH_GET_SYSTEM_REG(env, HEX_SREG_PMUCNT0+off) |
                (uint64_t)(ARCH_GET_SYSTEM_REG(env, HEX_SREG_PMUCNT0+off+1))
                           << 32;
        default:
            return 0;
    }
}

uint32_t HELPER(getimask)(CPUHexagonState *env, uint32_t tid)

{
    HEX_DEBUG_LOG("%s: tid %u, for tid %u\n",
        __FUNCTION__, env->threadId, tid);
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *found_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *found_env = &found_cpu->env;
        if (found_env->threadId == tid) {
            target_ulong imask = ARCH_GET_SYSTEM_REG(found_env,HEX_SREG_IMASK);
            HEX_DEBUG_LOG("%s: tid %d, found it, imask = 0x%x\n",
                __FUNCTION__, env->threadId,
                (unsigned)GET_FIELD(IMASK_MASK, imask));
            return GET_FIELD(IMASK_MASK, imask);
        }
    }
    return 0;
}
void HELPER(setprio)(CPUHexagonState *env, uint32_t thread, uint32_t prio)

{
    thread = thread & (THREADS_MAX-1);
    HEX_DEBUG_LOG("%s: tid %u, setting thread 0x%x, prio 0x%x\n",
      __FUNCTION__, env->threadId, thread, prio);
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *found_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *found_env = &found_cpu->env;
        if (thread == found_env->threadId) {
            SET_SYSTEM_FIELD(found_env, HEX_SREG_STID, STID_PRIO, prio);
            HEX_DEBUG_LOG("%s: tid[%d].PRIO = 0x%x\n",
                __FUNCTION__, found_env->threadId, prio);
            return;
        }
    }
    g_assert_not_reached();
}

void HELPER(setimask)(CPUHexagonState *env, uint32_t pred, uint32_t imask)

{
    HEX_DEBUG_LOG("%s: tid %u, pred 0x%x, imask 0x%x\n",
      __FUNCTION__, env->threadId, pred, imask);
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *found_cpu = HEXAGON_CPU(cs);
        CPUHexagonState *found_env = &found_cpu->env;

        if (pred & (0x1 << found_env->threadId)) {
            SET_SYSTEM_FIELD(found_env, HEX_SREG_IMASK, IMASK_MASK, imask);
            HEX_DEBUG_LOG("%s: tid %d, found it, imask 0x%x\n",
                __FUNCTION__, found_env->threadId, imask);
            return;
        }
    }
}

typedef struct {
    CPUState *cs;
    CPUHexagonState *env;
} thread_entry;


void HELPER(swi)(CPUHexagonState *env, uint32_t mask)
{
    uint32_t ints_asserted[32];
    size_t ints_asserted_count = 0;
    HexagonCPU *threads[THREADS_MAX];
    memset(threads, 0, sizeof(threads));

    hexagon_set_interrupts(env, mask);
    hexagon_find_asserted_interrupts(env, ints_asserted, sizeof(ints_asserted),
                                     &ints_asserted_count);

    for (size_t i = 0; i < ints_asserted_count; i++) {
        const uint32_t int_num = ints_asserted[i];
        size_t threads_count = 0;
        hexagon_find_int_threads(env, int_num, threads, &threads_count);
        if (threads_count < 1) {
            continue;
        }

        HexagonCPU *int_thread =
            hexagon_find_lowest_prio_any_thread(threads, threads_count, int_num, NULL);
        if (!int_thread) {
            continue;
        }
        hexagon_raise_interrupt(env, int_thread, int_num, 0);
    }

    /* exit loop if current thread was selected to handle a swi int */
    CPUState *current_cs = env_cpu(env);
    if (current_cs->exception_index >= HEX_EVENT_INT0 &&
        current_cs->exception_index <= HEX_EVENT_INTF) {
        HEX_DEBUG_LOG("%s: current selected %p\n", __FUNCTION__, current_cs);
        cpu_loop_exit(current_cs);
    }
}

void HELPER(resched)(CPUHexagonState *env)
{
    const uint32_t schedcfg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SCHEDCFG);
    const uint32_t schedcfg_en = GET_FIELD(SCHEDCFG_EN, schedcfg);
    const int int_number = GET_FIELD(SCHEDCFG_INTNO, schedcfg);
    CPUState *cs = CPU(hexagon_env_get_cpu(env));
    if (!schedcfg_en || hexagon_int_disabled(env, int_number)) {
        return;
    }

    uint32_t lowest_th_prio = 0; /* 0 is highest prio */
    HexagonCPU *threads[THREADS_MAX];
    memset(threads, 0, sizeof(threads));
    size_t i = 0;
    CPU_FOREACH(cs) {
        threads[i++] = HEXAGON_CPU(cs);
    }
    HexagonCPU *int_thread =
        hexagon_find_lowest_prio_any_thread(threads, i, int_number, &lowest_th_prio);

    if (!int_thread) {
        return;
    }

    CPUHexagonState *int_thread_env = &int_thread->env;
    uint32_t bestwait_reg = ARCH_GET_SYSTEM_REG(env, HEX_SREG_BESTWAIT);
    uint32_t best_prio = GET_FIELD(BESTWAIT_PRIO, bestwait_reg);

    /*
     * If the lowest priority thread is lower priority than the
     * value in the BESTWAIT register, we must raise the reschedule
     * interrupt on the lowest priority thread.
     */
    if (lowest_th_prio > best_prio) {
        /* do the resched int */
        HEX_DEBUG_LOG(
                "raising resched int %d, cur PC 0x%08x\n",
                int_number, ARCH_GET_THREAD_REG(env, HEX_REG_PC));
        SET_SYSTEM_FIELD(env, HEX_SREG_BESTWAIT, BESTWAIT_PRIO, 0x1ff);

#if HEX_DEBUG
        HEX_DEBUG_LOG("resched on tid %d\n", int_thread_env->threadId);
#endif
        assert(!hexagon_thread_is_busy(int_thread_env));
        hexagon_raise_interrupt(env, int_thread, int_number, 0);
    }
}

void HELPER(nmi)(CPUHexagonState *env, uint32_t thread_mask)
{
    CPUState *cs = NULL;
    CPU_FOREACH (cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread_env = &cpu->env;
        uint32_t thread_id_mask = 0x1 << thread_env->threadId;
        if ((thread_mask & thread_id_mask) != 0) {
            /* FIXME also wake these threads? cpu_resume/loop_exit_restore?*/
            cs->exception_index = HEX_EVENT_IMPRECISE;
            thread_env->cause_code = HEX_CAUSE_IMPRECISE_NMI;
            HEX_DEBUG_LOG("tid %d gets nmi\n", thread_env->threadId);
        }
    }
}

/*
 * Update the GCYCLE_XT register where X is the number of running threads
 */
void HELPER(inc_gcycle_xt)(CPUHexagonState *env)
{
    uint32_t num_threads = 0;
    CPUState *cs;
    CPU_FOREACH(cs) {
        if (!cpu_is_stopped(cs)) {
            num_threads++;
        }
    }
    if (1 <= num_threads && num_threads <= 6) {
        env->g_sreg[HEX_SREG_GCYCLE_1T + num_threads - 1]++;
    }
}

/*
 * Return the count of threads ready to run.
 */
uint32_t HELPER(get_ready_count)(CPUHexagonState *env)
{
    uint32_t ready_count = 0;
    CPUState *cs;
    CPU_FOREACH(cs) {
        HexagonCPU *cpu = HEXAGON_CPU(cs);
        CPUHexagonState *thread_env = &cpu->env;
        const bool running = (get_exe_mode(thread_env) == HEX_EXE_MODE_RUN);
        if (running) {
            ready_count += 1;
        }
    }
    return ready_count;
}

void HELPER(pending_interrupt)(CPUHexagonState *env)
{
    CPUState *cs = CPU(hexagon_env_get_cpu(env));
    const target_ulong pend_mask = hexagon_get_interrupts(env);
    int intnum;

    if (!pend_mask || cs->exception_index != HEX_EVENT_NONE) {
        return;
    }

    for (intnum = 0; intnum < reg_field_info[IPENDAD_IPEND].width;
         ++intnum) {
        unsigned mask = 0x1 << intnum;
        if ((pend_mask & mask) == 0) {
            continue;
        }

        if (hexagon_int_disabled(env, intnum)) {
            continue;
        }
        HexagonCPU *int_thread = hexagon_find_lowest_prio(env, intnum);
        if (!int_thread) {
            continue;
        }
        CPUHexagonState *thread_env = &int_thread->env;
        if (!hexagon_thread_is_interruptible(thread_env, intnum)) {
            continue;
        }
        uint32_t vid_num = hexagon_find_l2vic_pending();
        hexagon_raise_interrupt(thread_env, int_thread, intnum, vid_num);
    }
}

void HELPER(check_hvx)(CPUHexagonState *env)
{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    if (!GET_SSR_FIELD(SSR_XE, ssr)) {
        env->cause_code = HEX_CAUSE_NO_COPROC_ENABLE;
        helper_raise_exception(env, HEX_EVENT_PRECISE);
    }
}
#endif

#ifdef CONFIG_USER_ONLY
uint32_t HELPER(creg_read)(CPUHexagonState *env, uint32_t reg)
{
    return 0;
}

uint64_t HELPER(creg_read_pair)(CPUHexagonState *env, uint32_t reg)
{
    return 0;
}
#endif

void HELPER(commit_hmx)(CPUHexagonState *env)
{
    hmx_ext_commit_regs(env);
    hmx_ext_commit_mem(env, 0);
}
static inline uint8_t mem_load1(CPUHexagonState *env,
                                target_ulong vaddr)
{
    uint8_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u8(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 1, &retval);
#endif
    return retval;
}
static inline uint16_t mem_load2(CPUHexagonState *env,
                                 target_ulong vaddr)
{
    uint16_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u16(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 2, &retval);
#endif
    return retval;
}
static inline uint32_t mem_load4(CPUHexagonState *env,
                                 target_ulong vaddr)
{
    uint32_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u32(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 4, &retval);
#endif
    return retval;
}
static inline uint64_t mem_load8(CPUHexagonState *env,
                                 target_ulong vaddr)
{
    uint64_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u64(retval, vaddr);
#else
    hexagon_tools_memory_read(env, vaddr, 8, &retval);
#endif
    return retval;
}
static inline size1u_t mem_read1(CPUHexagonState *env, paddr_t paddr)
{
    size1u_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u8(retval, paddr);
#else
    hexagon_tools_memory_read(env, paddr, 1, &retval);
#endif
    return retval;
}
static inline size2u_t mem_read2(CPUHexagonState *env, paddr_t paddr)
{
    size2u_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u16(retval, paddr);
#else
    hexagon_tools_memory_read(env, paddr, 2, &retval);
#endif
    return retval;
}
static inline size4u_t mem_read4(CPUHexagonState *env, paddr_t paddr)
{
    size4u_t retval;
#ifdef CONFIG_USER_ONLY
    get_user_u32(retval, paddr);
#else
    hexagon_tools_memory_read(env, paddr, 4, &retval);
#endif
    return retval;
}
/* These macros can be referenced in the generated helper functions */
#define warn(...) /* Nothing */
#define fatal(...) g_assert_not_reached();
#define thread env
#define THREAD2STRUCT ((hmx_state_t*)env->processor_ptr->shared_extptr)

#define BOGUS_HELPER(tag) \
    printf("ERROR: bogus helper: " #tag "\n")

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#define DEF_QEMU(TAG, SHORTCODE, HELPER, GENFN, HELPFN) HELPFN
#include "qemu_def_generated.h"
#undef DEF_QEMU
#pragma GCC diagnostic pop
