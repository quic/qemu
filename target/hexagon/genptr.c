/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#ifndef CONFIG_USER_ONLY
#include "gen_tcg_sys.h"
#endif
#include "reg_fields.h"
#include "genptr.h"

#define IMMUTABLE (~0)

static void gen_check_reg_write(DisasContext *ctx, int rnum)
{
    if (rnum < NUM_GPREGS && test_bit(rnum, ctx->wreg_mult_gprs)) {
        TCGv cancelled = tcg_temp_new();
        TCGv mult_reg = tcg_temp_new();
        TCGLabel *skip = gen_new_label();

        tcg_gen_andi_tl(cancelled, hex_slot_cancelled, 1 << ctx->insn->slot);
        tcg_gen_brcondi_tl(TCG_COND_NE, cancelled, 0, skip);
        tcg_gen_andi_tl(mult_reg, hex_gpreg_written, 1 << rnum);
        tcg_gen_or_tl(hex_mult_reg_written, hex_mult_reg_written, mult_reg);
        tcg_gen_ori_tl(hex_gpreg_written, hex_gpreg_written, 1 << rnum);
        gen_set_label(skip);
    }
}

static void gen_check_reg_write_pair(DisasContext *ctx, int rnum)
{
    gen_check_reg_write(ctx, rnum);
    gen_check_reg_write(ctx, rnum + 1);
}

TCGv gen_read_reg(TCGv result, int num)
{
    tcg_gen_mov_tl(result, hex_gpr[num]);
    return result;
}

TCGv gen_read_preg(TCGv pred, uint8_t num)
{
    tcg_gen_mov_tl(pred, hex_pred[num]);
    return pred;
}

static inline void gen_masked_reg_write(TCGv new_val, TCGv cur_val,
                                        target_ulong reg_mask)
{
    if (reg_mask) {
        TCGv tmp = tcg_temp_new();

        /* new_val = (new_val & ~reg_mask) | (cur_val & reg_mask) */
        tcg_gen_andi_tl(new_val, new_val, ~reg_mask);
        tcg_gen_andi_tl(tmp, cur_val, reg_mask);
        tcg_gen_or_tl(new_val, new_val, tmp);
    }
}

static const target_ulong reg_immut_masks[TOTAL_PER_THREAD_REGS] = {
    [HEX_REG_USR] = 0xc13000c0,
    [HEX_REG_PC] = IMMUTABLE,
    [HEX_REG_GP] = 0x3f,
    [HEX_REG_UPCYCLELO] = IMMUTABLE,
    [HEX_REG_UPCYCLEHI] = IMMUTABLE,
    [HEX_REG_UTIMERLO] = IMMUTABLE,
    [HEX_REG_UTIMERHI] = IMMUTABLE,
};

static TCGv get_result_gpr(DisasContext *ctx, int rnum)
{
    return hex_new_value[rnum];
}

static TCGv_i64 get_result_gpr_pair(DisasContext *ctx, int rnum)
{
    TCGv_i64 result = tcg_temp_new_i64();
    tcg_gen_concat_i32_i64(result, hex_new_value[rnum],
                                   hex_new_value[rnum + 1]);
    return result;
}

#ifndef CONFIG_USER_ONLY
static inline void gen_log_greg_write(int rnum, TCGv val)
{
    tcg_gen_mov_tl(hex_greg_new_value[rnum], val);
    if (HEX_DEBUG) {
        /* Do this so HELPER(debug_commit_end) will know */
        tcg_gen_movi_tl(hex_greg_written[rnum], 1);
    }
}
#endif

void gen_log_reg_write(int rnum, TCGv val)
{
    const target_ulong reg_mask = reg_immut_masks[rnum];

    if (reg_mask != IMMUTABLE) {
        gen_masked_reg_write(val, hex_gpr[rnum], reg_mask);
        tcg_gen_mov_tl(hex_new_value[rnum], val);
        if (HEX_DEBUG) {
            /* Do this so HELPER(debug_commit_end) will know */
            tcg_gen_movi_tl(hex_reg_written[rnum], 1);
        }
    }
}

static void gen_log_reg_write_pair(int rnum, TCGv_i64 val)
{
    const target_ulong reg_mask_low = reg_immut_masks[rnum];
    const target_ulong reg_mask_high = reg_immut_masks[rnum + 1];
    TCGv val32 = tcg_temp_new();

    if (reg_mask_low != IMMUTABLE) {
        /* Low word */
        tcg_gen_extrl_i64_i32(val32, val);
        gen_masked_reg_write(val32, hex_gpr[rnum], reg_mask_low);
        tcg_gen_mov_tl(hex_new_value[rnum], val32);
        if (HEX_DEBUG) {
            /* Do this so HELPER(debug_commit_end) will know */
            tcg_gen_movi_tl(hex_reg_written[rnum], 1);
        }
    }

    if (reg_mask_high != IMMUTABLE) {
        /* High word */
        tcg_gen_extrh_i64_i32(val32, val);
        gen_masked_reg_write(val32, hex_gpr[rnum + 1], reg_mask_high);
        tcg_gen_mov_tl(hex_new_value[rnum + 1], val32);
        if (HEX_DEBUG) {
            /* Do this so HELPER(debug_commit_end) will know */
            tcg_gen_movi_tl(hex_reg_written[rnum + 1], 1);
        }
    }
}

#ifndef CONFIG_USER_ONLY
static bool greg_writable(int rnum, bool pair)
{
    if (pair) {
        if (rnum < HEX_GREG_G3) {
            return true;
        }
        qemu_log_mask(LOG_UNIMP,
                "Warning: ignoring write to guest register pair G%d:%d\n",
                rnum + 1, rnum);
    } else {
        if (rnum <= HEX_GREG_G3) {
            return true;
        }
        qemu_log_mask(LOG_UNIMP,
                "Warning: ignoring write to guest register G%d\n", rnum);
    }
    return false;
}

static void check_greg_impl(int rnum, bool pair)
{
    if (pair && (!greg_implemented(rnum) || !greg_implemented(rnum + 1))) {
        qemu_log_mask(LOG_UNIMP,
                "Warning: guest register pair G%d:%d is unimplemented or "
                "reserved. Read will yield 0.\n",
                rnum + 1, rnum);
    } else if (!pair && !greg_implemented(rnum)) {
        qemu_log_mask(LOG_UNIMP,
                "Warning: guest register G%d is unimplemented or reserved."
                " Read will yield 0.\n", rnum);
    }
}

