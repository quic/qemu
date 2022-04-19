/*
 *  Copyright(c) 2019-2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "cpu.h"
#include "internal.h"
#include "tcg/tcg-op.h"
#include "tcg/tcg-op-gvec.h"
#include "insn.h"
#include "opcodes.h"
#include "translate.h"
#define QEMU_GENERATE       /* Used internally by macros.h */
#include "macros.h"
#include "sys_macros.h"
#include "mmvec/macros.h"
#include "mmvec/macros_auto.h"
#undef QEMU_GENERATE
#include "attribs.h"
#include "gen_tcg.h"
#include "gen_tcg_hvx.h"
#include "reg_fields.h"


static inline TCGv gen_read_reg(TCGv result, int num)
{
    tcg_gen_mov_tl(result, hex_gpr[num]);
    return result;
}

static inline TCGv gen_read_preg(TCGv pred, uint8_t num)
{
    tcg_gen_mov_tl(pred, hex_pred[num]);
    return pred;
}

static inline void gen_log_predicated_reg_write(int rnum, TCGv val, int slot, TCGv zero)
{
    TCGv slot_mask = tcg_temp_new();

    tcg_gen_andi_tl(slot_mask, hex_slot_cancelled, 1 << slot);
    tcg_gen_movcond_tl(TCG_COND_EQ, hex_new_value[rnum], slot_mask, zero,
                           val, hex_new_value[rnum]);
    if (HEX_DEBUG) {
        /*
         * Do this so HELPER(debug_commit_end) will know
         *
         * Note that slot_mask indicates the value is not written
         * (i.e., slot was cancelled), so we create a true/false value before
         * or'ing with hex_reg_written[rnum].
         */
        tcg_gen_setcond_tl(TCG_COND_EQ, slot_mask, slot_mask, zero);
        tcg_gen_or_tl(hex_reg_written[rnum], hex_reg_written[rnum], slot_mask);
    }

    tcg_temp_free(slot_mask);
}

static inline void gen_log_reg_write(int rnum, TCGv val)
{
    tcg_gen_mov_tl(hex_new_value[rnum], val);
    if (HEX_DEBUG) {
        /* Do this so HELPER(debug_commit_end) will know */
        tcg_gen_movi_tl(hex_reg_written[rnum], 1);
    }
}

#ifndef CONFIG_USER_ONLY
static inline void gen_log_greg_write(int rnum, TCGv val)
{
    g_assert(rnum <= HEX_GREG_G3);
    tcg_gen_mov_tl(hex_greg_new_value[rnum], val);
    if (HEX_DEBUG) {
        /* Do this so HELPER(debug_commit_end) will know */
        tcg_gen_movi_tl(hex_greg_written[rnum], 1);
    }
}
#endif

static void gen_log_predicated_reg_write_pair(int rnum, TCGv_i64 val, int slot, TCGv zero)
{
    TCGv val32 = tcg_temp_new();
    TCGv slot_mask = tcg_temp_new();

    tcg_gen_andi_tl(slot_mask, hex_slot_cancelled, 1 << slot);
    /* Low word */
    tcg_gen_extrl_i64_i32(val32, val);
    tcg_gen_movcond_tl(TCG_COND_EQ, hex_new_value[rnum],
                       slot_mask, zero,
                       val32, hex_new_value[rnum]);
    /* High word */
    tcg_gen_extrh_i64_i32(val32, val);
    tcg_gen_movcond_tl(TCG_COND_EQ, hex_new_value[rnum + 1],
                       slot_mask, zero,
                       val32, hex_new_value[rnum + 1]);
    if (HEX_DEBUG) {
        /*
         * Do this so HELPER(debug_commit_end) will know
         *
         * Note that slot_mask indicates the value is not written
         * (i.e., slot was cancelled), so we create a true/false value before
         * or'ing with hex_reg_written[rnum].
         */
        tcg_gen_setcond_tl(TCG_COND_EQ, slot_mask, slot_mask, zero);
        tcg_gen_or_tl(hex_reg_written[rnum], hex_reg_written[rnum], slot_mask);
        tcg_gen_or_tl(hex_reg_written[rnum + 1], hex_reg_written[rnum + 1],
                      slot_mask);
    }

    tcg_temp_free(val32);
    tcg_temp_free(slot_mask);
}

static void gen_log_reg_write_pair(int rnum, TCGv_i64 val)
{
    /* Low word */
    tcg_gen_extrl_i64_i32(hex_new_value[rnum], val);
    if (HEX_DEBUG) {
        /* Do this so HELPER(debug_commit_end) will know */
        tcg_gen_movi_tl(hex_reg_written[rnum], 1);
    }

    /* High word */
    tcg_gen_extrh_i64_i32(hex_new_value[rnum + 1], val);
    if (HEX_DEBUG) {
        /* Do this so HELPER(debug_commit_end) will know */
        tcg_gen_movi_tl(hex_reg_written[rnum + 1], 1);
    }
}

#ifndef CONFIG_USER_ONLY
static void gen_log_greg_write_pair(int rnum, TCGv_i64 val)
{
    TCGv val32 = tcg_temp_new();

    /* Low word */
    tcg_gen_extrl_i64_i32(val32, val);
    tcg_gen_mov_tl(hex_greg_new_value[rnum], val32);
    /* High word */
    tcg_gen_extrh_i64_i32(val32, val);
    tcg_gen_mov_tl(hex_greg_new_value[rnum + 1], val32);

    tcg_temp_free(val32);
}

static void gen_log_sreg_write(int rnum, TCGv val)
{
    if (rnum < HEX_SREG_GLB_START) {
        tcg_gen_mov_tl(hex_t_sreg_new_value[rnum], val);
    } else {
        gen_helper_sreg_write(cpu_env, tcg_constant_i32(rnum), val);
    }
}

static void gen_log_sreg_write_pair(int rnum, TCGv_i64 val)
{
    if (rnum < HEX_SREG_GLB_START) {
        TCGv val32 = tcg_temp_new();

        /* Low word */
        tcg_gen_extrl_i64_i32(val32, val);
        tcg_gen_mov_tl(hex_t_sreg_new_value[rnum], val32);
        /* High word */
        tcg_gen_extrh_i64_i32(val32, val);
        tcg_gen_mov_tl(hex_t_sreg_new_value[rnum + 1], val32);

        tcg_temp_free(val32);
    } else {
        gen_helper_sreg_write_pair(cpu_env, tcg_constant_i32(rnum), val);
    }
}
#endif


static inline void gen_log_pred_write(DisasContext *ctx, int pnum, TCGv val)
{
    TCGv base_val = tcg_temp_new();

    tcg_gen_andi_tl(base_val, val, 0xff);

    /*
     * Section 6.1.3 of the Hexagon V67 Programmer's Reference Manual
     *
     * Multiple writes to the same preg are and'ed together
     * If this is the first predicate write in the packet, do a
     * straight assignment.  Otherwise, do an and.
     */
    if (!test_bit(pnum, ctx->pregs_written)) {
        tcg_gen_mov_tl(hex_new_pred_value[pnum], base_val);
    } else {
        tcg_gen_and_tl(hex_new_pred_value[pnum],
                       hex_new_pred_value[pnum], base_val);
    }
    tcg_gen_ori_tl(hex_pred_written, hex_pred_written, 1 << pnum);

    tcg_temp_free(base_val);
}

