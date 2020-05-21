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
    qemu_log_mask(CPU_LOG_INT, "%s: %d\n", __func__, exception);
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

static inline void log_reg_write(CPUHexagonState *env, int rnum,
                                 target_ulong val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_reg_write[%d] = " TARGET_FMT_ld " (0x" TARGET_FMT_lx ")",
                  rnum, val, val);
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

static inline void log_sreg_write(CPUHexagonState *env, int rnum,
                                 target_ulong val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_sreg_write[%s/%d] = " TARGET_FMT_ld " (0x" TARGET_FMT_lx ")",
                  hexagon_sregnames[rnum], rnum, val, val);
    if (env->slot_cancelled & (1 << slot)) {
        HEX_DEBUG_LOG(" CANCELLED");
    }

    target_ulong crnt_val = (rnum < HEX_SREG_GLB_START) ?
        env->t_sreg[rnum] : env->g_sreg[rnum];
    if (val == crnt_val) {
        HEX_DEBUG_LOG(" NO CHANGE");
    }
    HEX_DEBUG_LOG("\n");
    if (!(env->slot_cancelled & (1 << slot))) {
        HEX_DEBUG_LOG("\tsreg register set to 0x%x\n", val);
        if (rnum < HEX_SREG_GLB_START) {
            env->t_sreg_new_value[rnum] = val;
        } else {
            env->g_sreg_new_value[rnum] = val;
        }
    }
    else
      HEX_DEBUG_LOG("\tsreg register not set\n");
}

static __attribute__((unused))
inline void log_sreg_write_pair(CPUHexagonState *env, int rnum,
                                      int64_t val, uint32_t slot)
{
    HEX_DEBUG_LOG("log_sreg_write_pair[%d:%d] = %ld\n", rnum + 1, rnum, val);
    log_sreg_write(env, rnum, val & 0xFFFFFFFF, slot);
    log_sreg_write(env, rnum + 1, (val >> 32) & 0xFFFFFFFF, slot);
}

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
    HEX_DEBUG_LOG("Start packet: pc = 0x" TARGET_FMT_lx " r0=%d, tid=%d\n",
                  env->gpr[HEX_REG_PC], env->gpr[HEX_REG_R00], env->threadId);

    int i;
    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        env->reg_written[i] = 0;
    }
    for (i = 0; i < NUM_SREGS; i++) {
        if (i < HEX_SREG_GLB_START)
            env->t_sreg_written[i] = 0;
        else
            env->g_sreg_written[i] = 0;
    }
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

void hexagon_load_byte(CPUHexagonState *env, uint8_t *load_byte, target_ulong src_vaddr)

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

void hexagon_store_byte(CPUHexagonState *env, uint8_t store_byte, target_ulong dst_vaddr)

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
                    hexagon_store_byte(env, env->vtcm_log.data.ub[i], env->vtcm_log.va[i]);
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
    bool sreg_printed = false;
    bool pred_printed = false;
    int i;

    HEX_DEBUG_LOG("Packet committed: tid = %d, pc = 0x" TARGET_FMT_lx "\n",
                  env->threadId, env->this_PC);
    if (env->slot_cancelled)
      HEX_DEBUG_LOG("slot_cancelled = %d\n", env->slot_cancelled);

    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        if (env->reg_written[i]) {
            if (!reg_printed) {
                printf("Regs written\n");
                reg_printed = true;
            }
            printf("\tr%d = " TARGET_FMT_ld " (0x" TARGET_FMT_lx " )\n",
                          i, env->new_value[i], env->new_value[i]);
        }
    }

    for (i = 0; i < NUM_SREGS; i++) {
        if (i < HEX_SREG_GLB_START) {
            if (env->t_sreg_written[i]) {
                if (!sreg_printed) {
                    printf("SRegs written\n");
                    sreg_printed = true;
                }
                printf("\tset s%d (%s) = " TARGET_FMT_ld " (0x" TARGET_FMT_lx " )\n",
                    i, hexagon_sregnames[i], env->t_sreg_new_value[i],
                    env->t_sreg_new_value[i]);
            }
        } else {
            if (env->g_sreg_written[i]) {
                if (!sreg_printed) {
                    printf("SRegs written\n");
                    sreg_printed = true;
            }
            printf("\tset s%d (%s) = " TARGET_FMT_ld " (0x" TARGET_FMT_lx " )\n",
                i, hexagon_sregnames[i], env->g_sreg_new_value[i],
                env->g_sreg_new_value[i]);
            }
        }
    }

    for (i = 0; i < NUM_PREGS; i++) {
        if (env->pred_written & (1 << i)) {
            if (!pred_printed) {
                printf("Predicates written\n");
                pred_printed = true;
            }
            printf("\tp%d = 0x" TARGET_FMT_lx "\n",
                          i, env->new_pred_value[i]);
        }
    }

    if (has_st0 || has_st1) {
        printf("Stores\n");
        if (has_st0) {
            print_store(env, 0);
        }
        if (has_st1) {
            print_store(env, 1);
        }
    }

    HEX_DEBUG_LOG("Next PC = 0x%x\n", env->next_PC);
    HEX_DEBUG_LOG("Exec counters: pkt = " TARGET_FMT_lx
                  ", insn = " TARGET_FMT_lx
                  ", hvx = " TARGET_FMT_lx "\n",
                  env->gpr[HEX_REG_QEMU_PKT_CNT],
                  env->gpr[HEX_REG_QEMU_INSN_CNT],
                  env->gpr[HEX_REG_QEMU_HVX_CNT]);

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
    return RdV;
}

