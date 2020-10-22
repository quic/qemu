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
#include "qemu.h"
#include "exec/helper-proto.h"
#include "fpu/softfloat.h"
#include "cpu.h"
#include "internal.h"
#include "macros.h"
#include "arch.h"
#include "hex_arch_types.h"
#include "fma_emu.h"
#include "conv_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros.h"

#define SF_BIAS        127
#define SF_MANTBITS    23

#if COUNT_HEX_HELPERS
#include "opcodes.h"

typedef struct {
    int count;
    const char *tag;
} HelperCount;

HelperCount helper_counts[] = {
#define OPCODE(TAG)    { 0, #TAG }
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
    HelperCount *p;

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
static void QEMU_NORETURN do_raise_exception_err(CPUHexagonState *env,
                                                 uint32_t exception,
                                                 uintptr_t pc)
{
    CPUState *cs = CPU(hexagon_env_get_cpu(env));
    qemu_log_mask(CPU_LOG_INT, "%s: %d\n", __func__, exception);
    cs->exception_index = exception;
    cpu_loop_exit_restore(cs, pc);
}

void QEMU_NORETURN HELPER(raise_exception)(CPUHexagonState *env, uint32_t excp)
{
    do_raise_exception_err(env, excp, 0);
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
    HEX_DEBUG_LOG("Start packet: pc = 0x" TARGET_FMT_lx "\n",
                  env->gpr[HEX_REG_PC]);

    int i;
    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        env->reg_written[i] = 0;
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

void HELPER(commit_store)(CPUHexagonState *env, int slot_num)
{
    switch (env->mem_log_stores[slot_num].width) {
    case 1:
        put_user_u8(env->mem_log_stores[slot_num].data32,
                    env->mem_log_stores[slot_num].va);
        break;
    case 2:
        put_user_u16(env->mem_log_stores[slot_num].data32,
                     env->mem_log_stores[slot_num].va);
        break;
    case 4:
        put_user_u32(env->mem_log_stores[slot_num].data32,
                     env->mem_log_stores[slot_num].va);
        break;
    case 8:
        put_user_u64(env->mem_log_stores[slot_num].data64,
                     env->mem_log_stores[slot_num].va);
        break;
    default:
        g_assert_not_reached();
    }
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
                    put_user_u8(env->vstore[i].data.ub[j], va + j);
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
                SCATTER_OP_WRITE_TO_MEM(uint16_t);
            } else if (env->vtcm_log.op_size == 4) {
                /* Word Scatter += */
                SCATTER_OP_WRITE_TO_MEM(uint32_t);
            } else {
                g_assert_not_reached();
            }
        } else {
            for (int i = 0; i < env->vtcm_log.size; i++) {
                if (env->vtcm_log.mask.ub[i] != 0) {
                    put_user_u8(env->vtcm_log.data.ub[i], env->vtcm_log.va[i]);
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
        uint8_t width = env->mem_log_stores[slot].width;
        if (width == 1) {
            uint32_t data = env->mem_log_stores[slot].data32 & 0xff;
            HEX_DEBUG_LOG("\tmemb[0x" TARGET_FMT_lx "] = %d (0x%02x)\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 2) {
            uint32_t data = env->mem_log_stores[slot].data32 & 0xffff;
            HEX_DEBUG_LOG("\tmemh[0x" TARGET_FMT_lx "] = %d (0x%04x)\n",
                          env->mem_log_stores[slot].va, data, data);
        } else if (width == 4) {
            uint32_t data = env->mem_log_stores[slot].data32;
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

    HEX_DEBUG_LOG("Packet committed: pc = 0x" TARGET_FMT_lx "\n",
                  env->this_PC);
    HEX_DEBUG_LOG("slot_cancelled = %d\n", env->slot_cancelled);

    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        if (env->reg_written[i]) {
            if (!reg_printed) {
                HEX_DEBUG_LOG("Regs written\n");
                reg_printed = true;
            }
            HEX_DEBUG_LOG("\tr%d = " TARGET_FMT_ld " (0x" TARGET_FMT_lx ")\n",
                          i, env->new_value[i], env->new_value[i]);
        }
    }

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

    HEX_DEBUG_LOG("Next PC = 0x%x\n", env->next_PC);
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

static float32 build_float32(uint8_t sign, uint32_t exp, uint32_t mant)
{
    return make_float32(
        ((sign & 1) << 31) |
        ((exp & 0xff) << SF_MANTBITS) |
        (mant & ((1 << SF_MANTBITS) - 1)));
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
float32 HELPER(sfrecipa_val)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    /* int32_t PeV; Not needed to compute value */
    float32 RdV;
    int idx;
    int adjust;
    int mant;
    int exp;
    if (arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status)) {
        /* PeV = adjust; Not needed to compute value */
        idx = (RtV >> 16) & 0x7f;
        mant = (arch_recip_lookup(idx) << 15) | 1;
        exp = SF_BIAS - (float32_getexp(RtV) - SF_BIAS) - 1;
        RdV = build_float32(extract32(RtV, 31, 1), exp, mant);
    }
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(sfrecipa_pred)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    int32_t PeV = 0;
    float32 RdV;
    int adjust;
    if (arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status)) {
        PeV = adjust;
    }
    arch_fpop_end(env);
    return PeV;
}

float32 HELPER(sfinvsqrta_val)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    /* int32_t PeV; Not needed for val version */
    float32 RdV;
    int idx;
    int adjust;
    int mant;
    int exp;
    if (arch_sf_invsqrt_common(&RsV, &RdV, &adjust, &env->fp_status)) {
        /* PeV = adjust; Not needed for val version */
        idx = (RsV >> 17) & 0x7f;
        mant = (arch_invsqrt_lookup(idx) << 15);
        exp = SF_BIAS - ((float32_getexp(RsV) - SF_BIAS) >> 1) - 1;
        RdV = build_float32(extract32(RsV, 31, 1), exp, mant);
    }
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(sfinvsqrta_pred)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    int32_t PeV = 0;
    float32 RdV;
    int adjust;
    if (arch_sf_invsqrt_common(&RsV, &RdV, &adjust, &env->fp_status)) {
        PeV = adjust;
    }
    arch_fpop_end(env);
    return PeV;
}

int64_t HELPER(vacsh_val)(CPUHexagonState *env,
                           int64_t RxxV, int64_t RssV, int64_t RttV)
{
    int32_t PeV = 0;
    int i;
    int xv;
    int sv;
    int tv;
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
    int i;
    int xv;
    int sv;
    int tv;
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

/* Floating point */
float64 HELPER(conv_sf2df)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    float64 out_f64 = float32_to_float64(RsV, &env->fp_status);
    if (float64_is_any_nan(out_f64)) {
        out_f64 = make_float64(0xFFFFFFFFFFFFFFFFULL);
    }
    arch_fpop_end(env);
    return out_f64;
}

float32 HELPER(conv_df2sf)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    float32 out_f32 = float64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env);
    return out_f32;
}