#ifndef CONFIG_USER_ONLY
static void gen_read_sreg(TCGv dst, int reg_num)
{
    if (reg_num < HEX_SREG_GLB_START) {
        if (reg_num == HEX_SREG_BADVA) {
            gen_helper_sreg_read(dst, cpu_env, tcg_constant_i32(reg_num));
        } else {
            tcg_gen_mov_tl(dst, hex_t_sreg[reg_num]);
        }
    } else {
        gen_helper_sreg_read(dst, cpu_env, tcg_constant_i32(reg_num));
    }
}

static void gen_read_sreg_pair(TCGv_i64 dst, int reg_num)
{
    if (reg_num < HEX_SREG_GLB_START) {
        if (reg_num + 1 == HEX_SREG_BADVA) {
            TCGv badva = tcg_temp_new();
            gen_helper_sreg_read(badva, cpu_env,
                                 tcg_constant_tl(HEX_SREG_BADVA));
            tcg_gen_concat_i32_i64(dst, hex_t_sreg[reg_num], badva);
            tcg_temp_free(badva);
        } else {
            tcg_gen_concat_i32_i64(dst, hex_t_sreg[reg_num],
                                        hex_t_sreg[reg_num + 1]);
        }
    } else {
        gen_helper_sreg_read_pair(dst, cpu_env, tcg_constant_tl(reg_num));
    }
}

static void gen_read_greg(TCGv dst, int reg_num)
{
    gen_helper_greg_read(dst, cpu_env, tcg_constant_tl(reg_num));
}

static void gen_read_greg_pair(TCGv_i64 dst, int reg_num)
{
    gen_helper_greg_read_pair(dst, cpu_env, tcg_constant_tl(reg_num));
}
#endif

/*
 * The upcyclehi/upcyclelo (user pcycle) registers mirror
 * the pcyclehi/pcyclelo global sregs.
 * They are not modelled in user-only mode.
 * In system mode, they can only be read when SSR[CE] is set.
 */
static inline void gen_read_upcycle_reg(TCGv dest, int regnum, TCGv zero)
{
#ifdef CONFIG_USER_ONLY
    tcg_gen_movi_tl(dest, 0);
#else
    TCGv ssr_ce = tcg_temp_new();
    TCGv counter = tcg_temp_new();

    GET_SSR_FIELD(ssr_ce, SSR_CE);

    if (regnum == HEX_REG_UPCYCLEHI) {
        gen_read_sreg(counter, HEX_SREG_PCYCLEHI);
    } else if (regnum == HEX_REG_UPCYCLELO) {
        gen_read_sreg(counter, HEX_SREG_PCYCLELO);
    } else {
        g_assert_not_reached();
    }
    /* dest = (ssr_ce != 0 ? counter : 0) */
    tcg_gen_movcond_tl(TCG_COND_NE, dest, ssr_ce, zero, counter, zero);

    tcg_temp_free(ssr_ce);
    tcg_temp_free(counter);
#endif
}

static inline void gen_read_p3_0(TCGv control_reg)
{
    tcg_gen_movi_tl(control_reg, 0);
    for (int i = 0; i < NUM_PREGS; i++) {
        tcg_gen_deposit_tl(control_reg, control_reg, hex_pred[i], i * 8, 8);
    }
}

/*
 * Certain control registers require special handling on read
 *     HEX_REG_P3_0          aliased to the predicate registers
 *                           -> concat the 4 predicate registers together
 *     HEX_REG_PC            actual value stored in DisasContext
 *                           -> assign from ctx->base.pc_next
 *     HEX_REG_QEMU_*_CNT    changes in current TB in DisasContext
 *                           -> add current TB changes to existing reg value
 */
static inline void gen_read_ctrl_reg(DisasContext *ctx, const int reg_num,
                                     TCGv dest)
{
    if (reg_num == HEX_REG_P3_0) {
        gen_read_p3_0(dest);
    } else if (reg_num == HEX_REG_UPCYCLEHI) {
        gen_read_upcycle_reg(dest, HEX_REG_UPCYCLEHI, ctx->zero);
    } else if (reg_num == HEX_REG_UPCYCLELO) {
        gen_read_upcycle_reg(dest, HEX_REG_UPCYCLELO, ctx->zero);
    } else if (reg_num == HEX_REG_PC) {
        tcg_gen_movi_tl(dest, ctx->base.pc_next);
    } else if (reg_num == HEX_REG_QEMU_PKT_CNT) {
        tcg_gen_addi_tl(dest, hex_gpr[HEX_REG_QEMU_PKT_CNT],
                        ctx->num_packets);
    } else if (reg_num == HEX_REG_QEMU_INSN_CNT) {
        tcg_gen_addi_tl(dest, hex_gpr[HEX_REG_QEMU_INSN_CNT],
                        ctx->num_insns);
    } else if (reg_num == HEX_REG_QEMU_HVX_CNT) {
        tcg_gen_addi_tl(dest, hex_gpr[HEX_REG_QEMU_HVX_CNT],
                        ctx->num_hvx_insns);
    } else if (reg_num == HEX_REG_QEMU_HMX_CNT) {
        tcg_gen_addi_tl(dest, hex_gpr[HEX_REG_QEMU_HMX_CNT],
                        ctx->num_hmx_insns);
    } else if ((reg_num == HEX_REG_PKTCNTLO)
            || (reg_num == HEX_REG_PKTCNTHI)
            || (reg_num == HEX_REG_UTIMERLO)
            || (reg_num == HEX_REG_UTIMERHI) ) {
        gen_helper_creg_read(dest, cpu_env, tcg_constant_tl(reg_num));
    } else {
        tcg_gen_mov_tl(dest, hex_gpr[reg_num]);
    }
}

static inline void gen_read_ctrl_reg_pair(DisasContext *ctx, const int reg_num,
                                          TCGv_i64 dest)
{
    if (reg_num == HEX_REG_P3_0) {
        TCGv p3_0 = tcg_temp_new();
        gen_read_p3_0(p3_0);
        tcg_gen_concat_i32_i64(dest, p3_0, hex_gpr[reg_num + 1]);
        tcg_temp_free(p3_0);
    } else if (reg_num == HEX_REG_PC - 1) {
        tcg_gen_concat_i32_i64(dest, hex_gpr[reg_num],
                               tcg_constant_tl(ctx->base.pc_next));
    } else if (reg_num == HEX_REG_QEMU_PKT_CNT) {
        TCGv pkt_cnt = tcg_temp_new();
        TCGv insn_cnt = tcg_temp_new();
        tcg_gen_addi_tl(pkt_cnt, hex_gpr[HEX_REG_QEMU_PKT_CNT],
                        ctx->num_packets);
        tcg_gen_addi_tl(insn_cnt, hex_gpr[HEX_REG_QEMU_INSN_CNT],
                        ctx->num_insns);
        tcg_gen_concat_i32_i64(dest, pkt_cnt, insn_cnt);
        tcg_temp_free(pkt_cnt);
        tcg_temp_free(insn_cnt);
    } else if (reg_num == HEX_REG_QEMU_HVX_CNT) {
        TCGv hvx_cnt = tcg_temp_new();
        TCGv hmx_cnt = tcg_temp_new();
        tcg_gen_addi_tl(hvx_cnt, hex_gpr[HEX_REG_QEMU_HVX_CNT],
                        ctx->num_hvx_insns);
        tcg_gen_addi_tl(hmx_cnt, hex_gpr[HEX_REG_QEMU_HMX_CNT],
                        ctx->num_hmx_insns);
        tcg_gen_concat_i32_i64(dest, hvx_cnt, hmx_cnt);
        tcg_temp_free(hvx_cnt);
        tcg_temp_free(hvx_cnt);
    } else if ((reg_num == HEX_REG_PKTCNTLO)
            || (reg_num == HEX_REG_UTIMERLO)
            || (reg_num == HEX_REG_UPCYCLELO)
            ) {
        gen_helper_creg_read_pair(dest, cpu_env, tcg_constant_i32(reg_num));
    } else {
        tcg_gen_concat_i32_i64(dest,
            hex_gpr[reg_num],
            hex_gpr[reg_num + 1]);
    }
}

