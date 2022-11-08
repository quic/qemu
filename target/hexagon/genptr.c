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
#include "mmvec/macros.h"
#undef QEMU_GENERATE
#include "gen_tcg.h"
#include "gen_tcg_hvx.h"

static TCGv gen_read_reg(TCGv result, int num)
{
    tcg_gen_mov_tl(result, hex_gpr[num]);
    return result;
}

static TCGv gen_read_preg(TCGv pred, uint8_t num)
{
    tcg_gen_mov_tl(pred, hex_pred[num]);
    return pred;
}

static void gen_log_reg_write(int rnum, TCGv val)
{
    tcg_gen_mov_tl(hex_new_value[rnum], val);
    if (HEX_DEBUG) {
        /* Do this so HELPER(debug_commit_end) will know */
        tcg_gen_movi_tl(hex_reg_written[rnum], 1);
    }
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

static void gen_log_pred_write(DisasContext *ctx, int pnum, TCGv val)
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

static void gen_read_p3_0(TCGv control_reg)
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
static void gen_read_ctrl_reg(DisasContext *ctx, const int reg_num, TCGv dest)
{
    if (reg_num == HEX_REG_P3_0) {
        gen_read_p3_0(dest);
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
    } else {
        tcg_gen_mov_tl(dest, hex_gpr[reg_num]);
    }
}

static void gen_read_ctrl_reg_pair(DisasContext *ctx, const int reg_num,
                                   TCGv_i64 dest)
{
    if (reg_num == HEX_REG_P3_0) {
        TCGv p3_0 = tcg_temp_new();
        gen_read_p3_0(p3_0);
        tcg_gen_concat_i32_i64(dest, p3_0, hex_gpr[reg_num + 1]);
        tcg_temp_free(p3_0);
    } else if (reg_num == HEX_REG_PC - 1) {
        TCGv pc = tcg_const_tl(ctx->base.pc_next);
        tcg_gen_concat_i32_i64(dest, hex_gpr[reg_num], pc);
        tcg_temp_free(pc);
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
        tcg_gen_addi_tl(hvx_cnt, hex_gpr[HEX_REG_QEMU_HVX_CNT],
                        ctx->num_hvx_insns);
        tcg_gen_concat_i32_i64(dest, hvx_cnt, hex_gpr[reg_num + 1]);
        tcg_temp_free(hvx_cnt);
    } else {
        tcg_gen_concat_i32_i64(dest,
            hex_gpr[reg_num],
            hex_gpr[reg_num + 1]);
    }
}

static void gen_write_p3_0(DisasContext *ctx, TCGv control_reg)
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
static void gen_write_ctrl_reg(DisasContext *ctx, int reg_num, TCGv val)
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

static void gen_write_ctrl_reg_pair(DisasContext *ctx, int reg_num,
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

static TCGv gen_get_half(TCGv result, int N, TCGv src, bool sign)
{
    if (sign) {
        tcg_gen_sextract_tl(result, src, N * 16, 16);
    } else {
        tcg_gen_extract_tl(result, src, N * 16, 16);
    }
    return result;
}

static void gen_set_half(int N, TCGv result, TCGv src)
{
    tcg_gen_deposit_tl(result, result, src, N * 16, 16);
}

static void gen_seti_half(int N, TCGv result, int val)
{
    TCGv tmp = tcg_const_tl(val);
    gen_set_half(N, result, tmp);
    tcg_temp_free(tmp);
}

static void gen_set_half_i64(int N, TCGv_i64 result, TCGv src)
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

static void gen_load_locked4u(TCGv dest, TCGv vaddr, int mem_index)
{
    tcg_gen_qemu_ld32u(dest, vaddr, mem_index);
    tcg_gen_mov_tl(hex_llsc_addr, vaddr);
    tcg_gen_mov_tl(hex_llsc_val, dest);
}

static void gen_load_locked8u(TCGv_i64 dest, TCGv vaddr, int mem_index)
{
    tcg_gen_qemu_ld64(dest, vaddr, mem_index);
    tcg_gen_mov_tl(hex_llsc_addr, vaddr);
    tcg_gen_mov_i64(hex_llsc_val_i64, dest);
}

static void gen_store_conditional4(DisasContext *ctx,
                                   TCGv pred, TCGv vaddr, TCGv src)
{
    TCGLabel *fail = gen_new_label();
    TCGLabel *done = gen_new_label();
    TCGv one, zero, tmp;

    tcg_gen_brcond_tl(TCG_COND_NE, vaddr, hex_llsc_addr, fail);

    one = tcg_const_tl(0xff);
    zero = tcg_const_tl(0);
    tmp = tcg_temp_new();
    tcg_gen_atomic_cmpxchg_tl(tmp, hex_llsc_addr, hex_llsc_val, src,
                              ctx->mem_idx, MO_32);
    tcg_gen_movcond_tl(TCG_COND_EQ, pred, tmp, hex_llsc_val,
                       one, zero);
    tcg_temp_free(one);
    tcg_temp_free(zero);
    tcg_temp_free(tmp);
    tcg_gen_br(done);

    gen_set_label(fail);
    tcg_gen_movi_tl(pred, 0);

    gen_set_label(done);
    tcg_gen_movi_tl(hex_llsc_addr, ~0);
}

static void gen_store_conditional8(DisasContext *ctx, TCGv pred,
                                   TCGv vaddr, TCGv_i64 src)
{
    TCGLabel *fail = gen_new_label();
    TCGLabel *done = gen_new_label();
    TCGv_i64 one, zero, tmp;

    tcg_gen_brcond_tl(TCG_COND_NE, vaddr, hex_llsc_addr, fail);

    one = tcg_const_i64(0xff);
    zero = tcg_const_i64(0);
    tmp = tcg_temp_new_i64();
    tcg_gen_atomic_cmpxchg_i64(tmp, hex_llsc_addr, hex_llsc_val_i64, src,
                               ctx->mem_idx, MO_64);
    tcg_gen_movcond_i64(TCG_COND_EQ, tmp, tmp, hex_llsc_val_i64,
                        one, zero);
    tcg_gen_extrl_i64_i32(pred, tmp);
    tcg_temp_free_i64(one);
    tcg_temp_free_i64(zero);
    tcg_temp_free_i64(tmp);
    tcg_gen_br(done);

    gen_set_label(fail);
    tcg_gen_movi_tl(pred, 0);

    gen_set_label(done);
    tcg_gen_movi_tl(hex_llsc_addr, ~0);
}

static void gen_store32(TCGv vaddr, TCGv src, int width, int slot)
{
    tcg_gen_mov_tl(hex_store_addr[slot], vaddr);
    tcg_gen_movi_tl(hex_store_width[slot], width);
    tcg_gen_mov_tl(hex_store_val32[slot], src);
}

static void gen_store1(TCGv_env cpu_env, TCGv vaddr, TCGv src, int slot)
{
    gen_store32(vaddr, src, 1, slot);
}

static void gen_store1i(TCGv_env cpu_env, TCGv vaddr, int32_t src, int slot)
{
    TCGv tmp = tcg_const_tl(src);
    gen_store1(cpu_env, vaddr, tmp, slot);
    tcg_temp_free(tmp);
}

static void gen_store2(TCGv_env cpu_env, TCGv vaddr, TCGv src, int slot)
{
    gen_store32(vaddr, src, 2, slot);
}

static void gen_store2i(TCGv_env cpu_env, TCGv vaddr, int32_t src, int slot)
{
    TCGv tmp = tcg_const_tl(src);
    gen_store2(cpu_env, vaddr, tmp, slot);
    tcg_temp_free(tmp);
}

static void gen_store4(TCGv_env cpu_env, TCGv vaddr, TCGv src, int slot)
{
    gen_store32(vaddr, src, 4, slot);
}

static void gen_store4i(TCGv_env cpu_env, TCGv vaddr, int32_t src, int slot)
{
    TCGv tmp = tcg_const_tl(src);
    gen_store4(cpu_env, vaddr, tmp, slot);
    tcg_temp_free(tmp);
}

static void gen_store8(TCGv_env cpu_env, TCGv vaddr, TCGv_i64 src, int slot)
{
    tcg_gen_mov_tl(hex_store_addr[slot], vaddr);
    tcg_gen_movi_tl(hex_store_width[slot], 8);
    tcg_gen_mov_i64(hex_store_val64[slot], src);
}

static void gen_store8i(TCGv_env cpu_env, TCGv vaddr, int64_t src, int slot)
{
    TCGv_i64 tmp = tcg_const_i64(src);
    gen_store8(cpu_env, vaddr, tmp, slot);
    tcg_temp_free_i64(tmp);
}

static TCGv gen_8bitsof(TCGv result, TCGv value)
{
    TCGv zero = tcg_const_tl(0);
    TCGv ones = tcg_const_tl(0xff);
    tcg_gen_movcond_tl(TCG_COND_NE, result, value, zero, ones, zero);
    tcg_temp_free(zero);
    tcg_temp_free(ones);

    return result;
}

static void gen_write_new_pc_addr(DisasContext *ctx, TCGv addr, TCGv pred)
{
    TCGLabel *pred_false = NULL;
    if (pred != NULL) {
        pred_false = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_EQ, pred, 0, pred_false);
    }

    if (ctx->pkt->pkt_has_multi_cof) {
        /* If there are multiple branches in a packet, ignore the second one */
        TCGv zero = tcg_const_tl(0);
        tcg_gen_movcond_tl(TCG_COND_NE, hex_gpr[HEX_REG_PC],
                           hex_branch_taken, zero,
                           hex_gpr[HEX_REG_PC], addr);
        tcg_gen_movi_tl(hex_branch_taken, 1);
        tcg_temp_free(zero);
    } else {
        tcg_gen_mov_tl(hex_gpr[HEX_REG_PC], addr);
    }

    if (pred != NULL) {
        gen_set_label(pred_false);
    }
}

static void gen_write_new_pc_pcrel(DisasContext *ctx, int pc_off, TCGv pred)
{
    target_ulong dest = ctx->pkt->pc + pc_off;
    if (ctx->pkt->pkt_has_multi_cof) {
        TCGv d = tcg_temp_local_new();
        tcg_gen_movi_tl(d, dest);
        gen_write_new_pc_addr(ctx, d, pred);
        tcg_temp_free(d);
    } else {
        /* Defer this jump to the end of the TB */
        g_assert(ctx->branch_cond == NULL);
        ctx->has_single_direct_branch = true;
        if (pred != NULL) {
            ctx->branch_cond = tcg_temp_local_new();
            tcg_gen_mov_tl(ctx->branch_cond, pred);
        }
        ctx->branch_dest = dest;
    }
}

static void gen_set_usr_field(int field, TCGv val)
{
    tcg_gen_deposit_tl(hex_new_value[HEX_REG_USR], hex_new_value[HEX_REG_USR],
                       val,
                       reg_field_info[field].offset,
                       reg_field_info[field].width);
}

static void gen_set_usr_fieldi(int field, int x)
{
    if (reg_field_info[field].width == 1) {
        target_ulong bit = 1 << reg_field_info[field].offset;
        if ((x & 1) == 1) {
            tcg_gen_ori_tl(hex_new_value[HEX_REG_USR],
                           hex_new_value[HEX_REG_USR],
                           bit);
        } else {
            tcg_gen_andi_tl(hex_new_value[HEX_REG_USR],
                            hex_new_value[HEX_REG_USR],
                            ~bit);
        }
    } else {
        TCGv val = tcg_const_tl(x);
        gen_set_usr_field(field, val);
        tcg_temp_free(val);
    }
}

static void gen_loop0r(DisasContext *ctx, TCGv RsV, int riV, Insn *insn)
{
    TCGv tmp = tcg_temp_new();
    fIMMEXT(riV);
    fPCALIGN(riV);
    tcg_gen_movi_tl(tmp, ctx->pkt->pc + riV);
    gen_log_reg_write(HEX_REG_LC0, RsV);
    gen_log_reg_write(HEX_REG_SA0, tmp);
    fSET_LPCFG(0);
    tcg_temp_free(tmp);
}

static void gen_loop0i(DisasContext *ctx, int count, int riV, Insn *insn)
{
    TCGv tmp = tcg_const_tl(count);
    gen_loop0r(ctx, tmp, riV, insn);
    tcg_temp_free(tmp);
}

static void gen_loop1r(DisasContext *ctx, TCGv RsV, int riV, Insn *insn)
{
    TCGv tmp = tcg_temp_new();
    fIMMEXT(riV);
    fPCALIGN(riV);
    tcg_gen_movi_tl(tmp, ctx->pkt->pc + riV);
    gen_log_reg_write(HEX_REG_LC1, RsV);
    gen_log_reg_write(HEX_REG_SA1, tmp);
    tcg_temp_free(tmp);
}

static void gen_loop1i(DisasContext *ctx, int count, int riV, Insn *insn)
{
    TCGv tmp = tcg_const_tl(count);
    gen_loop1r(ctx, tmp, riV, insn);
    tcg_temp_free(tmp);
}

static void gen_compare(TCGCond cond, TCGv res, TCGv arg1, TCGv arg2)
{
    TCGv one = tcg_const_tl(0xff);
    TCGv zero = tcg_const_tl(0);

    tcg_gen_movcond_tl(cond, res, arg1, arg2, one, zero);

    tcg_temp_free(one);
    tcg_temp_free(zero);
}

static void gen_comparei(TCGCond cond, TCGv res, TCGv arg1, int arg2)
{
    TCGv tmp = tcg_const_tl(arg2);
    gen_compare(cond, res, arg1, tmp);
    tcg_temp_free(tmp);
}

static void gen_compare_byte(TCGCond cond, TCGv res, TCGv arg1, TCGv arg2,
                             bool sign)
{
    TCGv byte1 = tcg_temp_new();
    TCGv byte2 = tcg_temp_new();
    gen_get_byte(byte1, 0, arg1, sign);
    gen_get_byte(byte2, 0, arg2, sign);
    gen_compare(cond, res, byte1, byte2);
    tcg_temp_free(byte1);
    tcg_temp_free(byte2);
}

static void gen_comparei_byte(TCGCond cond, TCGv res, TCGv arg1, bool sign,
                              int arg2)
{
    TCGv byte = tcg_temp_new();
    gen_get_byte(byte, 0, arg1, sign);
    gen_comparei(cond, res, byte, arg2);
    tcg_temp_free(byte);
}

static void gen_compare_half(TCGCond cond, TCGv res, TCGv arg1, TCGv arg2,
                             bool sign)
{
    TCGv half1 = tcg_temp_new();
    TCGv half2 = tcg_temp_new();
    gen_get_half(half1, 0, arg1, sign);
    gen_get_half(half2, 0, arg2, sign);
    gen_compare(cond, res, half1, half2);
    tcg_temp_free(half1);
    tcg_temp_free(half2);
}

static void gen_comparei_half(TCGCond cond, TCGv res, TCGv arg1, bool sign,
                              int arg2)
{
    TCGv half = tcg_temp_new();
    gen_get_half(half, 0, arg1, sign);
    gen_comparei(cond, res, half, arg2);
    tcg_temp_free(half);
}

static void gen_compare_i64(TCGCond cond, TCGv res,
                            TCGv_i64 arg1, TCGv_i64 arg2)
{
    TCGv_i64 one = tcg_const_i64(0xff);
    TCGv_i64 zero = tcg_const_i64(0);
    TCGv_i64 temp = tcg_temp_new_i64();

    tcg_gen_movcond_i64(cond, temp, arg1, arg2, one, zero);
    tcg_gen_extrl_i64_i32(res, temp);
    tcg_gen_andi_tl(res, res, 0xff);

    tcg_temp_free_i64(one);
    tcg_temp_free_i64(zero);
    tcg_temp_free_i64(temp);
}

static void gen_cond_jumpr(DisasContext *ctx, TCGv pred, TCGv dst_pc)
{
    gen_write_new_pc_addr(ctx, dst_pc, pred);
}

static void gen_cond_jump(DisasContext *ctx, TCGv pred, int pc_off)
{
    gen_write_new_pc_pcrel(ctx, pc_off, pred);
}

static void gen_cmpnd_cmp_jmp(DisasContext *ctx,
                              int pnum, TCGCond cond,
                              bool sense, TCGv arg1, TCGv arg2,
                              int pc_off)
{
    if (ctx->insn->part1) {
        TCGv pred = tcg_temp_new();
        gen_compare(cond, pred, arg1, arg2);
        gen_log_pred_write(ctx, pnum, pred);
        tcg_temp_free(pred);
    } else {
        TCGv pred = tcg_temp_new();

        tcg_gen_mov_tl(pred, hex_new_pred_value[pnum]);
        if (!sense) {
            tcg_gen_xori_tl(pred, pred, 0xff);
        }

        gen_cond_jump(ctx, pred, pc_off);

        tcg_temp_free(pred);
    }
}

static void gen_cmpnd_cmpi_jmp(DisasContext *ctx,
                               int pnum, TCGCond cond,
                               bool sense, TCGv arg1, int arg2,
                               int pc_off)
{
    TCGv tmp = tcg_const_tl(arg2);
    gen_cmpnd_cmp_jmp(ctx, pnum, cond, sense, arg1, tmp, pc_off);
    tcg_temp_free(tmp);

}

static void gen_cmpnd_cmp_n1_jmp(DisasContext *ctx, int pnum, TCGCond cond,
                                 bool sense, TCGv arg, int pc_off)
{
    gen_cmpnd_cmpi_jmp(ctx, pnum, cond, sense, arg, -1, pc_off);
}

static void gen_cmpnd_tstbit0_jmp(DisasContext *ctx,
                                  int pnum, bool sense, TCGv arg, int pc_off)
{
    if (ctx->insn->part1) {
        TCGv pred = tcg_temp_new();
        tcg_gen_andi_tl(pred, arg, 1);
        gen_8bitsof(pred, pred);
        gen_log_pred_write(ctx, pnum, pred);
        tcg_temp_free(pred);
    } else {
        TCGv pred = tcg_temp_new();
        tcg_gen_mov_tl(pred, hex_new_pred_value[pnum]);
        if (!sense) {
            tcg_gen_xori_tl(pred, pred, 0xff);
        }
        gen_cond_jump(ctx, pred, pc_off);
        tcg_temp_free(pred);
    }
}

static void gen_testbit0_jumpnv(DisasContext *ctx,
                                bool sense, TCGv arg, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_andi_tl(pred, arg, 1);
    if (!sense) {
        tcg_gen_xori_tl(pred, pred, 1);
    }
    gen_cond_jump(ctx, pred, pc_off);
    tcg_temp_free(pred);
}

static void gen_jump(DisasContext *ctx, int pc_off)
{
    gen_write_new_pc_pcrel(ctx, pc_off, NULL);
}

static void gen_jumpr(DisasContext *ctx, TCGv new_pc)
{
    gen_write_new_pc_addr(ctx, new_pc, NULL);
}

static void gen_call(DisasContext *ctx, int pc_off)
{
    TCGv next_PC =
        tcg_const_tl(ctx->pkt->pc + ctx->pkt->encod_pkt_size_in_bytes);
    gen_log_reg_write(HEX_REG_LR, next_PC);
    gen_write_new_pc_pcrel(ctx, pc_off, NULL);
    tcg_temp_free(next_PC);
}

static void gen_callr(DisasContext *ctx, TCGv new_pc)
{
    TCGv next_PC =
        tcg_const_tl(ctx->pkt->pc + ctx->pkt->encod_pkt_size_in_bytes);
    gen_log_reg_write(HEX_REG_LR, next_PC);
    gen_write_new_pc_addr(ctx, new_pc, NULL);
    tcg_temp_free(next_PC);
}

static void gen_cond_call(DisasContext *ctx, TCGv pred, bool sense, int pc_off)
{
    TCGv next_PC;
    TCGv lsb = tcg_temp_local_new();
    TCGLabel *skip = gen_new_label();
    tcg_gen_andi_tl(lsb, pred, 1);
    if (!sense) {
        tcg_gen_xori_tl(lsb, lsb, 1);
    }
    gen_write_new_pc_pcrel(ctx, pc_off, lsb);
    tcg_gen_brcondi_tl(TCG_COND_EQ, lsb, 0, skip);
    tcg_temp_free(lsb);
    next_PC = tcg_const_tl(ctx->pkt->pc + ctx->pkt->encod_pkt_size_in_bytes);
    gen_log_reg_write(HEX_REG_LR, next_PC);
    tcg_temp_free(next_PC);
    gen_set_label(skip);
}

static void gen_cond_callr(DisasContext *ctx,
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
    gen_callr(ctx, new_pc);
    gen_set_label(skip);
}

static void gen_endloop0(DisasContext *ctx)
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
     * If we're in a tight loop, we'll do this at the end of the TB to take
     * advantage of direct block chaining.
     */
    if (!ctx->is_tight_loop) {
        /*
         *    if (hex_gpr[HEX_REG_LC0] > 1) {
         *        PC = hex_gpr[HEX_REG_SA0];
         *        hex_new_value[HEX_REG_LC0] = hex_gpr[HEX_REG_LC0] - 1;
         *    }
         */
        TCGLabel *label3 = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC0], 1, label3);
        {
            gen_jumpr(ctx, hex_gpr[HEX_REG_SA0]);
            tcg_gen_subi_tl(hex_new_value[HEX_REG_LC0],
                            hex_gpr[HEX_REG_LC0], 1);
        }
        gen_set_label(label3);
    }

    tcg_temp_free(lpcfg);
}