float32 HELPER(conv_uw2sf)(CPUHexagonState *env, int32_t RsV)
{
    arch_fpop_start(env);
    float32 RdV = uint32_to_float32(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_uw2df)(CPUHexagonState *env, int32_t RsV)
{
    arch_fpop_start(env);
    float64 RddV = uint32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(conv_w2sf)(CPUHexagonState *env, int32_t RsV)
{
    arch_fpop_start(env);
    float32 RdV = int32_to_float32(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_w2df)(CPUHexagonState *env, int32_t RsV)
{
    arch_fpop_start(env);
    float64 RddV = int32_to_float64(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(conv_ud2sf)(CPUHexagonState *env, int64_t RssV)
{
    arch_fpop_start(env);
    float32 RdV = uint64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_ud2df)(CPUHexagonState *env, int64_t RssV)
{
    arch_fpop_start(env);
    float64 RddV = uint64_to_float64(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(conv_d2sf)(CPUHexagonState *env, int64_t RssV)
{
    arch_fpop_start(env);
    float32 RdV = int64_to_float32(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(conv_d2df)(CPUHexagonState *env, int64_t RssV)
{
    arch_fpop_start(env);
    float64 RddV = int64_to_float64(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int32_t HELPER(conv_sf2uw)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    int32_t RdV = conv_sf_to_4u(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_sf2w)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    int32_t RdV = conv_sf_to_4s(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int64_t HELPER(conv_sf2ud)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    int64_t RddV = conv_sf_to_8u(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_sf2d)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    int64_t RddV = conv_sf_to_8s(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int32_t HELPER(conv_df2uw)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    int32_t RdV = conv_df_to_4u(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_df2w)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    int32_t RdV = conv_df_to_4s(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int64_t HELPER(conv_df2ud)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    int64_t RddV = conv_df_to_8u(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_df2d)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    int64_t RddV = conv_df_to_8s(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int32_t HELPER(conv_sf2uw_chop)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int32_t RdV = conv_sf_to_4u(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_sf2w_chop)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int32_t RdV = conv_sf_to_4s(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int64_t HELPER(conv_sf2ud_chop)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int64_t RddV = conv_sf_to_8u(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_sf2d_chop)(CPUHexagonState *env, float32 RsV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int64_t RddV = conv_sf_to_8s(RsV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int32_t HELPER(conv_df2uw_chop)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int32_t RdV = conv_df_to_4u(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(conv_df2w_chop)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int32_t RdV = conv_df_to_4s(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int64_t HELPER(conv_df2ud_chop)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int64_t RddV = conv_df_to_8u(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int64_t HELPER(conv_df2d_chop)(CPUHexagonState *env, float64 RssV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_to_zero, &env->fp_status);
    int64_t RddV = conv_df_to_8s(RssV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float32 HELPER(sfadd)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    float32 RdV = float32_add(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sfsub)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    float32 RdV = float32_sub(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(sfcmpeq)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    int32_t PdV = f8BITSOF(float32_eq(RsV, RtV, &env->fp_status));
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(sfcmpgt)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    int cmp = float32_compare(RsV, RtV, &env->fp_status);
    int32_t PdV = f8BITSOF(cmp == float_relation_greater);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(sfcmpge)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    int cmp = float32_compare(RsV, RtV, &env->fp_status);
    int32_t PdV = f8BITSOF(cmp == float_relation_greater ||
                           cmp == float_relation_equal);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(sfcmpuo)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    int32_t PdV = f8BITSOF(float32_is_any_nan(RsV) ||
                           float32_is_any_nan(RtV));
    arch_fpop_end(env);
    return PdV;
}

float32 HELPER(sfmax)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    float32 RdV = float32_max(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sfmin)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    float32 RdV = float32_min(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

int32_t HELPER(sfclass)(CPUHexagonState *env, float32 RsV, int32_t uiV)
{
    arch_fpop_start(env);
    int32_t PdV = 0;
    if (fGETBIT(0, uiV) && float32_is_zero(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(1, uiV) && float32_is_normal(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(2, uiV) && float32_is_denormal(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(3, uiV) && float32_is_infinity(RsV)) {
        PdV = 0xff;
    }
    if (fGETBIT(4, uiV) && float32_is_any_nan(RsV)) {
        PdV = 0xff;
    }
    set_float_exception_flags(0, &env->fp_status);
    arch_fpop_end(env);
    return PdV;
}

float32 HELPER(sffixupn)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV = 0;
    arch_fpop_start(env);
    int adjust;
    arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status);
    RdV = RsV;
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sffixupd)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    float32 RdV = 0;
    arch_fpop_start(env);
    int adjust;
    arch_sf_recip_common(&RsV, &RtV, &RdV, &adjust, &env->fp_status);
    RdV = RtV;
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sffixupr)(CPUHexagonState *env, float32 RsV)
{
    float32 RdV = 0;
    arch_fpop_start(env);
    int adjust;
    arch_sf_invsqrt_common(&RsV, &RdV, &adjust, &env->fp_status);
    RdV = RsV;
    arch_fpop_end(env);
    return RdV;
}

float64 HELPER(dfadd)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    float64 RddV = float64_add(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfsub)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    float64 RddV = float64_sub(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfmax)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    float64 RddV = float64_max(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfmin)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    float64 RddV = float64_min(RssV, RttV, &env->fp_status);
    arch_fpop_end(env);
    return RddV;
}

int32_t HELPER(dfcmpeq)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    int32_t PdV = f8BITSOF(float64_eq(RssV, RttV, &env->fp_status));
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfcmpgt)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    int cmp = float64_compare(RssV, RttV, &env->fp_status);
    int32_t PdV = f8BITSOF(cmp == float_relation_greater);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfcmpge)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    int cmp = float64_compare(RssV, RttV, &env->fp_status);
    int32_t PdV = f8BITSOF(cmp == float_relation_greater ||
                           cmp == float_relation_equal);
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfcmpuo)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    int32_t PdV = f8BITSOF(float64_is_any_nan(RssV) ||
                           float64_is_any_nan(RttV));
    arch_fpop_end(env);
    return PdV;
}

int32_t HELPER(dfclass)(CPUHexagonState *env, float64 RssV, int32_t uiV)
{
    arch_fpop_start(env);
    int32_t PdV = 0;
    if (fGETBIT(0, uiV) && float64_is_zero(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(1, uiV) && float64_is_normal(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(2, uiV) && float64_is_denormal(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(3, uiV) && float64_is_infinity(RssV)) {
        PdV = 0xff;
    }
    if (fGETBIT(4, uiV) && float64_is_any_nan(RssV)) {
        PdV = 0xff;
    }
    set_float_exception_flags(0, &env->fp_status);
    arch_fpop_end(env);
    return PdV;
}

float32 HELPER(sfmpy)(CPUHexagonState *env, float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    float32 RdV = internal_mpyf(RsV, RtV, &env->fp_status);
    arch_fpop_end(env);
    return RdV;
}

float32 HELPER(sffma)(CPUHexagonState *env, float32 RxV,
                      float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    RxV = internal_fmafx(RsV, RtV, RxV, 0, &env->fp_status);
    arch_fpop_end(env);
    return RxV;
}

static bool isfinite(float32 x)
{
    return !float32_is_any_nan(x) && !float32_is_infinity(x);
}

static bool is_zero_prod(float32 a, float32 b)
{
    return ((float32_is_zero(a) && isfinite(b)) ||
            (float32_is_zero(b) && isfinite(a)));
}

static float32 check_nan(float32 dst, float32 x, float_status *fp_status)
{
    float32 ret = dst;
    if (float32_is_any_nan(x)) {
        if (extract32(x, 22, 1) == 0) {
            float_raise(float_flag_invalid, fp_status);
        }
        ret = make_float32(0xffffffff);    /* nan */
    }
    return ret;
}

float32 HELPER(sffma_sc)(CPUHexagonState *env, float32 RxV,
                         float32 RsV, float32 RtV, float32 PuV)
{
    arch_fpop_start(env);
    size4s_t tmp;
    RxV = check_nan(RxV, RxV, &env->fp_status);
    RxV = check_nan(RxV, RsV, &env->fp_status);
    RxV = check_nan(RxV, RtV, &env->fp_status);
    tmp = internal_fmafx(RsV, RtV, RxV, fSXTN(8, 64, PuV), &env->fp_status);
    if (!(float32_is_zero(RxV) && is_zero_prod(RsV, RtV))) {
        RxV = tmp;
    }
    arch_fpop_end(env);
    return RxV;
}

float32 HELPER(sffms)(CPUHexagonState *env, float32 RxV,
                      float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    float32 neg_RsV = float32_sub(float32_zero, RsV, &env->fp_status);
    RxV = internal_fmafx(neg_RsV, RtV, RxV, 0, &env->fp_status);
    arch_fpop_end(env);
    return RxV;
}

static inline bool is_inf_prod(int32_t a, int32_t b)
{
    return (float32_is_infinity(a) && float32_is_infinity(b)) ||
           (float32_is_infinity(a) && isfinite(b) && !float32_is_zero(b)) ||
           (float32_is_infinity(b) && isfinite(a) && !float32_is_zero(a));
}

float32 HELPER(sffma_lib)(CPUHexagonState *env, float32 RxV,
                          float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_nearest_even, &env->fp_status);
    int infinp;
    int infminusinf;
    float32 tmp;
    infminusinf = float32_is_infinity(RxV) &&
                  is_inf_prod(RsV, RtV) &&
                  (fGETBIT(31, RsV ^ RxV ^ RtV) != 0);
    infinp = float32_is_infinity(RxV) ||
             float32_is_infinity(RtV) ||
             float32_is_infinity(RsV);
    RxV = check_nan(RxV, RxV, &env->fp_status);
    RxV = check_nan(RxV, RsV, &env->fp_status);
    RxV = check_nan(RxV, RtV, &env->fp_status);
    tmp = internal_fmafx(RsV, RtV, RxV, 0, &env->fp_status);
    if (!(float32_is_zero(RxV) && is_zero_prod(RsV, RtV))) {
        RxV = tmp;
    }
    set_float_exception_flags(0, &env->fp_status);
    if (float32_is_infinity(RxV) && !infinp) {
        RxV = RxV - 1;
    }
    if (infminusinf) {
        RxV = 0;
    }
    arch_fpop_end(env);
    return RxV;
}

float32 HELPER(sffms_lib)(CPUHexagonState *env, float32 RxV,
                          float32 RsV, float32 RtV)
{
    arch_fpop_start(env);
    set_float_rounding_mode(float_round_nearest_even, &env->fp_status);
    int infinp;
    int infminusinf;
    float32 tmp;
    infminusinf = float32_is_infinity(RxV) &&
                  is_inf_prod(RsV, RtV) &&
                  (fGETBIT(31, RsV ^ RxV ^ RtV) == 0);
    infinp = float32_is_infinity(RxV) ||
             float32_is_infinity(RtV) ||
             float32_is_infinity(RsV);
    RxV = check_nan(RxV, RxV, &env->fp_status);
    RxV = check_nan(RxV, RsV, &env->fp_status);
    RxV = check_nan(RxV, RtV, &env->fp_status);
    float32 minus_RsV = float32_sub(float32_zero, RsV, &env->fp_status);
    tmp = internal_fmafx(minus_RsV, RtV, RxV, 0, &env->fp_status);
    if (!(float32_is_zero(RxV) && is_zero_prod(RsV, RtV))) {
        RxV = tmp;
    }
    set_float_exception_flags(0, &env->fp_status);
    if (float32_is_infinity(RxV) && !infinp) {
        RxV = RxV - 1;
    }
    if (infminusinf) {
        RxV = 0;
    }
    arch_fpop_end(env);
    return RxV;
}

float64 HELPER(dfmpyfix)(CPUHexagonState *env, float64 RssV, float64 RttV)
{
    int64_t RddV;
    if (float64_is_denormal(RssV) &&
        (float64_getexp(RttV) >= 512) &&
        float64_is_normal(RttV)) {
        RddV = float64_mul(RssV, make_float64(0x4330000000000000),
                           &env->fp_status);
    } else if (float64_is_denormal(RttV) &&
               (float64_getexp(RssV) >= 512) &&
               float64_is_normal(RssV)) {
        RddV = float64_mul(RssV, make_float64(0x3cb0000000000000),
                           &env->fp_status);
    } else {
        RddV = RssV;
    }
    arch_fpop_end(env);
    return RddV;
}

float64 HELPER(dfmpyhh)(CPUHexagonState *env, float64 RxxV,
                        float64 RssV, float64 RttV)
{
    arch_fpop_start(env);
    RxxV = internal_mpyhh(RssV, RttV, RxxV, &env->fp_status);
    arch_fpop_end(env);
    return RxxV;
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
        env->future_VRegs[num] = *(MMVector *)var;
        if (vnew == EXT_TMP) {
            env->tmp_VRegs[num] = env->future_VRegs[num];
        }
    }
}

static inline void log_mmvector_write(CPUHexagonState *env, int num,
                                      MMVector var, int vnew, uint32_t slot)
{
    log_vreg_write(env, num, &var, vnew, slot);
}

static void cancel_slot(CPUHexagonState *env, uint32_t slot)
{
    HEX_DEBUG_LOG("Slot %d cancelled\n", slot);
    env->slot_cancelled |= (1 << slot);
}

/* These macros can be referenced in the generated helper functions */
#define warn(...) /* Nothing */
#define fatal(...) g_assert_not_reached();

#define BOGUS_HELPER(tag) \
    printf("ERROR: bogus helper: " #tag "\n")

#include "helper_funcs_generated.h"