static inline void gen_write_p3_0(DisasContext *ctx, TCGv control_reg)
{
    TCGv hex_p8 = tcg_temp_new();
    for (int i = 0; i < NUM_PREGS; i++) {
        tcg_gen_extract_tl(hex_p8, control_reg, i * 8, 8);
        gen_log_pred_write(ctx, i, hex_p8);
        ctx_log_pred_write(ctx, i);
    }
    tcg_temp_free(hex_p8);
}

/*
 * Certain control registers require special handling on write
 *     HEX_REG_P3_0          aliased to the predicate registers
 *                           -> break the value across 4 predicate registers
 *     HEX_REG_QEMU_*_CNT    changes in current TB in DisasContext
 *                            -> clear the changes
 */
static inline void gen_write_ctrl_reg(DisasContext *ctx, int reg_num,
                                      TCGv val)
{
    if (reg_num == HEX_REG_P3_0) {
        gen_write_p3_0(ctx, val);
    } else {
        gen_log_reg_write(reg_num, val);
        ctx_log_reg_write(ctx, reg_num);
        if (reg_num == HEX_REG_QEMU_PKT_CNT) {
            ctx->num_packets = 0;
        }
        if (reg_num == HEX_REG_QEMU_INSN_CNT) {
            ctx->num_insns = 0;
        }
        if (reg_num == HEX_REG_QEMU_HVX_CNT) {
            ctx->num_hvx_insns = 0;
        }
    }
}

static inline void gen_write_ctrl_reg_pair(DisasContext *ctx, int reg_num,
                                           TCGv_i64 val)
{
    if (reg_num == HEX_REG_P3_0) {
        TCGv val32 = tcg_temp_new();
        tcg_gen_extrl_i64_i32(val32, val);
        gen_write_p3_0(ctx, val32);
        tcg_gen_extrh_i64_i32(val32, val);
        gen_log_reg_write(reg_num + 1, val32);
        tcg_temp_free(val32);
        ctx_log_reg_write(ctx, reg_num + 1);
    } else {
        gen_log_reg_write_pair(reg_num, val);
        ctx_log_reg_write_pair(ctx, reg_num);
        if (reg_num == HEX_REG_QEMU_PKT_CNT) {
            ctx->num_packets = 0;
            ctx->num_insns = 0;
        }
        if (reg_num == HEX_REG_QEMU_HVX_CNT) {
            ctx->num_hvx_insns = 0;
        }
    }
}

static TCGv gen_get_word(TCGv result, int N, TCGv_i64 src, bool sign)
{
    if (N == 0) {
        tcg_gen_extrl_i64_i32(result, src);
    } else if (N == 1) {
        tcg_gen_extrh_i64_i32(result, src);
    } else {
      g_assert_not_reached();
    }
    return result;
}

static TCGv_i64 gen_get_word_i64(TCGv_i64 result, int N, TCGv_i64 src,
                                        bool sign)
{
    TCGv word = tcg_temp_new();
    gen_get_word(word, N, src, sign);
    if (sign) {
        tcg_gen_ext_i32_i64(result, word);
    } else {
        tcg_gen_extu_i32_i64(result, word);
    }
    tcg_temp_free(word);
    return result;
}

static TCGv gen_get_byte(TCGv result, int N, TCGv src, bool sign)
{
    if (sign) {
        tcg_gen_sextract_tl(result, src, N * 8, 8);
    } else {
        tcg_gen_extract_tl(result, src, N * 8, 8);
    }
    return result;
}

static TCGv gen_get_byte_i64(TCGv result, int N, TCGv_i64 src, bool sign)
{
    TCGv_i64 res64 = tcg_temp_new_i64();
    if (sign) {
        tcg_gen_sextract_i64(res64, src, N * 8, 8);
    } else {
        tcg_gen_extract_i64(res64, src, N * 8, 8);
    }
    tcg_gen_extrl_i64_i32(result, res64);
    tcg_temp_free_i64(res64);

    return result;
}

static inline TCGv gen_get_half(TCGv result, int N, TCGv src, bool sign)
{
    if (sign) {
        tcg_gen_sextract_tl(result, src, N * 16, 16);
    } else {
        tcg_gen_extract_tl(result, src, N * 16, 16);
    }
    return result;
}

static inline void gen_set_half(int N, TCGv result, TCGv src)
{
    tcg_gen_deposit_tl(result, result, src, N * 16, 16);
}

static inline void gen_set_half_i64(int N, TCGv_i64 result, TCGv src)
{
    TCGv_i64 src64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(src64, src);
    tcg_gen_deposit_i64(result, result, src64, N * 16, 16);
    tcg_temp_free_i64(src64);
}

static void gen_set_byte_i64(int N, TCGv_i64 result, TCGv src)
{
    TCGv_i64 src64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(src64, src);
    tcg_gen_deposit_i64(result, result, src64, N * 8, 8);
    tcg_temp_free_i64(src64);
}

static inline void gen_load_locked4u(TCGv dest, TCGv vaddr, int mem_index)
{
    tcg_gen_qemu_ld32u(dest, vaddr, mem_index);
    tcg_gen_mov_tl(hex_llsc_addr, vaddr);
    tcg_gen_mov_tl(hex_llsc_val, dest);
}

static inline void gen_load_locked8u(TCGv_i64 dest, TCGv vaddr, int mem_index)
{
    tcg_gen_qemu_ld64(dest, vaddr, mem_index);
    tcg_gen_mov_tl(hex_llsc_addr, vaddr);
    tcg_gen_mov_i64(hex_llsc_val_i64, dest);
}

static inline void gen_store_conditional4(DisasContext *ctx,
                                          TCGv pred, TCGv vaddr, TCGv src)
{
    TCGLabel *fail = gen_new_label();
    TCGLabel *done = gen_new_label();
    TCGv tmp;

    tcg_gen_brcond_tl(TCG_COND_NE, vaddr, hex_llsc_addr, fail);

    tmp = tcg_temp_new();
    tcg_gen_atomic_cmpxchg_tl(tmp, hex_llsc_addr, hex_llsc_val, src,
                              ctx->mem_idx, MO_32);
    tcg_gen_movcond_tl(TCG_COND_EQ, pred, tmp, hex_llsc_val,
                       ctx->ones, ctx->zero);
    tcg_temp_free(tmp);
    tcg_gen_br(done);

    gen_set_label(fail);
    tcg_gen_movi_tl(pred, 0);

    gen_set_label(done);
    tcg_gen_movi_tl(hex_llsc_addr, ~0);
}