static void gen_log_greg_write_pair(int rnum, TCGv_i64 val)
{
    TCGv val32 = tcg_temp_new();

    /* Low word */
    tcg_gen_extrl_i64_i32(val32, val);
    tcg_gen_mov_tl(hex_greg_new_value[rnum], val32);
    /* High word */
    tcg_gen_extrh_i64_i32(val32, val);
    tcg_gen_mov_tl(hex_greg_new_value[rnum + 1], val32);
}

static const target_ulong sreg_immut_masks[NUM_SREGS] = {
    [HEX_SREG_STID] = 0xff00ff00,
    [HEX_SREG_ELR] = 0x00000003,
    [HEX_SREG_SSR] = 0x00008000,
    [HEX_SREG_CCR] = 0x10e0ff24,
    [HEX_SREG_HTID] = IMMUTABLE,
    [HEX_SREG_IMASK] = 0xffff0000,
    [HEX_SREG_GEVB] = 0x000000ff,
    [HEX_SREG_EVB] = 0x000000ff,
    [HEX_SREG_MODECTL] = IMMUTABLE,
    [HEX_SREG_SYSCFG] = 0x80001c00,
    [HEX_SREG_IPENDAD] = IMMUTABLE,
    [HEX_SREG_VID] = 0xfc00fc00,
    [HEX_SREG_VID1] = 0xfc00fc00,
    [HEX_SREG_BESTWAIT] = 0xfffffe00,
    [HEX_SREG_SCHEDCFG] = 0xfffffef0,
    [HEX_SREG_CFGBASE] = IMMUTABLE,
    [HEX_SREG_REV] = IMMUTABLE,
    [HEX_SREG_ISDBST] = IMMUTABLE,
    [HEX_SREG_ISDBCFG0] = 0xe0000000,
    [HEX_SREG_BRKPTPC0] = 0x00000003,
    [HEX_SREG_BRKPTCFG0] = 0xfc007000,
    [HEX_SREG_BRKPTPC1] = 0x00000003,
    [HEX_SREG_BRKPTCFG1] = 0xfc007000,
    [HEX_SREG_ISDBMBXIN] = IMMUTABLE,
    [HEX_SREG_ISDBEN] = 0xfffffffe,
    [HEX_SREG_TIMERLO] = IMMUTABLE,
    [HEX_SREG_TIMERHI] = IMMUTABLE,
};

static void gen_log_sreg_write(int rnum, TCGv val)
{
    const target_ulong reg_mask = sreg_immut_masks[rnum];

    if (reg_mask != IMMUTABLE) {
        if (rnum < HEX_SREG_GLB_START) {
            gen_masked_reg_write(val, hex_t_sreg[rnum], reg_mask);
            tcg_gen_mov_tl(hex_t_sreg_new_value[rnum], val);
        } else {
            gen_masked_reg_write(val, hex_g_sreg[rnum], reg_mask);
            gen_helper_sreg_write(cpu_env, tcg_constant_i32(rnum), val);
        }
    }
}

static void gen_log_sreg_write_pair(int rnum, TCGv_i64 val)
{
    const target_ulong reg_mask_low = sreg_immut_masks[rnum];
    const target_ulong reg_mask_high = sreg_immut_masks[rnum + 1];

    TCGv val32 = tcg_temp_new();

    if (rnum < HEX_SREG_GLB_START) {
        if (reg_mask_low != IMMUTABLE) {
            tcg_gen_extrl_i64_i32(val32, val);
            gen_masked_reg_write(val32, hex_t_sreg[rnum], reg_mask_low);
            tcg_gen_mov_tl(hex_t_sreg_new_value[rnum], val32);
        }
        if (reg_mask_high != IMMUTABLE) {
            tcg_gen_extrh_i64_i32(val32, val);
            gen_masked_reg_write(val32, hex_t_sreg[rnum + 1], reg_mask_high);
            tcg_gen_mov_tl(hex_t_sreg_new_value[rnum + 1], val32);
        }
    } else {
        if (reg_mask_low != IMMUTABLE) {
            tcg_gen_extrl_i64_i32(val32, val);
            gen_masked_reg_write(val32, hex_g_sreg[rnum], reg_mask_low);
            gen_helper_sreg_write(cpu_env, tcg_constant_i32(rnum), val32);
        }
        if (reg_mask_high != IMMUTABLE) {
            tcg_gen_extrh_i64_i32(val32, val);
            gen_masked_reg_write(val32, hex_g_sreg[rnum + 1], reg_mask_high);
            gen_helper_sreg_write(cpu_env, tcg_constant_i32(rnum + 1), val32);
        }
    }
}
#endif


void gen_log_pred_write(DisasContext *ctx, int pnum, TCGv val)
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
    set_bit(pnum, ctx->pregs_written);
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
 * In system mode, they can only be read when SSR[CE] is set.
 * In user mode, SSR[CE] is not modeled. We pretend it is always set
 * to allow upcycle access.
 */