static void gen_endloop1(DisasContext *ctx)
{
    /*
     *    if (hex_gpr[HEX_REG_LC1] > 1) {
     *        PC = hex_gpr[HEX_REG_SA1];
     *        hex_new_value[HEX_REG_LC1] = hex_gpr[HEX_REG_LC1] - 1;
     *    }
     */
    TCGLabel *label = gen_new_label();
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC1], 1, label);
    {
        gen_jumpr(ctx, hex_gpr[HEX_REG_SA1]);
        tcg_gen_subi_tl(hex_new_value[HEX_REG_LC1], hex_gpr[HEX_REG_LC1], 1);
    }
    gen_set_label(label);
}

static void gen_endloop01(DisasContext *ctx)
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
        gen_jumpr(ctx, hex_gpr[HEX_REG_SA0]);
        tcg_gen_subi_tl(hex_new_value[HEX_REG_LC0], hex_gpr[HEX_REG_LC0], 1);
        tcg_gen_br(done);
    }
    gen_set_label(label3);
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC1], 1, done);
    {
        gen_jumpr(ctx, hex_gpr[HEX_REG_SA1]);
        tcg_gen_subi_tl(hex_new_value[HEX_REG_LC1], hex_gpr[HEX_REG_LC1], 1);
    }
    gen_set_label(done);
    tcg_temp_free(lpcfg);
}