static inline void gen_store_conditional8(DisasContext *ctx,
                                          TCGv pred, TCGv vaddr, TCGv_i64 src)
{
    TCGLabel *fail = gen_new_label();
    TCGLabel *done = gen_new_label();
    TCGv_i64 tmp;

    tcg_gen_brcond_tl(TCG_COND_NE, vaddr, hex_llsc_addr, fail);

    tmp = tcg_temp_new_i64();
    tcg_gen_atomic_cmpxchg_i64(tmp, hex_llsc_addr, hex_llsc_val_i64, src,
                               ctx->mem_idx, MO_64);
    tcg_gen_movcond_i64(TCG_COND_EQ, tmp, tmp, hex_llsc_val_i64,
                        ctx->ones64, ctx->zero64);
    tcg_gen_extrl_i64_i32(pred, tmp);
    tcg_temp_free_i64(tmp);
    tcg_gen_br(done);

    gen_set_label(fail);
    tcg_gen_movi_tl(pred, 0);

    gen_set_label(done);
    tcg_gen_movi_tl(hex_llsc_addr, ~0);
}

static inline void gen_store32(TCGv vaddr, TCGv src, int width, int slot)
{
    tcg_gen_mov_tl(hex_store_addr[slot], vaddr);
    tcg_gen_movi_tl(hex_store_width[slot], width);
    tcg_gen_mov_tl(hex_store_val32[slot], src);
}

static inline void gen_store1(TCGv_env cpu_env, TCGv vaddr, TCGv src,
                              DisasContext *ctx, int slot)
{
    gen_store32(vaddr, src, 1, slot);
    ctx->store_width[slot] = 1;
}

static inline void gen_store1i(TCGv_env cpu_env, TCGv vaddr, int32_t src,
                               DisasContext *ctx, int slot)
{
    gen_store1(cpu_env, vaddr, tcg_constant_tl(src), ctx, slot);
}

static inline void gen_store2(TCGv_env cpu_env, TCGv vaddr, TCGv src,
                              DisasContext *ctx, int slot)
{
    gen_store32(vaddr, src, 2, slot);
    ctx->store_width[slot] = 2;
}

static inline void gen_store2i(TCGv_env cpu_env, TCGv vaddr, int32_t src,
                               DisasContext *ctx, int slot)
{
    gen_store2(cpu_env, vaddr, tcg_constant_tl(src), ctx, slot);
}

static inline void gen_store4(TCGv_env cpu_env, TCGv vaddr, TCGv src,
                              DisasContext *ctx, int slot)
{
    gen_store32(vaddr, src, 4, slot);
    ctx->store_width[slot] = 4;
}

static inline void gen_store4i(TCGv_env cpu_env, TCGv vaddr, int32_t src,
                               DisasContext *ctx, int slot)
{
    gen_store4(cpu_env, vaddr, tcg_constant_tl(src), ctx, slot);
}

static inline void gen_store8(TCGv_env cpu_env, TCGv vaddr, TCGv_i64 src,
                              DisasContext *ctx, int slot)
{
    tcg_gen_mov_tl(hex_store_addr[slot], vaddr);
    tcg_gen_movi_tl(hex_store_width[slot], 8);
    tcg_gen_mov_i64(hex_store_val64[slot], src);
    ctx->store_width[slot] = 8;
}

static inline void gen_store8i(TCGv_env cpu_env, TCGv vaddr, int64_t src,
                               DisasContext *ctx, int slot)
{
    gen_store8(cpu_env, vaddr, tcg_constant_i64(src), ctx, slot);
}

static TCGv gen_8bitsof(TCGv result, TCGv value, DisasContext *ctx)
{
    tcg_gen_movcond_tl(TCG_COND_NE, result, value,
                       ctx->zero, ctx->ones, ctx->zero);

    return result;
}

static void gen_write_new_pc_addr(DisasContext *ctx, Packet *pkt, TCGv addr, TCGv pred)
{
    TCGLabel *pred_false = NULL;
    if (pkt->pkt_has_multi_cof) {
        if (pred != NULL) {
            pred_false = gen_new_label();
            tcg_gen_brcondi_tl(TCG_COND_EQ, pred, 0, pred_false);
        }
        /* If there are multiple branches in a packet, ignore the second one */
        TCGv zero = tcg_constant_tl(0);
        tcg_gen_movcond_tl(TCG_COND_NE, hex_next_PC,
                           hex_branch_taken, zero,
                           hex_next_PC, addr);
        tcg_gen_movi_tl(hex_branch_taken, 1);
        if (pred != NULL) {
            gen_set_label(pred_false);
        }
    } else {
        if (pred != NULL) {
            pred_false = gen_new_label();
            tcg_gen_brcondi_tl(TCG_COND_EQ, pred, 0, pred_false);
        }
        tcg_gen_mov_tl(hex_next_PC, addr);
        if (pred != NULL) {
            gen_set_label(pred_false);
        }
    }
}

static void gen_write_new_pc_pcrel(DisasContext *ctx, Packet *pkt, int pc_off, TCGv pred)
{
    target_ulong dest = ctx->base.pc_next + pc_off;
    if (pkt->pkt_has_multi_cof) {
        TCGLabel *pred_false = NULL;
        if (pred != NULL) {
            pred_false = gen_new_label();
            tcg_gen_brcondi_tl(TCG_COND_EQ, pred, 0, pred_false);
        }
        /* If there are multiple branches in a packet, ignore the second one */
        TCGLabel *skip = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_NE, hex_branch_taken, 0, skip);
        tcg_gen_movi_tl(hex_next_PC, dest);
        tcg_gen_movi_tl(hex_branch_taken, 1);
        gen_set_label(skip);
        if (pred != NULL) {
            gen_set_label(pred_false);
        }
    } else {
        TCGLabel *pred_false = NULL;
        if (pred != NULL) {
            pred_false = gen_new_label();
            tcg_gen_brcondi_tl(TCG_COND_EQ, pred, 0, pred_false);
        }
        tcg_gen_movi_tl(hex_next_PC, dest);
        if (pred != NULL) {
            gen_set_label(pred_false);
        }
    }
}

static inline void gen_set_usr_field(int field, TCGv val)
{
    tcg_gen_deposit_tl(hex_new_value[HEX_REG_USR], hex_new_value[HEX_REG_USR],
                       val,
                       reg_field_info[field].offset,
                       reg_field_info[field].width);
}

static inline void gen_set_usr_fieldi(int field, int x)
{
    gen_set_usr_field(field, tcg_constant_tl(x));
}

static inline void gen_cond_return(TCGv pred, TCGv addr, TCGv zero)
{
    tcg_gen_movcond_tl(TCG_COND_NE, hex_next_PC, pred, zero, addr, hex_next_PC);
}

static inline void gen_loop0r(TCGv RsV, int riV, Insn *insn)
{
    TCGv tmp = tcg_temp_new();
    fIMMEXT(riV);
    fPCALIGN(riV);
    /* fWRITE_LOOP_REGS0( fREAD_PC()+riV, RsV); */
    tcg_gen_addi_tl(tmp, hex_gpr[HEX_REG_PC], riV);
    gen_log_reg_write(HEX_REG_LC0, RsV);
    gen_log_reg_write(HEX_REG_SA0, tmp);
    fSET_LPCFG(0);
    tcg_temp_free(tmp);
}
static void gen_loop0i(int count, int riV, Insn *insn)
{
    gen_loop0r(tcg_constant_tl(count), riV, insn);
}