int32_t HELPER(sfrecipa_pred)(CPUHexagonState *env, int32_t RsV, int32_t RtV)
{
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
    return PeV;
}

int32_t HELPER(sfinvsqrta_val)(CPUHexagonState *env, int32_t RsV)
{
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
    return RdV;
}

int32_t HELPER(sfinvsqrta_pred)(CPUHexagonState *env, int32_t RsV)
{
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
void HELPER(modify_syscfg)(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    target_ulong old_mmu_enable = GET_FIELD(SYSCFG_MMUEN, old);
    target_ulong new_mmu_enable = GET_FIELD(SYSCFG_MMUEN, new);
    if (new_mmu_enable && !old_mmu_enable) {
        hex_mmu_on(env);
    } else if (!new_mmu_enable && old_mmu_enable) {
        hex_mmu_off(env);
    }
}

void HELPER(modify_ssr)(CPUHexagonState *env, uint32_t new, uint32_t old)
{
    /* FIXME - Change these to GET_FIELD */
    target_ulong old_EX = old & SSR_EX;
    target_ulong old_UM = old & SSR_UM;
    target_ulong old_GM = old & SSR_GM;
    target_ulong new_EX = new & SSR_EX;
    target_ulong new_UM = new & SSR_UM;
    target_ulong new_GM = new & SSR_GM;

    if ((old_EX != new_EX) ||
        (old_UM != new_UM) ||
        (old_GM != new_GM)) {
        hex_mmu_mode_change(env);
    }

    /* FIXME - Change these to GET_FIELD */
    uint8_t old_asid = (old & SSR_ASID) >> 8;
    uint8_t new_asid = (new & SSR_ASID) >> 8;
    if (new_asid != old_asid) {
        CPUState *cs = CPU(hexagon_env_get_cpu(env));
        tlb_flush(cs);
    }
}

void HELPER(checkforpriv)(CPUHexagonState *env)
{
    if (!sys_in_monitor_mode(env)) {
        helper_raise_exception(env, HEX_CAUSE_PRIV_USER_NO_SINSN);
    }
}

static inline void probe_store(CPUHexagonState *env, int slot)
{
    if (!(env->slot_cancelled & (1 << slot))) {
        size1u_t width = env->mem_log_stores[slot].width;
        target_ulong va = env->mem_log_stores[slot].va;
        int mmu_idx = cpu_mmu_index(env, false);
        probe_write(env, va, width, mmu_idx, env->gpr[HEX_REG_PC]);
    }
}

static inline void probe_hvx_stores(CPUHexagonState *env)
{
    int mmu_idx = cpu_mmu_index(env, false);
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
                              int has_dczeroa, int has_hvx_stores)
{
    if (has_st0 && !has_dczeroa) {
        probe_store(env, 0);
    }
    if (has_st1 && !has_dczeroa) {
        probe_store(env, 1);
    }
    if (has_dczeroa) {
        /* Probe 32 bytes starting at (dczero_addr & ~0x1f) */
        target_ulong va = env->dczero_addr & ~0x1f;
        int mmu_idx = cpu_mmu_index(env, false);
        probe_write(env, va +  0, 8, mmu_idx, env->gpr[HEX_REG_PC]);
        probe_write(env, va +  8, 8, mmu_idx, env->gpr[HEX_REG_PC]);
        probe_write(env, va + 16, 8, mmu_idx, env->gpr[HEX_REG_PC]);
        probe_write(env, va + 24, 8, mmu_idx, env->gpr[HEX_REG_PC]);
    }
    if (has_hvx_stores) {
        probe_hvx_stores(env);
    }
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
void HELPER(fstart)(CPUHexagonState *env, uint32_t mask)

{
    hexagon_create_cpu(env, mask);
}

void HELPER(clear_run_mode)(CPUHexagonState *env, uint32_t mask)

{
    hexagon_destroy_cpu(env, mask);
}
void HELPER(pause)(CPUHexagonState *env, uint32_t val)

{
    CPUState *cs = CPU(hexagon_env_get_cpu(env));

    /* Just let another CPU run.  */
    cs->exception_index = EXCP_INTERRUPT;
    cpu_loop_exit(cs);
}

uint32_t HELPER(sreg_read)(CPUHexagonState *env, uint32_t reg)

{
    return env->g_sreg[reg];
}

uint64_t HELPER(sreg_read_pair)(CPUHexagonState *env, uint32_t reg)

{
    return (uint64_t)(env->g_sreg[reg]) | ((uint64_t)(env->g_sreg[reg+1]) << 32);
}

void HELPER(sreg_write)(CPUHexagonState *env, uint32_t reg, uint32_t val)

{
    if (reg < HEX_SREG_GLB_START) {
        env->t_sreg[reg] = val;
    } else {
        env->g_sreg[reg] = val;
    }
}

void HELPER(sreg_write_pair)(CPUHexagonState *env, uint32_t reg, uint64_t val)

{
    if (reg < HEX_SREG_GLB_START) {
        env->t_sreg[reg] = val & 0xFFFFFFFF;
        env->t_sreg[reg+1] = val >> 32;
    } else {
        env->g_sreg[reg] = val & 0xFFFFFFFF;
        env->g_sreg[reg+1] = val >> 32;
    }
}
#endif

/* These macros can be referenced in the generated helper functions */
#define warn(...) /* Nothing */
#define fatal(...) g_assert_not_reached();
#define thread env

#define BOGUS_HELPER(tag) \
    printf("ERROR: bogus helper: " #tag "\n")

#define DEF_QEMU(TAG, SHORTCODE, HELPER, GENFN, HELPFN) HELPFN
#include "qemu_def_generated.h"
#undef DEF_QEMU