static void gen_ashiftr_4_4s(TCGv dst, TCGv src, int32_t shift_amt)
{
    tcg_gen_sari_tl(dst, src, shift_amt);
}

static void gen_ashiftl_4_4s(TCGv dst, TCGv src, int32_t shift_amt)
{
    if (shift_amt >= 32) {
        tcg_gen_movi_tl(dst, 0);
    } else {
        tcg_gen_shli_tl(dst, src, shift_amt);
    }
}

static void gen_cmp_jumpnv(DisasContext *ctx,
                           TCGCond cond, TCGv val, TCGv src, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_setcond_tl(cond, pred, val, src);
    gen_cond_jump(ctx, pred, pc_off);
    tcg_temp_free(pred);
}

static void gen_cmpi_jumpnv(DisasContext *ctx,
                            TCGCond cond, TCGv val, int src, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_setcondi_tl(cond, pred, val, src);
    gen_cond_jump(ctx, pred, pc_off);
    tcg_temp_free(pred);
}

static void gen_asl_r_r_or(TCGv RxV, TCGv RsV, TCGv RtV)
{
    TCGv zero = tcg_const_tl(0);
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

    tcg_temp_free(zero);
    tcg_temp_free(shift_amt);
    tcg_temp_free_i64(shift_amt_i64);
    tcg_temp_free_i64(shift_left_val_i64);
    tcg_temp_free(shift_left_val);
    tcg_temp_free_i64(shift_right_val_i64);
    tcg_temp_free(shift_right_val);
    tcg_temp_free(or_val);
}