static inline void gen_loop1r(TCGv RsV, int riV, Insn *insn)
{
    TCGv tmp = tcg_temp_new();
    fIMMEXT(riV);
    fPCALIGN(riV);
    /* fWRITE_LOOP_REGS1( fREAD_PC()+riV, RsV); */
    tcg_gen_addi_tl(tmp, hex_gpr[HEX_REG_PC], riV);
    gen_log_reg_write(HEX_REG_LC1, RsV);
    gen_log_reg_write(HEX_REG_SA1, tmp);
    tcg_temp_free(tmp);
}

static void gen_loop1i(int count, int riV, Insn *insn)
{
    gen_loop1r(tcg_constant_tl(count), riV, insn);
}

static inline void gen_compare(TCGCond cond, TCGv res, TCGv arg1, TCGv arg2,
                               DisasContext *ctx)
{
    tcg_gen_movcond_tl(cond, res, arg1, arg2, ctx->ones, ctx->zero);
}

static inline void gen_comparei(TCGCond cond, TCGv res, TCGv arg1, int arg2,
                                DisasContext *ctx)
{
    gen_compare(cond, res, arg1, tcg_constant_tl(arg2), ctx);
}

static inline void gen_compare_i64(TCGCond cond, TCGv res,
                                   TCGv_i64 arg1, TCGv_i64 arg2,
                                   DisasContext *ctx)
{
    TCGv_i64 temp = tcg_temp_new_i64();

    tcg_gen_movcond_i64(cond, temp, arg1, arg2, ctx->ones64, ctx->zero64);
    tcg_gen_extrl_i64_i32(res, temp);
    tcg_gen_andi_tl(res, res, 0xff);

    tcg_temp_free_i64(temp);
}

static void gen_cond_jumpr(DisasContext *ctx, Packet *pkt,
                           TCGv pred, TCGv dst_pc)
{
    gen_write_new_pc_addr(ctx, pkt, dst_pc, pred);
}

static void gen_cond_jump(DisasContext *ctx, Packet *pkt, TCGv pred, int pc_off)
{
    gen_write_new_pc_pcrel(ctx, pkt, pc_off, pred);
}

static inline void gen_cmpnd_cmp_jmp(DisasContext *ctx, Packet *pkt, Insn *insn,
                                     int pnum, TCGCond cond,
                                     bool sense, TCGv arg1, TCGv arg2,
                                     int pc_off)
{
    if (insn->part1) {
        TCGv pred = tcg_temp_new();
        gen_compare(cond, pred, arg1, arg2, ctx);
        gen_log_pred_write(ctx, pnum, pred);
        tcg_temp_free(pred);
    } else {
        TCGv pred = tcg_temp_new();

        tcg_gen_mov_tl(pred, hex_new_pred_value[pnum]);
        if (!sense) {
            tcg_gen_xori_tl(pred, pred, 0xff);
        }

        gen_cond_jump(ctx, pkt, pred, pc_off);
        tcg_temp_free(pred);
    }
}

static void gen_cmpnd_cmpi_jmp(DisasContext *ctx, Packet *pkt, Insn *insn,
                               int pnum, TCGCond cond,
                               bool sense, TCGv arg1, int arg2,
                               int pc_off)
{
    gen_cmpnd_cmp_jmp(ctx, pkt, insn, pnum, cond, sense, arg1,
                      tcg_constant_tl(arg2), pc_off);

}

static void gen_cmpnd_cmp_n1_jmp(DisasContext *ctx, Packet *pkt, Insn *insn,
                                 int pnum, TCGCond cond,
                                 bool sense, TCGv arg, int pc_off)
{
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, pnum, cond, sense, arg, -1, pc_off);
}


static void gen_jump(DisasContext *ctx, Packet *pkt, int pc_off)
{
    gen_write_new_pc_pcrel(ctx, pkt, pc_off, NULL);
}

static void gen_jumpr(DisasContext *ctx, Packet *pkt, TCGv new_pc)
{
    gen_write_new_pc_addr(ctx, pkt, new_pc, NULL);
}

static void gen_call(DisasContext *ctx, Packet *pkt, int pc_off)
{
    gen_log_reg_write(HEX_REG_LR, hex_next_PC);
    gen_write_new_pc_pcrel(ctx, pkt, pc_off, NULL);
}

static void gen_callr(DisasContext *ctx, Packet *pkt, TCGv new_pc)
{
    gen_log_reg_write(HEX_REG_LR, hex_next_PC);
    gen_write_new_pc_addr(ctx, pkt, new_pc, NULL);
}

static void gen_cond_call(DisasContext *ctx, Packet *pkt,
                          TCGv pred, bool sense, int pc_off)
{
    TCGv lsb = tcg_temp_local_new();
    TCGLabel *skip = gen_new_label();
    tcg_gen_andi_tl(lsb, pred, 1);
    if (!sense) {
        tcg_gen_xori_tl(lsb, lsb, 1);
    }
    tcg_gen_brcondi_tl(TCG_COND_EQ, lsb, 0, skip);
    tcg_temp_free(lsb);
    gen_log_reg_write(HEX_REG_LR, hex_next_PC);
    gen_set_label(skip);
    gen_write_new_pc_pcrel(ctx, pkt, pc_off, lsb);
}

static void gen_cond_callr(DisasContext *ctx, Packet *pkt,
                           TCGv pred, bool sense, TCGv new_pc)
{
    TCGv lsb = tcg_temp_new();
    TCGLabel *skip = gen_new_label();
    tcg_gen_andi_tl(lsb, pred, 1);
    if (!sense) {
        tcg_gen_xori_tl(lsb, lsb, 1);
    }
    tcg_gen_brcondi_tl(TCG_COND_EQ, lsb, 0, skip);
    tcg_temp_free(lsb);
    gen_callr(ctx, pkt, new_pc);
    gen_set_label(skip);
}

static void gen_endloop0(DisasContext *ctx, Packet *pkt)
{
    TCGv lpcfg = tcg_temp_local_new();

    GET_USR_FIELD(USR_LPCFG, lpcfg);

    /*
     *    if (lpcfg == 1) {
     *        hex_new_pred_value[3] = 0xff;
     *        hex_pred_written |= 1 << 3;
     *    }
     */
    TCGLabel *label1 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_NE, lpcfg, 1, label1);
    {
        tcg_gen_movi_tl(hex_new_pred_value[3], 0xff);
        tcg_gen_ori_tl(hex_pred_written, hex_pred_written, 1 << 3);
    }
    gen_set_label(label1);

    /*
     *    if (lpcfg) {
     *        SET_USR_FIELD(USR_LPCFG, lpcfg - 1);
     *    }
     */
    TCGLabel *label2 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, lpcfg, 0, label2);
    {
        tcg_gen_subi_tl(lpcfg, lpcfg, 1);
        SET_USR_FIELD(USR_LPCFG, lpcfg);
    }
    gen_set_label(label2);

    /*
     *    if (hex_gpr[HEX_REG_LC0] > 1) {
     *        PC = hex_gpr[HEX_REG_SA0];
     *        hex_gpr[HEX_REG_LC0] = hex_gpr[HEX_REG_LC0] - 1;
     *    }
     */
    TCGLabel *label3 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC0], 1, label3);
    {
            gen_jumpr(ctx, pkt, hex_gpr[HEX_REG_SA0]);
            tcg_gen_subi_tl(hex_new_value[HEX_REG_LC0],
                            hex_gpr[HEX_REG_LC0], 1);
    }
    gen_set_label(label3);

    tcg_temp_free(lpcfg);
}