static inline void gen_read_upcycle_reg(TCGv dest, int regnum, TCGv zero)
{
#ifdef CONFIG_USER_ONLY
    TCGv counter = dest;
#else
    TCGv ssr_ce = tcg_temp_new();
    TCGv counter = tcg_temp_new();
#endif

    if (regnum == HEX_REG_UPCYCLEHI) {
        gen_helper_read_pcyclehi(counter, cpu_env);
    } else if (regnum == HEX_REG_UPCYCLELO) {
        gen_helper_read_pcyclelo(counter, cpu_env);
    } else {
        g_assert_not_reached();
    }

#ifndef CONFIG_USER_ONLY
    GET_SSR_FIELD(ssr_ce, SSR_CE);
    /* dest = (ssr_ce != 0 ? counter : 0) */
    tcg_gen_movcond_tl(TCG_COND_NE, dest, ssr_ce, zero, counter, zero);
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
 *     HEX_REG_P3_0_ALIASED  aliased to the predicate registers
 *                           -> concat the 4 predicate registers together
 *     HEX_REG_PC            actual value stored in DisasContext
 *                           -> assign from ctx->base.pc_next
 *     HEX_REG_QEMU_*_CNT    changes in current TB in DisasContext
 *                           -> add current TB changes to existing reg value
 */
static inline void gen_read_ctrl_reg(DisasContext *ctx, const int reg_num,
                                     TCGv dest)
{
    if (reg_num == HEX_REG_P3_0_ALIASED) {
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
                        ctx->num_coproc_insns);
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
    if (reg_num == HEX_REG_P3_0_ALIASED) {
        TCGv p3_0 = tcg_temp_new();
        gen_read_p3_0(p3_0);
        tcg_gen_concat_i32_i64(dest, p3_0, hex_gpr[reg_num + 1]);
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
    } else if (reg_num == HEX_REG_QEMU_HVX_CNT) {
        TCGv hvx_cnt = tcg_temp_new();
        TCGv coproc_cnt = tcg_temp_new();
        tcg_gen_addi_tl(hvx_cnt, hex_gpr[HEX_REG_QEMU_HVX_CNT],
                        ctx->num_hvx_insns);
        tcg_gen_addi_tl(coproc_cnt, hex_gpr[HEX_REG_QEMU_HMX_CNT],
                        ctx->num_coproc_insns);
        tcg_gen_concat_i32_i64(dest, hvx_cnt, coproc_cnt);
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
    }
}

/*
 * Certain control registers require special handling on write
 *     HEX_REG_P3_0_ALIASED  aliased to the predicate registers
 *                           -> break the value across 4 predicate registers
 *     HEX_REG_QEMU_*_CNT    changes in current TB in DisasContext
 *                            -> clear the changes
 */
static inline void gen_write_ctrl_reg(DisasContext *ctx, int reg_num,
                                      TCGv val)
{
    if (reg_num == HEX_REG_P3_0_ALIASED) {
        gen_write_p3_0(ctx, val);
    } else {
        gen_log_reg_write(reg_num, val);
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
    if (reg_num == HEX_REG_P3_0_ALIASED) {
        TCGv result = get_result_gpr(ctx, reg_num + 1);
        TCGv val32 = tcg_temp_new();
        tcg_gen_extrl_i64_i32(val32, val);
        gen_write_p3_0(ctx, val32);
        tcg_gen_extrh_i64_i32(val32, val);
        tcg_gen_mov_tl(result, val32);
    } else {
        gen_log_reg_write_pair(reg_num, val);
        if (reg_num == HEX_REG_QEMU_PKT_CNT) {
            ctx->num_packets = 0;
            ctx->num_insns = 0;
        }
        if (reg_num == HEX_REG_QEMU_HVX_CNT) {
            ctx->num_hvx_insns = 0;
        }
    }
}

TCGv gen_get_byte(TCGv result, int N, TCGv src, bool sign)
{
    if (sign) {
        tcg_gen_sextract_tl(result, src, N * 8, 8);
    } else {
        tcg_gen_extract_tl(result, src, N * 8, 8);
    }
    return result;
}

TCGv gen_get_byte_i64(TCGv result, int N, TCGv_i64 src, bool sign)
{
    TCGv_i64 res64 = tcg_temp_new_i64();
    if (sign) {
        tcg_gen_sextract_i64(res64, src, N * 8, 8);
    } else {
        tcg_gen_extract_i64(res64, src, N * 8, 8);
    }
    tcg_gen_extrl_i64_i32(result, res64);

    return result;
}

TCGv gen_get_half(TCGv result, int N, TCGv src, bool sign)
{
    if (sign) {
        tcg_gen_sextract_tl(result, src, N * 16, 16);
    } else {
        tcg_gen_extract_tl(result, src, N * 16, 16);
    }
    return result;
}

void gen_set_half(int N, TCGv result, TCGv src)
{
    tcg_gen_deposit_tl(result, result, src, N * 16, 16);
}

void gen_set_half_i64(int N, TCGv_i64 result, TCGv src)
{
    TCGv_i64 src64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(src64, src);
    tcg_gen_deposit_i64(result, result, src64, N * 16, 16);
}

void gen_set_byte_i64(int N, TCGv_i64 result, TCGv src)
{
    TCGv_i64 src64 = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(src64, src);
    tcg_gen_deposit_i64(result, result, src64, N * 8, 8);
}

static inline void gen_load_locked4u(TCGv dest, TCGv vaddr, int mem_index)
{
    tcg_gen_qemu_ld_tl(dest, vaddr, mem_index, MO_TEUL);
    tcg_gen_mov_tl(hex_llsc_addr, vaddr);
    tcg_gen_mov_tl(hex_llsc_val, dest);
}

static inline void gen_load_locked8u(TCGv_i64 dest, TCGv vaddr, int mem_index)
{
    tcg_gen_qemu_ld_i64(dest, vaddr, mem_index, MO_TEUQ);
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
    tcg_gen_br(done);

    gen_set_label(fail);
    tcg_gen_movi_tl(pred, 0);

    gen_set_label(done);
    tcg_gen_movi_tl(hex_llsc_addr, ~0);
}

void gen_store32(TCGv vaddr, TCGv src, int width, uint32_t slot)
{
    tcg_gen_mov_tl(hex_store_addr[slot], vaddr);
    tcg_gen_movi_tl(hex_store_width[slot], width);
    tcg_gen_mov_tl(hex_store_val32[slot], src);
}

void gen_store1(TCGv_env cpu_env, TCGv vaddr, TCGv src, uint32_t slot)
{
    gen_store32(vaddr, src, 1, slot);
}

void gen_store1i(TCGv_env cpu_env, TCGv vaddr, int32_t src, uint32_t slot)
{
    gen_store1(cpu_env, vaddr, tcg_constant_tl(src), slot);
}

void gen_store2(TCGv_env cpu_env, TCGv vaddr, TCGv src, uint32_t slot)
{
    gen_store32(vaddr, src, 2, slot);
}

void gen_store2i(TCGv_env cpu_env, TCGv vaddr, int32_t src, uint32_t slot)
{
    gen_store2(cpu_env, vaddr, tcg_constant_tl(src), slot);
}

void gen_store4(TCGv_env cpu_env, TCGv vaddr, TCGv src, uint32_t slot)
{
    gen_store32(vaddr, src, 4, slot);
}

void gen_store4i(TCGv_env cpu_env, TCGv vaddr, int32_t src, uint32_t slot)
{
    gen_store4(cpu_env, vaddr, tcg_constant_tl(src), slot);
}

void gen_store8(TCGv_env cpu_env, TCGv vaddr, TCGv_i64 src, uint32_t slot)
{
    tcg_gen_mov_tl(hex_store_addr[slot], vaddr);
    tcg_gen_movi_tl(hex_store_width[slot], 8);
    tcg_gen_mov_i64(hex_store_val64[slot], src);
}

void gen_store8i(TCGv_env cpu_env, TCGv vaddr, int64_t src, uint32_t slot)
{
    gen_store8(cpu_env, vaddr, tcg_constant_i64(src), slot);
}

TCGv gen_8bitsof(TCGv result, TCGv value, DisasContext *ctx)
{
    tcg_gen_movcond_tl(TCG_COND_NE, result, value,
                       ctx->zero, ctx->ones, ctx->zero);

    return result;
}

static void gen_write_new_pc_addr(DisasContext *ctx, TCGv addr,
                                  TCGCond cond, TCGv pred)
{
    TCGLabel *pred_false = NULL;
    if (cond != TCG_COND_ALWAYS) {
        pred_false = gen_new_label();
        tcg_gen_brcondi_tl(cond, pred, 0, pred_false);
    }

    if (ctx->pkt->pkt_has_multi_cof) {
        /* If there are multiple branches in a packet, ignore the second one */
        tcg_gen_movcond_tl(TCG_COND_NE, hex_gpr[HEX_REG_PC],
                           hex_branch_taken, tcg_constant_tl(0),
                           hex_gpr[HEX_REG_PC], addr);
        tcg_gen_movi_tl(hex_branch_taken, 1);
    } else {
        tcg_gen_mov_tl(hex_gpr[HEX_REG_PC], addr);
    }

    if (cond != TCG_COND_ALWAYS) {
        gen_set_label(pred_false);
    }
}

static void gen_write_new_pc_pcrel(DisasContext *ctx, int pc_off,
                                   TCGCond cond, TCGv pred)
{
    target_ulong dest = ctx->pkt->pc + pc_off;
    if (ctx->pkt->pkt_has_multi_cof) {
        gen_write_new_pc_addr(ctx, tcg_constant_tl(dest), cond, pred);
    } else {
        /* Defer this jump to the end of the TB */
        ctx->branch_cond = TCG_COND_ALWAYS;
        if (pred != NULL) {
            ctx->branch_cond = cond;
            tcg_gen_mov_tl(hex_branch_taken, pred);
        }
        ctx->branch_dest = dest;
    }
}

void gen_set_usr_field(DisasContext *ctx, int field, TCGv val)
{
    TCGv usr = get_result_gpr(ctx, HEX_REG_USR);
    tcg_gen_deposit_tl(usr, usr, val,
                       reg_field_info[field].offset,
                       reg_field_info[field].width);
}

void gen_set_usr_fieldi(DisasContext *ctx, int field, int x)
{
    if (reg_field_info[field].width == 1) {
        TCGv usr = get_result_gpr(ctx, HEX_REG_USR);
        target_ulong bit = 1 << reg_field_info[field].offset;
        if ((x & 1) == 1) {
            tcg_gen_ori_tl(usr, usr, bit);
        } else {
            tcg_gen_andi_tl(usr, usr, ~bit);
        }
    } else {
        TCGv val = tcg_constant_tl(x);
        gen_set_usr_field(ctx, field, val);
    }
}

static inline void gen_compare(TCGCond cond, TCGv res, TCGv arg1, TCGv arg2,
                               DisasContext *ctx)
{
    tcg_gen_movcond_tl(cond, res, arg1, arg2, ctx->ones, ctx->zero);
}

#ifndef CONFIG_HEXAGON_IDEF_PARSER
static inline void gen_loop0r(DisasContext *ctx, TCGv RsV, int riV)
{
    TCGv tmp = tcg_temp_new();
    fIMMEXT(riV);
    fPCALIGN(riV);
    /* fWRITE_LOOP_REGS0( fREAD_PC()+riV, RsV); */
    tcg_gen_movi_tl(tmp, ctx->pkt->pc + riV);
    gen_log_reg_write(HEX_REG_LC0, RsV);
    gen_log_reg_write(HEX_REG_SA0, tmp);
    fSET_LPCFG(0);
}

static void gen_loop0i(DisasContext *ctx, int count, int riV)
{
    gen_loop0r(ctx, tcg_constant_tl(count), riV);
}

static inline void gen_loop1r(DisasContext *ctx, TCGv RsV, int riV)
{
    TCGv tmp = tcg_temp_new();
    fIMMEXT(riV);
    fPCALIGN(riV);
    /* fWRITE_LOOP_REGS1( fREAD_PC()+riV, RsV); */
    tcg_gen_movi_tl(tmp, ctx->pkt->pc + riV);
    gen_log_reg_write(HEX_REG_LC1, RsV);
    gen_log_reg_write(HEX_REG_SA1, tmp);
}

static void gen_loop1i(DisasContext *ctx, int count, int riV)
{
    gen_loop1r(ctx, tcg_constant_tl(count), riV);
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
}
#endif

static void gen_cond_jumpr(DisasContext *ctx, TCGv dst_pc,
                           TCGCond cond, TCGv pred)
{
    gen_write_new_pc_addr(ctx, dst_pc, cond, pred);
}

static void gen_cond_jumpr31(DisasContext *ctx, TCGCond cond, TCGv pred)
{
    TCGv LSB = tcg_temp_new();
    tcg_gen_andi_tl(LSB, pred, 1);
    gen_cond_jumpr(ctx, hex_gpr[HEX_REG_LR], cond, LSB);
}

static void gen_cond_jump(DisasContext *ctx, TCGCond cond, TCGv pred,
                          int pc_off)
{
    gen_write_new_pc_pcrel(ctx, pc_off, cond, pred);
}

static void gen_cmpnd_cmp_jmp(DisasContext *ctx,
                              int pnum, TCGCond cond1, TCGv arg1, TCGv arg2,
                              TCGCond cond2, int pc_off)
{
    if (ctx->insn->part1) {
        TCGv pred = tcg_temp_new();
        gen_compare(cond1, pred, arg1, arg2, ctx);
        gen_log_pred_write(ctx, pnum, pred);
    } else {
        TCGv pred = tcg_temp_new();
        tcg_gen_mov_tl(pred, hex_new_pred_value[pnum]);
        gen_cond_jump(ctx, cond2, pred, pc_off);
    }
}

static void gen_cmpnd_cmp_jmp_t(DisasContext *ctx,
                                int pnum, TCGCond cond, TCGv arg1, TCGv arg2,
                                int pc_off)
{
    gen_cmpnd_cmp_jmp(ctx, pnum, cond, arg1, arg2, TCG_COND_EQ, pc_off);
}

static void gen_cmpnd_cmp_jmp_f(DisasContext *ctx,
                                int pnum, TCGCond cond, TCGv arg1, TCGv arg2,
                                int pc_off)
{
    gen_cmpnd_cmp_jmp(ctx, pnum, cond, arg1, arg2, TCG_COND_NE, pc_off);
}

static void gen_cmpnd_cmpi_jmp_t(DisasContext *ctx,
                                 int pnum, TCGCond cond, TCGv arg1, int arg2,
                                 int pc_off)
{
    TCGv tmp = tcg_constant_tl(arg2);
    gen_cmpnd_cmp_jmp(ctx, pnum, cond, arg1, tmp, TCG_COND_EQ, pc_off);
}

static void gen_cmpnd_cmpi_jmp_f(DisasContext *ctx,
                                 int pnum, TCGCond cond, TCGv arg1, int arg2,
                                 int pc_off)
{
    TCGv tmp = tcg_constant_tl(arg2);
    gen_cmpnd_cmp_jmp(ctx, pnum, cond, arg1, tmp, TCG_COND_NE, pc_off);
}

static void gen_cmpnd_cmp_n1_jmp_t(DisasContext *ctx, int pnum, TCGCond cond,
                                   TCGv arg, int pc_off)
{
    gen_cmpnd_cmpi_jmp_t(ctx, pnum, cond, arg, -1, pc_off);
}

static void gen_cmpnd_cmp_n1_jmp_f(DisasContext *ctx, int pnum, TCGCond cond,
                                   TCGv arg, int pc_off)
{
    gen_cmpnd_cmpi_jmp_f(ctx, pnum, cond, arg, -1, pc_off);
}

static void gen_cmpnd_tstbit0_jmp(DisasContext *ctx,
                                  int pnum, TCGv arg, TCGCond cond, int pc_off)
{
    if (ctx->insn->part1) {
        TCGv pred = tcg_temp_new();
        tcg_gen_andi_tl(pred, arg, 1);
        gen_8bitsof(pred, pred, ctx);
        gen_log_pred_write(ctx, pnum, pred);
    } else {
        TCGv pred = tcg_temp_new();
        tcg_gen_mov_tl(pred, hex_new_pred_value[pnum]);
        gen_cond_jump(ctx, cond, pred, pc_off);
    }
}

static void gen_testbit0_jumpnv(DisasContext *ctx,
                                TCGv arg, TCGCond cond, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_andi_tl(pred, arg, 1);
    gen_cond_jump(ctx, cond, pred, pc_off);
}

static void gen_jump(DisasContext *ctx, int pc_off)
{
    gen_write_new_pc_pcrel(ctx, pc_off, TCG_COND_ALWAYS, NULL);
}

static void gen_jumpr(DisasContext *ctx, TCGv new_pc)
{
    gen_write_new_pc_addr(ctx, new_pc, TCG_COND_ALWAYS, NULL);
}

static void gen_call(DisasContext *ctx, int pc_off)
{
    TCGv lr = get_result_gpr(ctx, HEX_REG_LR);
    tcg_gen_movi_tl(lr, ctx->next_PC);
    gen_write_new_pc_pcrel(ctx, pc_off, TCG_COND_ALWAYS, NULL);
}

static void gen_callr(DisasContext *ctx, TCGv new_pc)
{
    TCGv lr = get_result_gpr(ctx, HEX_REG_LR);
    tcg_gen_movi_tl(lr, ctx->next_PC);
    gen_write_new_pc_addr(ctx, new_pc, TCG_COND_ALWAYS, NULL);
}

static void gen_cond_call(DisasContext *ctx, TCGv pred,
                          TCGCond cond, int pc_off)
{
    TCGv lr = get_result_gpr(ctx, HEX_REG_LR);
    TCGv lsb = tcg_temp_new();
    TCGLabel *skip = gen_new_label();
    tcg_gen_andi_tl(lsb, pred, 1);
    gen_write_new_pc_pcrel(ctx, pc_off, cond, lsb);
    tcg_gen_brcondi_tl(cond, lsb, 0, skip);
    tcg_gen_movi_tl(lr, ctx->next_PC);
    gen_set_label(skip);
}

static void gen_cond_callr(DisasContext *ctx,
                           TCGCond cond, TCGv pred, TCGv new_pc)
{
    TCGv lsb = tcg_temp_new();
    TCGLabel *skip = gen_new_label();
    tcg_gen_andi_tl(lsb, pred, 1);
    tcg_gen_brcondi_tl(cond, lsb, 0, skip);
    gen_callr(ctx, new_pc);
    gen_set_label(skip);
}

#ifndef CONFIG_HEXAGON_IDEF_PARSER
/* frame = ((LR << 32) | FP) ^ (FRAMEKEY << 32)) */
static void gen_frame_scramble(TCGv_i64 result)
{
    TCGv_i64 framekey = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(framekey, hex_gpr[HEX_REG_FRAMEKEY]);
    tcg_gen_shli_i64(framekey, framekey, 32);
    tcg_gen_concat_i32_i64(result, hex_gpr[HEX_REG_FP], hex_gpr[HEX_REG_LR]);
    tcg_gen_xor_i64(result, result, framekey);
}
#endif

/* frame ^= (int64_t)FRAMEKEY << 32 */
static void gen_frame_unscramble(TCGv_i64 frame)
{
    TCGv_i64 framekey = tcg_temp_new_i64();
    tcg_gen_extu_i32_i64(framekey, hex_gpr[HEX_REG_FRAMEKEY]);
    tcg_gen_shli_i64(framekey, framekey, 32);
    tcg_gen_xor_i64(frame, frame, framekey);
}

static void gen_load_frame(DisasContext *ctx, TCGv_i64 frame, TCGv EA)
{
    Insn *insn = ctx->insn;  /* Needed for CHECK_NOSHUF */
    CHECK_NOSHUF(EA, 8);
    tcg_gen_qemu_ld_i64(frame, EA, ctx->mem_idx, MO_TEUQ);
}

#ifndef CONFIG_HEXAGON_IDEF_PARSER
/* Stack overflow check */
static void gen_framecheck(TCGv EA, int framesize)
{
    /* Not modelled in linux-user mode */
    /* Placeholder for system mode */
}

static void gen_allocframe(DisasContext *ctx, TCGv r29, int framesize)
{
    TCGv r30 = tcg_temp_new();
    TCGv_i64 frame = tcg_temp_new_i64();
    tcg_gen_addi_tl(r30, r29, -8);
    gen_frame_scramble(frame);
    gen_store8(cpu_env, r30, frame, ctx->insn->slot);
    gen_log_reg_write(HEX_REG_FP, r30);
    gen_framecheck(r30, framesize);
    tcg_gen_subi_tl(r29, r30, framesize);
}

static void gen_deallocframe(DisasContext *ctx, TCGv_i64 r31_30, TCGv r30)
{
    TCGv r29 = tcg_temp_new();
    TCGv_i64 frame = tcg_temp_new_i64();
    gen_load_frame(ctx, frame, r30);
    gen_frame_unscramble(frame);
    tcg_gen_mov_i64(r31_30, frame);
    tcg_gen_addi_tl(r29, r30, 8);
    gen_log_reg_write(HEX_REG_SP, r29);
}
#endif

static void gen_return(DisasContext *ctx, TCGv_i64 dst, TCGv src)
{
    /*
     * frame = *src
     * dst = frame_unscramble(frame)
     * SP = src + 8
     * PC = dst.w[1]
     */
    TCGv_i64 frame = tcg_temp_new_i64();
    TCGv r31 = tcg_temp_new();
    TCGv r29 = get_result_gpr(ctx, HEX_REG_SP);

    gen_load_frame(ctx, frame, src);
    gen_frame_unscramble(frame);
    tcg_gen_mov_i64(dst, frame);
    tcg_gen_addi_tl(r29, src, 8);
    tcg_gen_extrh_i64_i32(r31, dst);
    gen_jumpr(ctx, r31);
}

/* if (pred) dst = dealloc_return(src):raw */
static void gen_cond_return(DisasContext *ctx, TCGv_i64 dst, TCGv src,
                            TCGv pred, TCGCond cond)
{
    TCGv LSB = tcg_temp_new();
    TCGLabel *skip = gen_new_label();
    tcg_gen_andi_tl(LSB, pred, 1);

    tcg_gen_brcondi_tl(cond, LSB, 0, skip);
    gen_return(ctx, dst, src);
    gen_set_label(skip);
}

/* sub-instruction version (no RddV, so handle it manually) */
static void gen_cond_return_subinsn(DisasContext *ctx, TCGCond cond, TCGv pred)
{
    TCGv_i64 RddV = get_result_gpr_pair(ctx, HEX_REG_FP);
    gen_cond_return(ctx, RddV, hex_gpr[HEX_REG_FP], pred, cond);
    gen_log_reg_write_pair(HEX_REG_FP, RddV);
}

static void gen_endloop0(DisasContext *ctx)
{
    TCGv lpcfg = tcg_temp_new();

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
        gen_set_usr_field(ctx, USR_LPCFG, lpcfg);
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
            TCGv lc0 = get_result_gpr(ctx, HEX_REG_LC0);
            gen_jumpr(ctx, hex_gpr[HEX_REG_SA0]);
            tcg_gen_subi_tl(lc0, hex_gpr[HEX_REG_LC0], 1);
        }
        gen_set_label(label3);
    }
}