static void gen_lshiftr_4_4u(TCGv dst, TCGv src, int32_t shift_amt)
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
        TCGv tcg_min = tcg_const_tl(min);
        TCGv tcg_max = tcg_const_tl(max);
        tcg_gen_movcond_tl(TCG_COND_LT, dst, src, tcg_min, tcg_min, tcg_max);
        tcg_temp_free(tcg_min);
        tcg_temp_free(tcg_max);
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
        TCGv_i64 min = tcg_const_i64(-(1LL << (bits - 1)));
        TCGv_i64 max = tcg_const_i64((1LL << (bits - 1)) - 1);
        TCGv_i64 zero = tcg_const_i64(0);
        tcg_gen_movcond_i64(TCG_COND_LT, dst, src, zero, min, max);
        gen_set_usr_fieldi(USR_OVF, 1);
        tcg_temp_free_i64(min);
        tcg_temp_free_i64(max);
        tcg_temp_free_i64(zero);
    }
    gen_set_label(label);
}

static void gen_satval(TCGv_i64 dest, TCGv_i64 source, uint32_t bits)
{
    TCGv_i64 min = tcg_const_i64(-(1LL << (bits - 1)));
    TCGv_i64 max = tcg_const_i64((1LL << (bits - 1)) - 1);
    TCGv_i64 zero = tcg_const_i64(0);

    gen_set_usr_fieldi(USR_OVF, 1);
    tcg_gen_movcond_i64(TCG_COND_LT, dest, source, zero,
                        min, max);

    tcg_temp_free_i64(min);
    tcg_temp_free_i64(max);
    tcg_temp_free_i64(zero);
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

static void gen_sar(TCGv RdV, TCGv RsV, TCGv shift_amt)
{
    /*
     * if (shift_amt < 32) {
     *     RdV = sar(RsV, shift_amt);
     * } else {
     *     if (RsV > 0) {
     *         RdV = 0;
     *     } else {
     *         RdV = ~0;
     *     }
     * }
     */
    TCGLabel *shift_ge_32 = gen_new_label();
    TCGLabel *done = gen_new_label();

    tcg_gen_brcondi_tl(TCG_COND_GE, shift_amt, 32, shift_ge_32);
    tcg_gen_sar_tl(RdV, RsV, shift_amt);
    tcg_gen_br(done);

    gen_set_label(shift_ge_32);
    TCGv zero = tcg_const_tl(0);
    TCGv ones = tcg_const_tl(~0);
    tcg_gen_movcond_tl(TCG_COND_LT, RdV, RsV, zero,
                       ones, zero);
    tcg_temp_free(zero);
    tcg_temp_free(ones);

    gen_set_label(done);
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
    gen_sar(RdV, RsV, shift_amt);

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
    gen_sar(RdV, RsV, shift_amt);
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
                               VRegWriteType type)
{
    intptr_t dstoff;

    if (type != EXT_TMP) {
        dstoff = ctx_future_vreg_off(ctx, num, 1, true);
        tcg_gen_gvec_mov(MO_64, dstoff, srcoff,
                         sizeof(MMVector), sizeof(MMVector));
    } else {
        dstoff = ctx_tmp_vreg_off(ctx, num, 1, false);
        tcg_gen_gvec_mov(MO_64, dstoff, srcoff,
                         sizeof(MMVector), sizeof(MMVector));
    }
}

static void gen_log_vreg_write_pair(DisasContext *ctx, intptr_t srcoff, int num,
                                    VRegWriteType type)
{
    gen_log_vreg_write(ctx, srcoff, num ^ 0, type);
    srcoff += sizeof(MMVector);
    gen_log_vreg_write(ctx, srcoff, num ^ 1, type);
}

static void gen_log_qreg_write(intptr_t srcoff, int num, int vnew,
                               int slot_num)
{
    intptr_t dstoff;

    dstoff = offsetof(CPUHexagonState, future_QRegs[num]);
    tcg_gen_gvec_mov(MO_64, dstoff, srcoff, sizeof(MMQReg), sizeof(MMQReg));
}

static void gen_vreg_load(DisasContext *ctx, intptr_t dstoff, TCGv src,
                          bool aligned)
{
    TCGv_i64 tmp = tcg_temp_new_i64();
    if (aligned) {
        tcg_gen_andi_tl(src, src, ~((int32_t)sizeof(MMVector) - 1));
    }
    for (int i = 0; i < sizeof(MMVector) / 8; i++) {
        tcg_gen_qemu_ld64(tmp, src, ctx->mem_idx);
        tcg_gen_addi_tl(src, src, 8);
        tcg_gen_st_i64(tmp, cpu_env, dstoff + i * 8);
    }
    tcg_temp_free_i64(tmp);
}

static void gen_vreg_store(DisasContext *ctx, TCGv EA, intptr_t srcoff,
                           int slot, bool aligned)
{
    intptr_t dstoff = offsetof(CPUHexagonState, vstore[slot].data);
    intptr_t maskoff = offsetof(CPUHexagonState, vstore[slot].mask);

    if (is_gather_store_insn(ctx)) {
        TCGv sl = tcg_const_tl(slot);
        gen_helper_gather_store(cpu_env, EA, sl);
        tcg_temp_free(sl);
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

static void vec_to_qvec(size_t size, intptr_t dstoff, intptr_t srcoff)
{
    TCGv_i64 tmp = tcg_temp_new_i64();
    TCGv_i64 word = tcg_temp_new_i64();
    TCGv_i64 bits = tcg_temp_new_i64();
    TCGv_i64 mask = tcg_temp_new_i64();
    TCGv_i64 zero = tcg_const_i64(0);
    TCGv_i64 ones = tcg_const_i64(~0);

    for (int i = 0; i < sizeof(MMVector) / 8; i++) {
        tcg_gen_ld_i64(tmp, cpu_env, srcoff + i * 8);
        tcg_gen_movi_i64(mask, 0);

        for (int j = 0; j < 8; j += size) {
            tcg_gen_extract_i64(word, tmp, j * 8, size * 8);
            tcg_gen_movcond_i64(TCG_COND_NE, bits, word, zero, ones, zero);
            tcg_gen_deposit_i64(mask, mask, bits, j, size);
        }

        tcg_gen_st8_i64(mask, cpu_env, dstoff + i);
    }
    tcg_temp_free_i64(tmp);
    tcg_temp_free_i64(word);
    tcg_temp_free_i64(bits);
    tcg_temp_free_i64(mask);
    tcg_temp_free_i64(zero);
    tcg_temp_free_i64(ones);
}

static void gen_vrmpyub(intptr_t dst_off, intptr_t VuV_off, TCGv RtV, bool acc)
{
    /*
     * fVFOREACH(32, i) {
     *     dst.uw[i] += fMPY8UU(fGETUBYTE(0,VuV.uw[i]), fGETUBYTE(0,RtV));
     *     dst.uw[i] += fMPY8UU(fGETUBYTE(1,VuV.uw[i]), fGETUBYTE(1,RtV));
     *     dst.uw[i] += fMPY8UU(fGETUBYTE(2,VuV.uw[i]), fGETUBYTE(2,RtV));
     *     dst.uw[i] += fMPY8UU(fGETUBYTE(3,VuV.uw[i]), fGETUBYTE(3,RtV));
     * }
     */
    TCGv RtV_bytes[4];
    TCGv VuV_word = tcg_temp_new();
    TCGv VuV_byte = tcg_temp_new();
    TCGv sum = tcg_temp_new();
    TCGv prod = tcg_temp_new();
    int i;

    for (i = 0; i < 4; i++) {
        RtV_bytes[i] = tcg_temp_new();
        gen_get_byte(RtV_bytes[i], i, RtV, false);
    }

    for (i = 0; i < sizeof(MMVector) / 4; i++) {
        if (acc) {
            /* Load sum from dst.uw[i] */
            tcg_gen_ld_tl(sum, cpu_env, dst_off);
        } else {
            tcg_gen_movi_tl(sum, 0);
        }

        /* Load from VuV.uw[i] */
        tcg_gen_ld_tl(VuV_word, cpu_env, VuV_off);

        for (int j = 0; j < 4; j++) {
            gen_get_byte(VuV_byte, j, VuV_word, false);
            tcg_gen_mul_tl(prod, VuV_byte, RtV_bytes[j]);
            tcg_gen_add_tl(sum, sum, prod);
        }

        /* Store sum to dst.uw[i] */
        tcg_gen_st_tl(sum, cpu_env, dst_off);

        dst_off += 4;
        VuV_off += 4;
    }

    for (i = 0; i < 4; i++) {
        tcg_temp_free(RtV_bytes[i]);
    }
    tcg_temp_free(VuV_word);
    tcg_temp_free(VuV_byte);
    tcg_temp_free(sum);
    tcg_temp_free(prod);
}

static void gen_vrmpyubv(intptr_t dst_off, intptr_t VuV_off, intptr_t VvV_off,
                         bool acc)
{
    /*
     *    fVFOREACH(32, i) {
     *        dst.uw[i] += fMPY8UU(fGETUBYTE(0,VuV.uw[i]),
     *                             fGETUBYTE(0,VvV.uw[i]));
     *        dst.uw[i] += fMPY8UU(fGETUBYTE(1,VuV.uw[i]),
     *                             fGETUBYTE(1,VvV.uw[i]));
     *        dst.uw[i] += fMPY8UU(fGETUBYTE(2,VuV.uw[i]),
     *                             fGETUBYTE(2,VvV.uw[i]));
     *        dst.uw[i] += fMPY8UU(fGETUBYTE(3,VuV.uw[i]),
     *                             fGETUBYTE(3,VvV.uw[i])) ;
     *    }
     */
    TCGv VuV_word = tcg_temp_new();
    TCGv VvV_word = tcg_temp_new();
    TCGv VuV_byte = tcg_temp_new();
    TCGv VvV_byte = tcg_temp_new();
    TCGv sum = tcg_temp_new();
    TCGv prod = tcg_temp_new();
    int i;

    for (i = 0; i < sizeof(MMVector) / 4; i++) {
        if (acc) {
            /* Load sum from dst.uw[i] */
            tcg_gen_ld_tl(sum, cpu_env, dst_off);
        } else {
            tcg_gen_movi_tl(sum, 0);
        }

        /* Load from VuV.uw[i] */
        tcg_gen_ld_tl(VuV_word, cpu_env, VuV_off);
        /* Load from VvV.uw[i] */
        tcg_gen_ld_tl(VvV_word, cpu_env, VvV_off);

        for (int j = 0; j < 4; j++) {
            gen_get_byte(VuV_byte, j, VuV_word, false);
            gen_get_byte(VvV_byte, j, VvV_word, false);
            tcg_gen_mul_tl(prod, VuV_byte, VvV_byte);
            tcg_gen_add_tl(sum, sum, prod);
        }

        /* Store sum to dst.uw[i] */
        tcg_gen_st_tl(sum, cpu_env, dst_off);

        dst_off += 4;
        VuV_off += 4;
        VvV_off += 4;
    }

    tcg_temp_free(VuV_word);
    tcg_temp_free(VvV_word);
    tcg_temp_free(VuV_byte);
    tcg_temp_free(VvV_byte);
    tcg_temp_free(sum);
    tcg_temp_free(prod);
}

static void gen_vrmpyubi(intptr_t dst_off, intptr_t VuuV_off, TCGv RtV,
                         int uiV, bool acc)
{
    /*
     *    fVFOREACH(32, i) {
     *        dst.v[0].uw[i] += fMPY8UU(fGETUBYTE(0, VuuV.v[uiV ? 1:0].uw[i]),
     *                                  fGETUBYTE((0-uiV) & 0x3,RtV));
     *        dst.v[0].uw[i] += fMPY8UU(fGETUBYTE(1, VuuV.v[0        ].uw[i]),
     *                                  fGETUBYTE((1-uiV) & 0x3,RtV));
     *        dst.v[0].uw[i] += fMPY8UU(fGETUBYTE(2, VuuV.v[0        ].uw[i]),
     *                                  fGETUBYTE((2-uiV) & 0x3,RtV));
     *        dst.v[0].uw[i] += fMPY8UU(fGETUBYTE(3, VuuV.v[0        ].uw[i]),
     *                                  fGETUBYTE((3-uiV) & 0x3,RtV));
     *
     *        dst.v[1].uw[i] += fMPY8UU(fGETUBYTE(0, VuuV.v[1        ].uw[i]),
     *                                  fGETUBYTE((2-uiV) & 0x3,RtV));
     *        dst.v[1].uw[i] += fMPY8UU(fGETUBYTE(1, VuuV.v[1        ].uw[i]),
     *                                  fGETUBYTE((3-uiV) & 0x3,RtV));
     *        dst.v[1].uw[i] += fMPY8UU(fGETUBYTE(2, VuuV.v[uiV ? 1:0].uw[i]),
     *                                  fGETUBYTE((0-uiV) & 0x3,RtV));
     *        dst.v[1].uw[i] += fMPY8UU(fGETUBYTE(3, VuuV.v[0        ].uw[i]),
     *                                  fGETUBYTE((1-uiV) & 0x3,RtV)) ;
     *    }
     */
    TCGv RtV_bytes[8];
    int VuuV_vectors[8];
    TCGv VuuV_word = tcg_temp_new();
    TCGv VuuV_byte = tcg_temp_new();
    TCGv sum = tcg_temp_new();
    TCGv prod = tcg_temp_new();
    int i;

    for (i = 0; i < 8; i++) {
        RtV_bytes[i] = tcg_temp_new();
    }
    gen_get_byte(RtV_bytes[0], (0 - uiV) & 0x3, RtV, false);
    gen_get_byte(RtV_bytes[1], (1 - uiV) & 0x3, RtV, false);
    gen_get_byte(RtV_bytes[2], (2 - uiV) & 0x3, RtV, false);
    gen_get_byte(RtV_bytes[3], (3 - uiV) & 0x3, RtV, false);
    gen_get_byte(RtV_bytes[4], (2 - uiV) & 0x3, RtV, false);
    gen_get_byte(RtV_bytes[5], (3 - uiV) & 0x3, RtV, false);
    gen_get_byte(RtV_bytes[6], (0 - uiV) & 0x3, RtV, false);
    gen_get_byte(RtV_bytes[7], (1 - uiV) & 0x3, RtV, false);

    VuuV_vectors[0] = uiV ? 1 : 0;
    VuuV_vectors[1] = 0;
    VuuV_vectors[2] = 0;
    VuuV_vectors[3] = 0;
    VuuV_vectors[4] = 1;
    VuuV_vectors[5] = 1;
    VuuV_vectors[6] = uiV ? 1 : 0;
    VuuV_vectors[7] = 0;

    for (i = 0; i < sizeof(MMVector) / 4; i++) {
        if (acc) {
            /* Load sum from dst.v[0].uw[i] */
            tcg_gen_ld_tl(sum, cpu_env, dst_off);
        } else {
            tcg_gen_movi_tl(sum, 0);
        }

        for (int j = 0; j < 4; j++) {
            /* Load from VuuV.v[?].uw[i] */
            tcg_gen_ld_tl(VuuV_word, cpu_env,
                          VuuV_off + VuuV_vectors[j] * sizeof(MMVector));
            gen_get_byte(VuuV_byte, j, VuuV_word, false);
            tcg_gen_mul_tl(prod, VuuV_byte, RtV_bytes[j]);
            tcg_gen_add_tl(sum, sum, prod);
        }

        /* Store sum to dst.v[0].uw[i] */
        tcg_gen_st_tl(sum, cpu_env, dst_off);

        if (acc) {
            /* Load sum from dst.v[1].uw[i] */
            tcg_gen_ld_tl(sum, cpu_env, dst_off + sizeof(MMVector));
        } else {
            tcg_gen_movi_tl(sum, 0);
        }

        for (int j = 0; j < 4; j++) {
            /* Load from VuuV.v[?].uw[i] */
            tcg_gen_ld_tl(VuuV_word, cpu_env,
                          VuuV_off + VuuV_vectors[j + 4] * sizeof(MMVector));
            gen_get_byte(VuuV_byte, j, VuuV_word, false);
            tcg_gen_mul_tl(prod, VuuV_byte, RtV_bytes[j + 4]);
            tcg_gen_add_tl(sum, sum, prod);
        }

        /* Store sum to dst.v[1].uw[i] */
        tcg_gen_st_tl(sum, cpu_env, dst_off + sizeof(MMVector));

        dst_off += 4;
        VuuV_off += 4;
    }

    for (i = 0; i < 8; i++) {
        tcg_temp_free(RtV_bytes[i]);
    }
    tcg_temp_free(VuuV_word);
    tcg_temp_free(VuuV_byte);
    tcg_temp_free(sum);
    tcg_temp_free(prod);
}

static void gen_vmpyewuh(intptr_t VdV_off, intptr_t VuV_off, intptr_t VvV_off)
{
    /*
     *    fVFOREACH(32, i) {
     *        VdV.w[i] = fMPY3216SU(VuV.w[i], fGETUHALF(0, VvV.w[i])) >> 16;
     *    }
     */
    int i;
    TCGv_i64 VuV_word = tcg_temp_new_i64();
    TCGv_i64 VvV_half = tcg_temp_new_i64();
    TCGv_i64 prod = tcg_temp_new_i64();

    for (i = 0; i < sizeof(MMVector) / 4; i++) {
        tcg_gen_ld32s_i64(VuV_word, cpu_env, VuV_off);
        tcg_gen_ld16u_i64(VvV_half, cpu_env, VvV_off);
        tcg_gen_mul_i64(prod, VuV_word, VvV_half);
        tcg_gen_shri_i64(prod, prod, 16);
        tcg_gen_st32_i64(prod, cpu_env, VdV_off);

        VdV_off += 4;
        VuV_off += 4;
        VvV_off += 4;
    }
    tcg_temp_free_i64(VuV_word);
    tcg_temp_free_i64(VvV_half);
    tcg_temp_free_i64(prod);
}

static void probe_noshuf_load(TCGv va, int s, int mi)
{
    TCGv size = tcg_const_tl(s);
    TCGv mem_idx = tcg_const_tl(mi);
    gen_helper_probe_noshuf_load(cpu_env, va, size, mem_idx);
    tcg_temp_free(size);
    tcg_temp_free(mem_idx);
}

#include "tcg_funcs_generated.c.inc"
#include "tcg_func_table_generated.c.inc"