static inline void gen_endloop1(DisasContext *ctx, Packet *pkt)
{
    /*
     *    if (hex_gpr[HEX_REG_LC1] > 1) {
     *        PC = hex_gpr[HEX_REG_SA1];
     *        hex_gpr[HEX_REG_LC1] = hex_gpr[HEX_REG_LC1] - 1;
     *    }
     */
    TCGLabel *label = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC1], 1, label);
    {
        gen_jumpr(ctx, pkt, hex_gpr[HEX_REG_SA1]);
        tcg_gen_subi_tl(hex_new_value[HEX_REG_LC1], hex_gpr[HEX_REG_LC1], 1);
    }
    gen_set_label(label);
}

static void gen_endloop01(DisasContext *ctx, Packet *pkt)
{
    TCGv lpcfg = tcg_temp_local_new();

    GET_USR_FIELD(USR_LPCFG, lpcfg);

    /*
     *    if (lpcfg == 1) {
     *        hex_new_pred_value[3] = 0xff;
     *        hex_pred_written |= 1 << 3;
     *    }
     */
    TCGLabel *label1 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_NE, lpcfg, 1, label1);
    {
        tcg_gen_movi_tl(hex_new_pred_value[3], 0xff);
        tcg_gen_ori_tl(hex_pred_written, hex_pred_written, 1 << 3);
    }
    gen_set_label(label1);

    /*
     *    if (lpcfg) {
     *        SET_USR_FIELD(USR_LPCFG, lpcfg - 1);
     *    }
     */
    TCGLabel *label2 = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_EQ, lpcfg, 0, label2);
    {
        tcg_gen_subi_tl(lpcfg, lpcfg, 1);
        SET_USR_FIELD(USR_LPCFG, lpcfg);
    }
    gen_set_label(label2);

    /*
     *    if (hex_gpr[HEX_REG_LC0] > 1) {
     *        PC = hex_gpr[HEX_REG_SA0];
     *        hex_new_value[HEX_REG_LC0] = hex_gpr[HEX_REG_LC0] - 1;
     *    } else {
     *        if (hex_gpr[HEX_REG_LC1] > 1) {
     *            hex_next_pc = hex_gpr[HEX_REG_SA1];
     *            hex_new_value[HEX_REG_LC1] = hex_gpr[HEX_REG_LC1] - 1;
     *        }
     *    }
     */
    TCGLabel *label3 = gen_new_label();
    TCGLabel *done = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC0], 1, label3);
    {
        gen_jumpr(ctx, pkt, hex_gpr[HEX_REG_SA0]);
        tcg_gen_subi_tl(hex_new_value[HEX_REG_LC0], hex_gpr[HEX_REG_LC0], 1);
        tcg_gen_br(done);
    }
    gen_set_label(label3);
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC1], 1, done);
    {
        gen_jumpr(ctx, pkt, hex_gpr[HEX_REG_SA1]);
        tcg_gen_subi_tl(hex_new_value[HEX_REG_LC1], hex_gpr[HEX_REG_LC1], 1);
    }
    gen_set_label(done);
    tcg_temp_free(lpcfg);
}

static inline void gen_ashiftr_4_4s(TCGv dst, TCGv src, int32_t shift_amt)
{
    tcg_gen_sari_tl(dst, src, shift_amt);
}

static inline void gen_ashiftl_4_4s(TCGv dst, TCGv src, int32_t shift_amt)
{
    if (shift_amt >= 32) {
        tcg_gen_movi_tl(dst, 0);
    } else {
        tcg_gen_shli_tl(dst, src, shift_amt);
    }
}

static void gen_cmp_jumpnv(DisasContext *ctx, Packet *pkt,
                           TCGCond cond, TCGv val, TCGv src, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_setcond_tl(cond, pred, val, src);
    gen_cond_jump(ctx, pkt, pred, pc_off);
    tcg_temp_free(pred);
}

static void gen_cmpi_jumpnv(DisasContext *ctx, Packet *pkt,
                            TCGCond cond, TCGv val, int src, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_setcondi_tl(cond, pred, val, src);
    gen_cond_jump(ctx, pkt, pred, pc_off);
    tcg_temp_free(pred);
}

static inline void gen_asl_r_r_or(TCGv RxV, TCGv RsV, TCGv RtV, TCGv zero)
{
    TCGv shift_amt = tcg_temp_new();
    TCGv_i64 shift_amt_i64 = tcg_temp_new_i64();
    TCGv_i64 shift_left_val_i64 = tcg_temp_new_i64();
    TCGv shift_left_val = tcg_temp_new();
    TCGv_i64 shift_right_val_i64 = tcg_temp_new_i64();
    TCGv shift_right_val = tcg_temp_new();
    TCGv or_val = tcg_temp_new();

    /* Sign extend 7->32 bits */
    tcg_gen_shli_tl(shift_amt, RtV, 32 - 7);
    tcg_gen_sari_tl(shift_amt, shift_amt, 32 - 7);
    tcg_gen_ext_i32_i64(shift_amt_i64, shift_amt);

    tcg_gen_ext_i32_i64(shift_left_val_i64, RsV);
    tcg_gen_shl_i64(shift_left_val_i64, shift_left_val_i64, shift_amt_i64);
    tcg_gen_extrl_i64_i32(shift_left_val, shift_left_val_i64);

    /* ((-(SHAMT)) - 1) */
    tcg_gen_neg_i64(shift_amt_i64, shift_amt_i64);
    tcg_gen_subi_i64(shift_amt_i64, shift_amt_i64, 1);

    tcg_gen_ext_i32_i64(shift_right_val_i64, RsV);
    tcg_gen_sar_i64(shift_right_val_i64, shift_right_val_i64, shift_amt_i64);
    tcg_gen_sari_i64(shift_right_val_i64, shift_right_val_i64, 1);
    tcg_gen_extrl_i64_i32(shift_right_val, shift_right_val_i64);

    tcg_gen_movcond_tl(TCG_COND_GE, or_val, shift_amt, zero,
                       shift_left_val, shift_right_val);
    tcg_gen_or_tl(RxV, RxV, or_val);

    tcg_temp_free(shift_amt);
    tcg_temp_free_i64(shift_amt_i64);
    tcg_temp_free_i64(shift_left_val_i64);
    tcg_temp_free(shift_left_val);
    tcg_temp_free_i64(shift_right_val_i64);
    tcg_temp_free(shift_right_val);
    tcg_temp_free(or_val);
}

static inline void gen_lshiftr_4_4u(TCGv dst, TCGv src, int32_t shift_amt)
{
    if (shift_amt >= 32) {
        tcg_gen_movi_tl(dst, 0);
    } else {
        tcg_gen_shri_tl(dst, src, shift_amt);
    }
}