static inline void gen_endloop1(DisasContext *ctx)
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
        TCGv lc1 = get_result_gpr(ctx, HEX_REG_LC1);
        gen_jumpr(ctx, hex_gpr[HEX_REG_SA1]);
        tcg_gen_subi_tl(lc1, hex_gpr[HEX_REG_LC1], 1);
    }
    gen_set_label(label);
}

static void gen_endloop01(DisasContext *ctx)
{
    TCGv lpcfg = tcg_temp_new();
    TCGLabel *label1 = gen_new_label();
    TCGLabel *label2 = gen_new_label();
    TCGLabel *label3 = gen_new_label();
    TCGLabel *done = gen_new_label();

    GET_USR_FIELD(USR_LPCFG, lpcfg);

    /*
     *    if (lpcfg == 1) {
     *        hex_new_pred_value[3] = 0xff;
     *        hex_pred_written |= 1 << 3;
     *    }
     */
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
    tcg_gen_brcondi_tl(TCG_COND_EQ, lpcfg, 0, label2);
    {
        tcg_gen_subi_tl(lpcfg, lpcfg, 1);
        gen_set_usr_field(ctx, USR_LPCFG, lpcfg);
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
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC0], 1, label3);
    {
        TCGv lc0 = get_result_gpr(ctx, HEX_REG_LC0);
        gen_jumpr(ctx, hex_gpr[HEX_REG_SA0]);
        tcg_gen_subi_tl(lc0, hex_gpr[HEX_REG_LC0], 1);
        tcg_gen_br(done);
    }
    gen_set_label(label3);
    tcg_gen_brcondi_tl(TCG_COND_LEU, hex_gpr[HEX_REG_LC1], 1, done);
    {
        TCGv lc1 = get_result_gpr(ctx, HEX_REG_LC1);
        gen_jumpr(ctx, hex_gpr[HEX_REG_SA1]);
        tcg_gen_subi_tl(lc1, hex_gpr[HEX_REG_LC1], 1);
    }
    gen_set_label(done);
}