static void gen_sat(TCGv dst, TCGv src, bool sign, uint32_t bits)
{
    uint32_t min = sign ? -(1 << (bits - 1)) : 0;
    uint32_t max = sign ? (1 << (bits - 1)) - 1 : (1 << bits) - 1;
    TCGLabel *label = gen_new_label();

    if (sign) {
        tcg_gen_sextract_tl(dst, src, 0, bits);
    } else {
        tcg_gen_extract_tl(dst, src, 0, bits);
    }
    tcg_gen_brcond_tl(TCG_COND_EQ, dst, src, label);
    {
        TCGv tcg_min = tcg_constant_tl(min);
        TCGv tcg_max = tcg_constant_tl(max);
        tcg_gen_movcond_tl(TCG_COND_LT, dst, src, tcg_min, tcg_min, tcg_max);
        gen_set_usr_fieldi(USR_OVF, 1);
    }
    gen_set_label(label);
}

static void gen_sat_i64(TCGv_i64 dst, TCGv_i64 src, uint32_t bits)
{
    TCGLabel *label = gen_new_label();

    tcg_gen_sextract_i64(dst, src, 0, bits);
    tcg_gen_brcond_i64(TCG_COND_EQ, dst, src, label);
    {
        TCGv_i64 min = tcg_constant_i64(-(1LL << (bits - 1)));
        TCGv_i64 max = tcg_constant_i64((1LL << (bits - 1)) - 1);
        tcg_gen_movcond_i64(TCG_COND_LT, dst, src, tcg_constant_i64(0),
                            min, max);
        gen_set_usr_fieldi(USR_OVF, 1);
    }
    gen_set_label(label);
}

static void gen_satval(TCGv_i64 dest, TCGv_i64 source, uint32_t bits)
{
    TCGv_i64 min = tcg_constant_i64(-(1LL << (bits - 1)));
    TCGv_i64 max = tcg_constant_i64((1LL << (bits - 1)) - 1);

    gen_set_usr_fieldi(USR_OVF, 1);
    tcg_gen_movcond_i64(TCG_COND_LT, dest, source, tcg_constant_i64(0),
                        min, max);
}

/* Shift left with saturation */
static void gen_shl_sat(TCGv RdV, TCGv RsV, TCGv shift_amt)
{
    /*
     * int64_t A = (fCAST4_8s(RsV) << shift_amt;
     * if (((int32_t)((fSAT(A)) ^ ((int32_t)(RsV)))) < 0) {
     *     RdV = fSATVALN(32, ((int32_t)(RsV)))
     * } else if (((RsV) > 0) && ((A) == 0)) {
     *     RdV = fSATVALN(32, (RsV));
     * } else {
     *     RdV = fSAT(A);
     * }
     */
    TCGv_i64 RsV_i64 = tcg_temp_local_new_i64();
    TCGv_i64 shift_amt_i64 = tcg_temp_local_new_i64();
    TCGv_i64 A = tcg_temp_local_new_i64();
    TCGv_i64 A_sat_i64 = tcg_temp_local_new_i64();
    TCGv A_sat = tcg_temp_local_new();
    TCGv_i64 RdV_i64 = tcg_temp_local_new_i64();
    TCGv tmp = tcg_temp_new();
    TCGLabel *label1 = gen_new_label();
    TCGLabel *label2 = gen_new_label();
    TCGLabel *done = gen_new_label();

    tcg_gen_ext_i32_i64(RsV_i64, RsV);
    tcg_gen_ext_i32_i64(shift_amt_i64, shift_amt);
    tcg_gen_shl_i64(A, RsV_i64, shift_amt_i64);

    /* Check for saturation */
    gen_sat_i64(A_sat_i64, A, 32);
    tcg_gen_extrl_i64_i32(A_sat, A_sat_i64);
    tcg_gen_xor_tl(tmp, A_sat, RsV);
    tcg_gen_brcondi_tl(TCG_COND_GE, tmp, 0, label1);
    gen_satval(RdV_i64, RsV_i64, 32);
    tcg_gen_extrl_i64_i32(RdV, RdV_i64);
    tcg_gen_br(done);

    gen_set_label(label1);
    tcg_gen_brcondi_tl(TCG_COND_LE, RsV, 0, label2);
    tcg_gen_brcondi_i64(TCG_COND_NE, A, 0, label2);
    gen_satval(RdV_i64, RsV_i64, 32);
    tcg_gen_extrl_i64_i32(RdV, RdV_i64);
    tcg_gen_br(done);

    gen_set_label(label2);
    tcg_gen_mov_tl(RdV, A_sat);

    gen_set_label(done);

    tcg_temp_free_i64(RsV_i64);
    tcg_temp_free_i64(shift_amt_i64);
    tcg_temp_free_i64(A);
    tcg_temp_free_i64(A_sat_i64);
    tcg_temp_free(A_sat);
    tcg_temp_free_i64(RdV_i64);
    tcg_temp_free(tmp);
}

/* Bidirectional shift right with saturation */
static void gen_asr_r_r_sat(TCGv RdV, TCGv RsV, TCGv RtV)
{
    TCGv shift_amt = tcg_temp_local_new();
    TCGLabel *positive = gen_new_label();
    TCGLabel *done = gen_new_label();

    tcg_gen_sextract_i32(shift_amt, RtV, 0, 7);
    tcg_gen_brcondi_tl(TCG_COND_GE, shift_amt, 0, positive);

    /* Negative shift amount => shift left */
    tcg_gen_neg_tl(shift_amt, shift_amt);
    gen_shl_sat(RdV, RsV, shift_amt);
    tcg_gen_br(done);

    gen_set_label(positive);
    /* Positive shift amount => shift right */
    tcg_gen_sar_tl(RdV, RsV, shift_amt);

    gen_set_label(done);

    tcg_temp_free(shift_amt);
}

/* Bidirectional shift left with saturation */
static void gen_asl_r_r_sat(TCGv RdV, TCGv RsV, TCGv RtV)
{
    TCGv shift_amt = tcg_temp_local_new();
    TCGLabel *positive = gen_new_label();
    TCGLabel *done = gen_new_label();

    tcg_gen_sextract_i32(shift_amt, RtV, 0, 7);
    tcg_gen_brcondi_tl(TCG_COND_GE, shift_amt, 0, positive);

    /* Negative shift amount => shift right */
    tcg_gen_neg_tl(shift_amt, shift_amt);
    tcg_gen_sar_tl(RdV, RsV, shift_amt);
    tcg_gen_br(done);

    gen_set_label(positive);
    /* Positive shift amount => shift left */
    gen_shl_sat(RdV, RsV, shift_amt);

    gen_set_label(done);

    tcg_temp_free(shift_amt);
}

static intptr_t vreg_src_off(DisasContext *ctx, int num)
{
    intptr_t offset = offsetof(CPUHexagonState, VRegs[num]);

    if (test_bit(num, ctx->vregs_select)) {
        offset = ctx_future_vreg_off(ctx, num, 1, false);
    }
    if (test_bit(num, ctx->vregs_updated_tmp)) {
        offset = ctx_tmp_vreg_off(ctx, num, 1, false);
    }
    return offset;
}

static void gen_log_vreg_write(DisasContext *ctx, intptr_t srcoff, int num,
                               VRegWriteType type, int slot_num,
                               bool is_predicated)
{
    TCGLabel *label_end = NULL;
    intptr_t dstoff;

    if (is_predicated) {
        TCGv cancelled = tcg_temp_local_new();
        label_end = gen_new_label();

        /* Don't do anything if the slot was cancelled */
        tcg_gen_extract_tl(cancelled, hex_slot_cancelled, slot_num, 1);
        tcg_gen_brcondi_tl(TCG_COND_NE, cancelled, 0, label_end);
        tcg_temp_free(cancelled);
    }

    if (type != EXT_TMP) {
        dstoff = ctx_future_vreg_off(ctx, num, 1, true);
        tcg_gen_gvec_mov(MO_64, dstoff, srcoff,
                         sizeof(MMVector), sizeof(MMVector));
        tcg_gen_ori_tl(hex_VRegs_updated, hex_VRegs_updated, 1 << num);
    } else {
        dstoff = ctx_tmp_vreg_off(ctx, num, 1, false);
        tcg_gen_gvec_mov(MO_64, dstoff, srcoff,
                         sizeof(MMVector), sizeof(MMVector));
    }

    if (is_predicated) {
        gen_set_label(label_end);
    }
}

static void gen_log_vreg_write_pair(DisasContext *ctx, intptr_t srcoff, int num,
                                    VRegWriteType type, int slot_num,
                                    bool is_predicated)
{
    gen_log_vreg_write(ctx, srcoff, num ^ 0, type, slot_num, is_predicated);
    srcoff += sizeof(MMVector);
    gen_log_vreg_write(ctx, srcoff, num ^ 1, type, slot_num, is_predicated);
}

static void gen_log_qreg_write(intptr_t srcoff, int num, int vnew,
                               int slot_num, bool is_predicated)
{
    TCGLabel *label_end = NULL;
    intptr_t dstoff;

    if (is_predicated) {
        TCGv cancelled = tcg_temp_local_new();
        label_end = gen_new_label();

        /* Don't do anything if the slot was cancelled */
        tcg_gen_extract_tl(cancelled, hex_slot_cancelled, slot_num, 1);
        tcg_gen_brcondi_tl(TCG_COND_NE, cancelled, 0, label_end);
        tcg_temp_free(cancelled);
    }

    dstoff = offsetof(CPUHexagonState, future_QRegs[num]);
    if (dstoff != srcoff) {
        tcg_gen_gvec_mov(MO_64, dstoff, srcoff, sizeof(MMQReg), sizeof(MMQReg));
    }

    if (is_predicated) {
        tcg_gen_ori_tl(hex_QRegs_updated, hex_QRegs_updated, 1 << num);
        gen_set_label(label_end);
    }
}

static inline void gen_pause(void) {
    tcg_gen_mov_tl(hex_gpr[HEX_REG_PC], hex_next_PC);
    gen_exception(EXCP_YIELD);
}

static void gen_vreg_load(DisasContext *ctx, intptr_t dstoff, TCGv src,
                          bool aligned)
{
    TCGv_i64 tmp = tcg_temp_new_i64();
    if (aligned) {
        tcg_gen_andi_tl(src, src, ~((int32_t)sizeof(MMVector) - 1));
    }
    for (int i = 0; i < sizeof(MMVector) / 8; i++) {
        tcg_gen_qemu_ld_i64(tmp, src, ctx->mem_idx, MO_UNALN | MO_TEUQ);
        tcg_gen_addi_tl(src, src, 8);
        tcg_gen_st_i64(tmp, cpu_env, dstoff + i * 8);
    }
    tcg_temp_free_i64(tmp);
}

static void gen_vreg_store(DisasContext *ctx, Insn *insn, Packet *pkt,
                           TCGv EA, intptr_t srcoff, int slot, bool aligned)
{
    intptr_t dstoff = offsetof(CPUHexagonState, vstore[slot].data);
    intptr_t maskoff = offsetof(CPUHexagonState, vstore[slot].mask);

    if (is_gather_store_insn(insn, pkt)) {
        gen_helper_gather_store(cpu_env, EA, tcg_constant_tl(slot));
        return;
    }

    tcg_gen_movi_tl(hex_vstore_pending[slot], 1);
    if (aligned) {
        tcg_gen_andi_tl(hex_vstore_addr[slot], EA,
                        ~((int32_t)sizeof(MMVector) - 1));
    } else {
        tcg_gen_mov_tl(hex_vstore_addr[slot], EA);
    }
    tcg_gen_movi_tl(hex_vstore_size[slot], sizeof(MMVector));

    /* Copy the data to the vstore buffer */
    tcg_gen_gvec_mov(MO_64, dstoff, srcoff, sizeof(MMVector), sizeof(MMVector));
    /* Set the mask to all 1's */
    tcg_gen_gvec_dup_imm(MO_64, maskoff, sizeof(MMQReg), sizeof(MMQReg), ~0LL);
}

static void gen_vreg_masked_store(DisasContext *ctx, TCGv EA, intptr_t srcoff,
                                  intptr_t bitsoff, int slot, bool invert)
{
    intptr_t dstoff = offsetof(CPUHexagonState, vstore[slot].data);
    intptr_t maskoff = offsetof(CPUHexagonState, vstore[slot].mask);

    tcg_gen_movi_tl(hex_vstore_pending[slot], 1);
    tcg_gen_andi_tl(hex_vstore_addr[slot], EA,
                    ~((int32_t)sizeof(MMVector) - 1));
    tcg_gen_movi_tl(hex_vstore_size[slot], sizeof(MMVector));

    /* Copy the data to the vstore buffer */
    tcg_gen_gvec_mov(MO_64, dstoff, srcoff, sizeof(MMVector), sizeof(MMVector));
    /* Copy the mask */
    tcg_gen_gvec_mov(MO_64, maskoff, bitsoff, sizeof(MMQReg), sizeof(MMQReg));
    if (invert) {
        tcg_gen_gvec_not(MO_64, maskoff, maskoff,
                         sizeof(MMQReg), sizeof(MMQReg));
    }
}

static void vec_to_qvec(size_t size, intptr_t dstoff, intptr_t srcoff,
    TCGv_i64 zero64)
{
    TCGv_i64 tmp = tcg_temp_new_i64();
    TCGv_i64 word = tcg_temp_new_i64();
    TCGv_i64 bits = tcg_temp_new_i64();
    TCGv_i64 mask = tcg_temp_new_i64();

    for (int i = 0; i < sizeof(MMVector) / 8; i++) {
        tcg_gen_ld_i64(tmp, cpu_env, srcoff + i * 8);
        tcg_gen_movi_i64(mask, 0);

        for (int j = 0; j < 8; j += size) {
            tcg_gen_extract_i64(word, tmp, j * 8, size * 8);
            tcg_gen_movcond_i64(TCG_COND_NE, bits, word, zero64,
                                tcg_constant_i64(~0), zero64);
            tcg_gen_deposit_i64(mask, mask, bits, j, size);
        }

        tcg_gen_st8_i64(mask, cpu_env, dstoff + i);
    }
    tcg_temp_free_i64(tmp);
    tcg_temp_free_i64(word);
    tcg_temp_free_i64(bits);
    tcg_temp_free_i64(mask);
}

#include "tcg_funcs_generated.c.inc"
#include "tcg_func_table_generated.c.inc"