#ifndef CONFIG_HEXAGON_IDEF_PARSER
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
#endif

static void gen_cmp_jumpnv(DisasContext *ctx,
                           TCGCond cond, TCGv val, TCGv src, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_setcond_tl(cond, pred, val, src);
    gen_cond_jump(ctx, TCG_COND_EQ, pred, pc_off);
}

static void gen_cmpi_jumpnv(DisasContext *ctx,
                            TCGCond cond, TCGv val, int src, int pc_off)
{
    TCGv pred = tcg_temp_new();
    tcg_gen_setcondi_tl(cond, pred, val, src);
    gen_cond_jump(ctx, TCG_COND_EQ, pred, pc_off);
}

#ifndef CONFIG_HEXAGON_IDEF_PARSER
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
    TCGv tcg_min = tcg_constant_tl(min);
    TCGv tcg_max = tcg_constant_tl(max);
    TCGv satval = tcg_temp_new();
    TCGv ovf = tcg_temp_new();

    if (sign) {
        tcg_gen_sextract_tl(dst, src, 0, bits);
    } else {
        tcg_gen_extract_tl(dst, src, 0, bits);
    }
    tcg_gen_movcond_tl(TCG_COND_LT, satval, src, tcg_min, tcg_min, tcg_max);
    tcg_gen_movcond_tl(TCG_COND_EQ, dst, dst, src, dst, satval);

    tcg_gen_setcond_tl(TCG_COND_NE, ovf, dst, src);
    tcg_gen_shli_tl(ovf, ovf, reg_field_info[USR_OVF].offset);
    tcg_gen_or_tl(hex_new_value[HEX_REG_USR], hex_new_value[HEX_REG_USR], ovf);
}
#endif

/* Shift left with saturation */
static void gen_shl_sat(DisasContext *ctx, TCGv dst, TCGv src, TCGv shift_amt)
{
    TCGv usr = get_result_gpr(ctx, HEX_REG_USR);
    TCGv sh32 = tcg_temp_new();
    TCGv dst_sar = tcg_temp_new();
    TCGv ovf = tcg_temp_new();
    TCGv satval = tcg_temp_new();
    TCGv min = tcg_constant_tl(0x80000000);
    TCGv max = tcg_constant_tl(0x7fffffff);

    /*
     *    Possible values for shift_amt are 0 .. 64
     *    We need special handling for values above 31
     *
     *    sh32 = shift & 31;
     *    dst = sh32 == shift ? src : 0;
     *    dst <<= sh32;
     *    dst_sar = dst >> sh32;
     *    satval = src < 0 ? min : max;
     *    if (dst_asr != src) {
     *        usr.OVF |= 1;
     *        dst = satval;
     *    }
     */

    tcg_gen_andi_tl(sh32, shift_amt, 31);
    tcg_gen_movcond_tl(TCG_COND_EQ, dst, sh32, shift_amt,
                       src, tcg_constant_tl(0));
    tcg_gen_shl_tl(dst, dst, sh32);
    tcg_gen_sar_tl(dst_sar, dst, sh32);
    tcg_gen_movcond_tl(TCG_COND_LT, satval, src, tcg_constant_tl(0), min, max);

    tcg_gen_setcond_tl(TCG_COND_NE, ovf, dst_sar, src);
    tcg_gen_shli_tl(ovf, ovf, reg_field_info[USR_OVF].offset);
    tcg_gen_or_tl(usr, usr, ovf);

    tcg_gen_movcond_tl(TCG_COND_EQ, dst, dst_sar, src, dst, satval);
}

static void gen_sar(TCGv dst, TCGv src, TCGv shift_amt)
{
    /*
     *  Shift arithmetic right
     *  Robust when shift_amt is >31 bits
     */
    TCGv tmp = tcg_temp_new();
    tcg_gen_umin_tl(tmp, shift_amt, tcg_constant_tl(31));
    tcg_gen_sar_tl(dst, src, tmp);
}

/* Bidirectional shift right with saturation */
static void gen_asr_r_r_sat(DisasContext *ctx, TCGv RdV, TCGv RsV, TCGv RtV)
{
    TCGv shift_amt = tcg_temp_new();
    TCGLabel *positive = gen_new_label();
    TCGLabel *done = gen_new_label();

    tcg_gen_sextract_i32(shift_amt, RtV, 0, 7);
    tcg_gen_brcondi_tl(TCG_COND_GE, shift_amt, 0, positive);

    /* Negative shift amount => shift left */
    tcg_gen_neg_tl(shift_amt, shift_amt);
    gen_shl_sat(ctx, RdV, RsV, shift_amt);
    tcg_gen_br(done);

    gen_set_label(positive);
    /* Positive shift amount => shift right */
    gen_sar(RdV, RsV, shift_amt);

    gen_set_label(done);
}

/* Bidirectional shift left with saturation */
static void gen_asl_r_r_sat(DisasContext *ctx, TCGv RdV, TCGv RsV, TCGv RtV)
{
    TCGv shift_amt = tcg_temp_new();
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
    gen_shl_sat(ctx, RdV, RsV, shift_amt);

    gen_set_label(done);
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

static intptr_t get_result_qreg(DisasContext *ctx, int qnum)
{
    return  offsetof(CPUHexagonState, future_QRegs[qnum]);
}

static void gen_pause(DisasContext *ctx)
{
#ifdef CONFIG_USER_ONLY
    tcg_gen_movi_tl(hex_gpr[HEX_REG_PC], ctx->next_PC);
#else
    gen_exception(EXCP_YIELD, ctx->next_PC);
#endif
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
}

static void gen_vreg_store(DisasContext *ctx, TCGv EA, intptr_t srcoff,
                           int slot, bool aligned)
{
    intptr_t dstoff = offsetof(CPUHexagonState, vstore[slot].data);
    intptr_t maskoff = offsetof(CPUHexagonState, vstore[slot].mask);

    if (is_gather_store_insn(ctx)) {
        TCGv sl = tcg_constant_tl(slot);
        gen_helper_gather_store(cpu_env, EA, sl);
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
}

void probe_noshuf_load(TCGv va, int s, int mi)
{
    TCGv size = tcg_constant_tl(s);
    TCGv mem_idx = tcg_constant_tl(mi);
    gen_helper_probe_noshuf_load(cpu_env, va, size, mem_idx);
}

/*
 * Note: Since this function might branch, `val` is
 * required to be a `tcg_temp_local`.
 */
void gen_set_usr_field_if(DisasContext *ctx, int field, TCGv val)
{
    /* Sets the USR field if `val` is non-zero */
    if (reg_field_info[field].width == 1) {
        TCGv usr = get_result_gpr(ctx, HEX_REG_USR);
        TCGv tmp = tcg_temp_new();
        tcg_gen_extract_tl(tmp, val, 0, reg_field_info[field].width);
        tcg_gen_shli_tl(tmp, tmp, reg_field_info[field].offset);
        tcg_gen_or_tl(usr, usr, tmp);
    } else {
        TCGLabel *skip_label = gen_new_label();
        tcg_gen_brcondi_tl(TCG_COND_EQ, val, 0, skip_label);
        gen_set_usr_field(ctx, field, val);
        gen_set_label(skip_label);
    }
}

void gen_sat_i32(TCGv dest, TCGv source, int width)
{
    TCGv max_val = tcg_constant_tl((1 << (width - 1)) - 1);
    TCGv min_val = tcg_constant_tl(-(1 << (width - 1)));
    tcg_gen_smin_tl(dest, source, max_val);
    tcg_gen_smax_tl(dest, dest, min_val);
}

void gen_sat_i32_ovfl(TCGv ovfl, TCGv dest, TCGv source, int width)
{
    gen_sat_i32(dest, source, width);
    tcg_gen_setcond_tl(TCG_COND_NE, ovfl, source, dest);
}

void gen_satu_i32(TCGv dest, TCGv source, int width)
{
    TCGv max_val = tcg_constant_tl((1 << width) - 1);
    TCGv zero = tcg_constant_tl(0);
    tcg_gen_movcond_tl(TCG_COND_GTU, dest, source, max_val, max_val, source);
    tcg_gen_movcond_tl(TCG_COND_LT, dest, source, zero, zero, dest);
}

void gen_satu_i32_ovfl(TCGv ovfl, TCGv dest, TCGv source, int width)
{
    gen_satu_i32(dest, source, width);
    tcg_gen_setcond_tl(TCG_COND_NE, ovfl, source, dest);
}

void gen_sat_i64(TCGv_i64 dest, TCGv_i64 source, int width)
{
    TCGv_i64 max_val = tcg_constant_i64((1LL << (width - 1)) - 1LL);
    TCGv_i64 min_val = tcg_constant_i64(-(1LL << (width - 1)));
    tcg_gen_smin_i64(dest, source, max_val);
    tcg_gen_smax_i64(dest, dest, min_val);
}

void gen_sat_i64_ovfl(TCGv ovfl, TCGv_i64 dest, TCGv_i64 source, int width)
{
    TCGv_i64 ovfl_64;
    gen_sat_i64(dest, source, width);
    ovfl_64 = tcg_temp_new_i64();
    tcg_gen_setcond_i64(TCG_COND_NE, ovfl_64, dest, source);
    tcg_gen_trunc_i64_tl(ovfl, ovfl_64);
}

void gen_satu_i64(TCGv_i64 dest, TCGv_i64 source, int width)
{
    TCGv_i64 max_val = tcg_constant_i64((1LL << width) - 1LL);
    TCGv_i64 zero = tcg_constant_i64(0);
    tcg_gen_movcond_i64(TCG_COND_GTU, dest, source, max_val, max_val, source);
    tcg_gen_movcond_i64(TCG_COND_LT, dest, source, zero, zero, dest);
}

void gen_satu_i64_ovfl(TCGv ovfl, TCGv_i64 dest, TCGv_i64 source, int width)
{
    TCGv_i64 ovfl_64;
    gen_satu_i64(dest, source, width);
    ovfl_64 = tcg_temp_new_i64();
    tcg_gen_setcond_i64(TCG_COND_NE, ovfl_64, dest, source);
    tcg_gen_trunc_i64_tl(ovfl, ovfl_64);
}

/* Implements the fADDSAT64 macro in TCG */
void gen_add_sat_i64(DisasContext *ctx, TCGv_i64 ret, TCGv_i64 a, TCGv_i64 b)
{
    TCGv_i64 sum = tcg_temp_new_i64();
    TCGv_i64 xor = tcg_temp_new_i64();
    TCGv_i64 cond1 = tcg_temp_new_i64();
    TCGv_i64 cond2 = tcg_temp_new_i64();
    TCGv_i64 cond3 = tcg_temp_new_i64();
    TCGv_i64 mask = tcg_constant_i64(0x8000000000000000ULL);
    TCGv_i64 max_pos = tcg_constant_i64(0x7FFFFFFFFFFFFFFFLL);
    TCGv_i64 max_neg = tcg_constant_i64(0x8000000000000000LL);
    TCGv_i64 zero = tcg_constant_i64(0);
    TCGLabel *no_ovfl_label = gen_new_label();
    TCGLabel *ovfl_label = gen_new_label();
    TCGLabel *ret_label = gen_new_label();

    tcg_gen_add_i64(sum, a, b);
    tcg_gen_xor_i64(xor, a, b);

    /* if (xor & mask) */
    tcg_gen_and_i64(cond1, xor, mask);
    tcg_gen_brcondi_i64(TCG_COND_NE, cond1, 0, no_ovfl_label);

    /* else if ((a ^ sum) & mask) */
    tcg_gen_xor_i64(cond2, a, sum);
    tcg_gen_and_i64(cond2, cond2, mask);
    tcg_gen_brcondi_i64(TCG_COND_NE, cond2, 0, ovfl_label);
    /* fallthrough to no_ovfl_label branch */

    /* if branch */
    gen_set_label(no_ovfl_label);
    tcg_gen_mov_i64(ret, sum);
    tcg_gen_br(ret_label);

    /* else if branch */
    gen_set_label(ovfl_label);
    tcg_gen_and_i64(cond3, sum, mask);
    tcg_gen_movcond_i64(TCG_COND_NE, ret, cond3, zero, max_pos, max_neg);
    gen_set_usr_fieldi(ctx, USR_OVF, 1);

    gen_set_label(ret_label);
}

#include "tcg_funcs_generated.c.inc"
#include "tcg_func_table_generated.c.inc"
