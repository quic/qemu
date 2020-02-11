/*
 *  Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#define QEMU_GENERATE
#include "qemu/osdep.h"
#include "cpu.h"
#include "tcg/tcg-op.h"
#include "exec/cpu_ldst.h"
#include "exec/log.h"
#include "internal.h"
#include "attribs.h"
#include "insn.h"
#include "decode.h"
#include "translate.h"
#include "printinsn.h"
#include "macros.h"

TCGv hex_gpr[TOTAL_PER_THREAD_REGS];
TCGv hex_pred[NUM_PREGS];
TCGv hex_next_PC;
TCGv hex_this_PC;
TCGv hex_slot_cancelled;
TCGv hex_branch_taken;
TCGv hex_new_value[TOTAL_PER_THREAD_REGS];
#if HEX_DEBUG
TCGv hex_reg_written[TOTAL_PER_THREAD_REGS];
#endif
TCGv hex_new_pred_value[NUM_PREGS];
TCGv hex_pred_written[NUM_PREGS];
TCGv hex_store_addr[STORES_MAX];
TCGv hex_store_width[STORES_MAX];
TCGv hex_store_val32[STORES_MAX];
TCGv_i64 hex_store_val64[STORES_MAX];
TCGv hex_dczero_addr;
TCGv llsc_addr;
TCGv llsc_val;
TCGv_i64 llsc_val_i64;
TCGv hex_is_gather_store_insn;
TCGv hex_gather_issued;
TCGv hex_VRegs_updated_tmp;
TCGv hex_VRegs_updated;
TCGv hex_VRegs_select;
TCGv hex_QRegs_updated;

static const char * const hexagon_prednames[] = {
  "p0", "p1", "p2", "p3"
};

void gen_exception(int excp)
{
    TCGv_i32 helper_tmp = tcg_const_i32(excp);
    gen_helper_raise_exception(cpu_env, helper_tmp);
    tcg_temp_free_i32(helper_tmp);
}

void gen_exception_debug(void)
{
    gen_exception(EXCP_DEBUG);
}

#if HEX_DEBUG
static void print_pkt(packet_t *pkt)
{
    char buf[1028];
    snprint_a_pkt(buf, 128, pkt);
    HEX_DEBUG_LOG("%s", buf);
}
#define HEX_DEBUG_PRINT_PKT(pkt)  print_pkt(pkt)
#else
#define HEX_DEBUG_PRINT_PKT(pkt)  /* nothing */
#endif

static int read_packet_words(CPUHexagonState *env, DisasContext *ctx,
                             uint32_t words[])
{
    bool found_end = false;
    int max_words;
    int nwords;
    int i;

    /* Make sure we don't cross a page boundary */
    max_words = -(ctx->base.pc_next | TARGET_PAGE_MASK) / sizeof(uint32_t);
    if (max_words < PACKET_WORDS_MAX) {
        /* Might cross a page boundary */
        if (ctx->base.num_insns == 1) {
            /* OK if it's the first packet in the TB */
            max_words = PACKET_WORDS_MAX;
        }
    } else {
        max_words = PACKET_WORDS_MAX;
    }

    memset(words, 0, PACKET_WORDS_MAX * sizeof(uint32_t));
    for (nwords = 0; !found_end && nwords < max_words; nwords++) {
        words[nwords] = cpu_ldl_code(env,
                                ctx->base.pc_next + nwords * sizeof(uint32_t));
        found_end = is_packet_end(words[nwords]);
    }
    if (!found_end) {
        if (nwords == PACKET_WORDS_MAX) {
            /* Read too many words without finding the end */
            gen_exception(HEX_EXCP_INVALID_PACKET);
            ctx->base.is_jmp = DISAS_NORETURN;
            return 0;
        }
        /* Crosses page boundary - defer to next TB */
        ctx->base.is_jmp = DISAS_TOO_MANY;
        return 0;
    }

    HEX_DEBUG_LOG("decode_packet: pc = 0x%x\n", ctx->base.pc_next);
    HEX_DEBUG_LOG("    words = { ");
    for (i = 0; i < nwords; i++) {
        HEX_DEBUG_LOG("0x%x, ", words[i]);
    }
    HEX_DEBUG_LOG("}\n");

    return nwords;
}

static void gen_start_packet(DisasContext *ctx, packet_t *pkt)
{
    target_ulong next_PC = ctx->base.pc_next + pkt->encod_pkt_size_in_bytes;
    int i;

    /* Clear out the disassembly context */
    ctx->ctx_reg_log_idx = 0;
    ctx->ctx_preg_log_idx = 0;
    ctx->ctx_temp_vregs_idx = 0;
    ctx->ctx_temp_qregs_idx = 0;
    ctx->ctx_vreg_log_idx = 0;
    ctx->ctx_qreg_log_idx = 0;
    for (i = 0; i < STORES_MAX; i++) {
        ctx->ctx_store_width[i] = 0;
    }

#if HEX_DEBUG
    /* Handy place to set a breakpoint before the packet executes */
    gen_helper_debug_start_packet(cpu_env);
#endif

    /* Initialize the runtime state for packet semantics */
    tcg_gen_movi_tl(hex_gpr[HEX_REG_PC], ctx->base.pc_next);
    tcg_gen_movi_tl(hex_slot_cancelled, 0);
    tcg_gen_movi_tl(hex_branch_taken, 0);
    tcg_gen_mov_tl(hex_this_PC, hex_gpr[HEX_REG_PC]);
    tcg_gen_movi_tl(hex_next_PC, next_PC);
    for (i = 0; i < NUM_PREGS; i++) {
        tcg_gen_movi_tl(hex_pred_written[i], 0);
    }

    if (pkt->pkt_has_hvx) {
        tcg_gen_movi_tl(hex_VRegs_updated_tmp, 0);
        tcg_gen_movi_tl(hex_VRegs_updated, 0);
        tcg_gen_movi_tl(hex_VRegs_select, 0);
        tcg_gen_movi_tl(hex_QRegs_updated, 0);
        tcg_gen_movi_tl(hex_is_gather_store_insn, 0);
        tcg_gen_movi_tl(hex_gather_issued, 0);
    }
}

static int is_gather_store_insn(insn_t *insn)
{
    int check = GET_ATTRIB(insn->opcode, A_CVI_NEW);
    check &= (insn->new_value_producer_slot == 1);
    return check;
}

/*
 * The LOG_*_WRITE macros mark most of the writes in a packet
 * However, there are some implicit writes marked as attributes
 * of the applicable instructions.
 */
static void mark_implicit_reg_write(DisasContext *ctx, insn_t *insn,
                                    int attrib, int rnum)
{
    if (GET_ATTRIB(insn->opcode, attrib)) {
        ctx->ctx_reg_log[ctx->ctx_reg_log_idx] = rnum;
        ctx->ctx_reg_log_idx++;
    }
}

static void mark_implicit_pred_write(DisasContext *ctx, insn_t *insn,
                                     int attrib, int pnum)
{
    if (GET_ATTRIB(insn->opcode, attrib)) {
        ctx->ctx_preg_log[ctx->ctx_preg_log_idx] = pnum;
        ctx->ctx_preg_log_idx++;
    }
}

static void mark_implicit_writes(DisasContext *ctx, insn_t *insn)
{
    mark_implicit_reg_write(ctx, insn, A_IMPLICIT_WRITES_LR,  HEX_REG_LR);
    mark_implicit_reg_write(ctx, insn, A_IMPLICIT_WRITES_LC0, HEX_REG_LC0);
    mark_implicit_reg_write(ctx, insn, A_IMPLICIT_WRITES_SA0, HEX_REG_SA0);
    mark_implicit_reg_write(ctx, insn, A_IMPLICIT_WRITES_LC1, HEX_REG_LC1);
    mark_implicit_reg_write(ctx, insn, A_IMPLICIT_WRITES_SA1, HEX_REG_SA1);

    mark_implicit_pred_write(ctx, insn, A_IMPLICIT_WRITES_P0, 0);
    mark_implicit_pred_write(ctx, insn, A_IMPLICIT_WRITES_P1, 1);
    mark_implicit_pred_write(ctx, insn, A_IMPLICIT_WRITES_P2, 2);
    mark_implicit_pred_write(ctx, insn, A_IMPLICIT_WRITES_P3, 3);
}

static void gen_insn(CPUHexagonState *env, DisasContext *ctx, insn_t *insn)
{
    if (insn->generate) {
        bool is_gather_store = is_gather_store_insn(insn);
        if (is_gather_store) {
            tcg_gen_movi_tl(hex_is_gather_store_insn, 1);
        }
        insn->generate(env, ctx, insn);
        mark_implicit_writes(ctx, insn);
        if (is_gather_store) {
            tcg_gen_movi_tl(hex_is_gather_store_insn, 0);
        }
    } else {
        gen_exception(HEX_EXCP_INVALID_OPCODE);
        ctx->base.is_jmp = DISAS_NORETURN;
    }
}

/*
 * Helpers for generating the packet commit
 */
static void gen_reg_writes(DisasContext *ctx)
{
    int i;

    for (i = 0; i < ctx->ctx_reg_log_idx; i++) {
        int reg_num = ctx->ctx_reg_log[i];

        tcg_gen_mov_tl(hex_gpr[reg_num], hex_new_value[reg_num]);
#if HEX_DEBUG
        /* Do this so HELPER(debug_commit_end) will know */
        tcg_gen_movi_tl(hex_reg_written[reg_num], 1);
#endif
    }
}

static void gen_pred_writes(DisasContext *ctx, packet_t *pkt)
{
    /* Early exit if the log is empty */
    if (!ctx->ctx_preg_log_idx) {
        return;
    }

    TCGv zero = tcg_const_tl(0);
    TCGv control_reg = tcg_temp_new();
    TCGv pval = tcg_temp_new();
    int i;

    /*
     * Only endloop instructions will conditionally
     * write a predicate.  If there are no endloop
     * instructions, we can use the non-conditional
     * write of the predicates.
     */
    if (pkt->pkt_has_endloop) {
        for (i = 0; i < ctx->ctx_preg_log_idx; i++) {
            int pred_num = ctx->ctx_preg_log[i];

            tcg_gen_movcond_tl(TCG_COND_NE, hex_pred[pred_num],
                               hex_pred_written[pred_num], zero,
                               hex_new_pred_value[pred_num],
                               hex_pred[pred_num]);
        }
    } else {
        for (i = 0; i < ctx->ctx_preg_log_idx; i++) {
            int pred_num = ctx->ctx_preg_log[i];
            tcg_gen_mov_tl(hex_pred[pred_num], hex_new_pred_value[pred_num]);
#if HEX_DEBUG
            /* Do this so HELPER(debug_commit_end) will know */
            tcg_gen_movi_tl(hex_pred_written[pred_num], 1);
#endif
        }
    }

    tcg_temp_free(zero);
    tcg_temp_free(control_reg);
    tcg_temp_free(pval);
}

#if HEX_DEBUG
static inline void gen_check_store_width(DisasContext *ctx, int slot_num)
{
    TCGv slot = tcg_const_tl(slot_num);
    TCGv check = tcg_const_tl(ctx->ctx_store_width[slot_num]);
    gen_helper_debug_check_store_width(cpu_env, slot, check);
    tcg_temp_free(slot);
    tcg_temp_free(check);
}
#define HEX_DEBUG_GEN_CHECK_STORE_WIDTH(ctx, slot_num) \
    gen_check_store_width(ctx, slot_num)
#else
#define HEX_DEBUG_GEN_CHECK_STORE_WIDTH(ctx, slot_num)  /* nothing */
#endif

static void process_store(DisasContext *ctx, int slot_num)
{
    TCGv cancelled = tcg_temp_local_new();
    TCGLabel *label_end = gen_new_label();

    /* Don't do anything if the slot was cancelled */
    gen_slot_cancelled_check(cancelled, slot_num);
    tcg_gen_brcondi_tl(TCG_COND_NE, cancelled, 0, label_end);
    {
        int ctx_width = ctx->ctx_store_width[slot_num];
        TCGv address = tcg_temp_local_new();
        tcg_gen_mov_tl(address, hex_store_addr[slot_num]);

        /*
         * If we know the width from the DisasContext, we can
         * generate much cleaner code.
         * Unfortunately, not all instructions execute the fSTORE
         * macro during code generation.  Anything that uses the
         * generic helper will have this problem.  Instructions
         * that use fWRAP to generate proper TCG code will be OK.
         */
        if (ctx_width == 1) {
            HEX_DEBUG_GEN_CHECK_STORE_WIDTH(ctx, slot_num);
            TCGv value = tcg_temp_new();
            tcg_gen_mov_tl(value, hex_store_val32[slot_num]);
            tcg_gen_qemu_st8(value, address, ctx->mem_idx);
            tcg_temp_free(value);
        } else if (ctx_width == 2) {
            HEX_DEBUG_GEN_CHECK_STORE_WIDTH(ctx, slot_num);
            TCGv value = tcg_temp_new();
            tcg_gen_mov_tl(value, hex_store_val32[slot_num]);
            tcg_gen_qemu_st16(value, address, ctx->mem_idx);
            tcg_temp_free(value);
        } else if (ctx_width == 4) {
            HEX_DEBUG_GEN_CHECK_STORE_WIDTH(ctx, slot_num);
            TCGv value = tcg_temp_new();
            tcg_gen_mov_tl(value, hex_store_val32[slot_num]);
            tcg_gen_qemu_st32(value, address, ctx->mem_idx);
            tcg_temp_free(value);
        } else if (ctx_width == 8) {
            HEX_DEBUG_GEN_CHECK_STORE_WIDTH(ctx, slot_num);
            TCGv_i64 value = tcg_temp_new_i64();
            tcg_gen_mov_i64(value, hex_store_val64[slot_num]);
            tcg_gen_qemu_st64(value, address, ctx->mem_idx);
            tcg_temp_free_i64(value);
        } else {
            /*
             * If we get to here, we don't know the width at
             * TCG generation time, we'll generate branching
             * based on the width at runtime.
             */
            TCGLabel *label_w2 = gen_new_label();
            TCGLabel *label_w4 = gen_new_label();
            TCGLabel *label_w8 = gen_new_label();
            TCGv width = tcg_temp_local_new();

            tcg_gen_mov_tl(width, hex_store_width[slot_num]);
            tcg_gen_brcondi_tl(TCG_COND_NE, width, 1, label_w2);
            {
                /* Width is 1 byte */
                TCGv value = tcg_temp_new();
                tcg_gen_mov_tl(value, hex_store_val32[slot_num]);
                tcg_gen_qemu_st8(value, address, ctx->mem_idx);
                tcg_gen_br(label_end);
                tcg_temp_free(value);
            }
            gen_set_label(label_w2);
            tcg_gen_brcondi_tl(TCG_COND_NE, width, 2, label_w4);
            {
                /* Width is 2 bytes */
                TCGv value = tcg_temp_new();
                tcg_gen_mov_tl(value, hex_store_val32[slot_num]);
                tcg_gen_qemu_st16(value, address, ctx->mem_idx);
                tcg_gen_br(label_end);
                tcg_temp_free(value);
            }
            gen_set_label(label_w4);
            tcg_gen_brcondi_tl(TCG_COND_NE, width, 4, label_w8);
            {
                /* Width is 4 bytes */
                TCGv value = tcg_temp_new();
                tcg_gen_mov_tl(value, hex_store_val32[slot_num]);
                tcg_gen_qemu_st32(value, address, ctx->mem_idx);
                tcg_gen_br(label_end);
                tcg_temp_free(value);
            }
            gen_set_label(label_w8);
            {
                /* Width is 8 bytes */
                TCGv_i64 value = tcg_temp_new_i64();
                tcg_gen_mov_i64(value, hex_store_val64[slot_num]);
                tcg_gen_qemu_st64(value, address, ctx->mem_idx);
                tcg_gen_br(label_end);
                tcg_temp_free_i64(value);
            }

            tcg_temp_free(width);
        }
        tcg_temp_free(address);
    }
    gen_set_label(label_end);

    tcg_temp_free(cancelled);
}

static void process_store_log(DisasContext *ctx, packet_t *pkt)
{
    /*
     *  When a packet has two stores, the hardware processes
     *  slot 1 and then slot 2.  This will be important when
     *  the memory accesses overlap.
     */
    if (pkt->pkt_has_store_s1 && !pkt->pkt_has_dczeroa) {
        process_store(ctx, 1);
    }
    if (pkt->pkt_has_store_s0 && !pkt->pkt_has_dczeroa) {
        process_store(ctx, 0);
    }
}

/* Zero out a 32-bit cache line */
static void process_dczeroa(DisasContext *ctx, packet_t *pkt)
{
    if (pkt->pkt_has_dczeroa) {
        /* Store 32 bytes of zero starting at (addr & ~0x1f) */
        TCGv addr = tcg_temp_new();
        TCGv_i64 zero = tcg_const_i64(0);

        tcg_gen_andi_tl(addr, hex_dczero_addr, ~0x1f);
        tcg_gen_qemu_st64(zero, addr, ctx->mem_idx);
        tcg_gen_addi_tl(addr, addr, 8);
        tcg_gen_qemu_st64(zero, addr, ctx->mem_idx);
        tcg_gen_addi_tl(addr, addr, 8);
        tcg_gen_qemu_st64(zero, addr, ctx->mem_idx);
        tcg_gen_addi_tl(addr, addr, 8);
        tcg_gen_qemu_st64(zero, addr, ctx->mem_idx);

        tcg_temp_free(addr);
        tcg_temp_free_i64(zero);
    }
}

static bool process_change_of_flow(DisasContext *ctx, packet_t *pkt)
{
    if (pkt->pkt_has_cof) {
        tcg_gen_mov_tl(hex_gpr[HEX_REG_PC], hex_next_PC);
        return true;
    }
    return false;
}

void gen_memcpy(TCGv_ptr dest, TCGv_ptr src, size_t n)
{
    TCGv_ptr d = tcg_temp_new_ptr();
    TCGv_ptr s = tcg_temp_new_ptr();
    int i;

    tcg_gen_addi_ptr(d, dest, 0);
    tcg_gen_addi_ptr(s, src, 0);
    if (n % 8 == 0) {
        TCGv_i64 temp = tcg_temp_new_i64();
        for (i = 0; i < n / 8; i++) {
            tcg_gen_ld_i64(temp, s, 0);
            tcg_gen_st_i64(temp, d, 0);
            tcg_gen_addi_ptr(s, s, 8);
            tcg_gen_addi_ptr(d, d, 8);
        }
        tcg_temp_free_i64(temp);
    } else if (n % 4 == 0) {
        TCGv temp = tcg_temp_new();
        for (i = 0; i < n / 4; i++) {
            tcg_gen_ld32u_tl(temp, s, 0);
            tcg_gen_st32_tl(temp, d, 0);
            tcg_gen_addi_ptr(s, s, 4);
            tcg_gen_addi_ptr(d, d, 4);
        }
        tcg_temp_free(temp);
    } else if (n % 2 == 0) {
        TCGv temp = tcg_temp_new();
        for (i = 0; i < n / 2; i++) {
            tcg_gen_ld16u_tl(temp, s, 0);
            tcg_gen_st16_tl(temp, d, 0);
            tcg_gen_addi_ptr(s, s, 2);
            tcg_gen_addi_ptr(d, d, 2);
        }
        tcg_temp_free(temp);
    } else {
        TCGv temp = tcg_temp_new();
        for (i = 0; i < n; i++) {
            tcg_gen_ld8u_tl(temp, s, 0);
            tcg_gen_st8_tl(temp, d, 0);
            tcg_gen_addi_ptr(s, s, 1);
            tcg_gen_addi_ptr(d, d, 1);
        }
        tcg_temp_free(temp);
    }

    tcg_temp_free_ptr(d);
    tcg_temp_free_ptr(s);
}

static inline void gen_vec_copy(intptr_t dstoff, intptr_t srcoff, size_t size)
{
    TCGv_ptr src = tcg_temp_new_ptr();
    TCGv_ptr dst = tcg_temp_new_ptr();
    tcg_gen_addi_ptr(src, cpu_env, srcoff);
    tcg_gen_addi_ptr(dst, cpu_env, dstoff);
    gen_memcpy(dst, src, size);
    tcg_temp_free_ptr(src);
    tcg_temp_free_ptr(dst);
}

static void gen_commit_hvx(DisasContext *ctx)
{
    int i;

    /*
     *    for (i = 0; i < ctx->ctx_vreg_log_idx; i++) {
     *        int rnum = ctx->ctx_vreg_log[i];
     *        if (ctx->ctx_vreg_is_predicated[i]) {
     *            if (env->VRegs_updated & (1 << rnum)) {
     *                env->VRegs[rnum] = env->future_VRegs[rnum];
     *            }
     *        } else {
     *            env->VRegs[rnum] = env->future_VRegs[rnum];
     *        }
     *    }
     */
    for (i = 0; i < ctx->ctx_vreg_log_idx; i++) {
        int rnum = ctx->ctx_vreg_log[i];
        int is_predicated = ctx->ctx_vreg_is_predicated[i];
        intptr_t dstoff = offsetof(CPUHexagonState, VRegs[rnum]);
        intptr_t srcoff = offsetof(CPUHexagonState, future_VRegs[rnum]);
        size_t size = sizeof(mmvector_t);

        if (is_predicated) {
            TCGv cmp = tcg_temp_local_new();
            TCGLabel *label_skip = gen_new_label();

            tcg_gen_andi_tl(cmp, hex_VRegs_updated, 1 << rnum);
            tcg_gen_brcondi_tl(TCG_COND_EQ, cmp, 0, label_skip);
            {
                gen_vec_copy(dstoff, srcoff, size);
            }
            gen_set_label(label_skip);
            tcg_temp_free(cmp);
        } else {
            gen_vec_copy(dstoff, srcoff, size);
        }
    }

    /*
     *    for (i = 0; i < ctx-_ctx_qreg_log_idx; i++) {
     *        int rnum = ctx->ctx_qreg_log[i];
     *        if (ctx->ctx_qreg_is_predicated[i]) {
     *            if (env->QRegs_updated) & (1 << rnum)) {
     *                env->QRegs[rnum] = env->future_QRegs[rnum];
     *            }
     *        } else {
     *            env->QRegs[rnum] = env->future_QRegs[rnum];
     *        }
     *    }
     */
    for (i = 0; i < ctx->ctx_qreg_log_idx; i++) {
        int rnum = ctx->ctx_qreg_log[i];
        int is_predicated = ctx->ctx_qreg_is_predicated[i];
        intptr_t dstoff = offsetof(CPUHexagonState, QRegs[rnum]);
        intptr_t srcoff = offsetof(CPUHexagonState, future_QRegs[rnum]);
        size_t size = sizeof(mmqreg_t);

        if (is_predicated) {
            TCGv cmp = tcg_temp_local_new();
            TCGLabel *label_skip = gen_new_label();

            tcg_gen_andi_tl(cmp, hex_QRegs_updated, 1 << rnum);
            tcg_gen_brcondi_tl(TCG_COND_EQ, cmp, 0, label_skip);
            {
                gen_vec_copy(dstoff, srcoff, size);
            }
            gen_set_label(label_skip);
            tcg_temp_free(cmp);
        } else {
            gen_vec_copy(dstoff, srcoff, size);
        }
    }

    gen_helper_commit_hvx_stores(cpu_env);
}

static void gen_exec_counters(packet_t *pkt)
{
    int num_insns = pkt->num_insns;
    int num_real_insns = 0;
    int num_hvx_insns = 0;
    int i;

    for (i = 0; i < num_insns; i++) {
        if (!pkt->insn[i].is_endloop &&
            !pkt->insn[i].part1 &&
            !GET_ATTRIB(pkt->insn[i].opcode, A_IT_NOP)) {
            num_real_insns++;
        }
        if (pkt->insn[i].hvx_resource) {
            num_hvx_insns++;
        }
    }

    tcg_gen_addi_tl(hex_gpr[HEX_REG_QEMU_PKT_CNT],
                    hex_gpr[HEX_REG_QEMU_PKT_CNT], 1);
    if (num_real_insns) {
        tcg_gen_addi_tl(hex_gpr[HEX_REG_QEMU_INSN_CNT],
                        hex_gpr[HEX_REG_QEMU_INSN_CNT], num_real_insns);
    }
    if (num_hvx_insns) {
        tcg_gen_addi_tl(hex_gpr[HEX_REG_QEMU_HVX_CNT],
                        hex_gpr[HEX_REG_QEMU_HVX_CNT], num_hvx_insns);
    }
}

static void gen_commit_packet(DisasContext *ctx, packet_t *pkt)
{
    bool end_tb = false;

    gen_reg_writes(ctx);
    gen_pred_writes(ctx, pkt);
    process_store_log(ctx, pkt);
    process_dczeroa(ctx, pkt);
    end_tb |= process_change_of_flow(ctx, pkt);
    if (pkt->pkt_has_hvx) {
        gen_commit_hvx(ctx);
    }
    gen_exec_counters(pkt);
#if HEX_DEBUG
    {
        TCGv has_st0 =
            tcg_const_tl(pkt->pkt_has_store_s0 && !pkt->pkt_has_dczeroa);
        TCGv has_st1 =
            tcg_const_tl(pkt->pkt_has_store_s1 && !pkt->pkt_has_dczeroa);

        /* Handy place to set a breakpoint at the end of execution */
        gen_helper_debug_commit_end(cpu_env, has_st0, has_st1);

        tcg_temp_free(has_st0);
        tcg_temp_free(has_st1);
    }
#endif

    if (end_tb) {
        tcg_gen_exit_tb(NULL, 0);
        ctx->base.is_jmp = DISAS_NORETURN;
    }
}

static void decode_and_translate_packet(CPUHexagonState *env, DisasContext *ctx)
{
    uint32_t words[PACKET_WORDS_MAX];
    int nwords;
    packet_t pkt;
    int i;

    nwords = read_packet_words(env, ctx, words);
    if (!nwords) {
        return;
    }

    if (decode_this(nwords, words, &pkt)) {
        HEX_DEBUG_PRINT_PKT(&pkt);
        gen_start_packet(ctx, &pkt);
        for (i = 0; i < pkt.num_insns; i++) {
            gen_insn(env, ctx, &pkt.insn[i]);
        }
        gen_commit_packet(ctx, &pkt);
        ctx->base.pc_next += pkt.encod_pkt_size_in_bytes;
    } else {
        gen_exception(HEX_EXCP_INVALID_PACKET);
        ctx->base.is_jmp = DISAS_NORETURN;
    }
}

static void hexagon_tr_init_disas_context(DisasContextBase *dcbase,
                                          CPUState *cs)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    ctx->mem_idx = MMU_USER_IDX;
}

static void hexagon_tr_tb_start(DisasContextBase *db, CPUState *cpu)
{
}

static void hexagon_tr_insn_start(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    tcg_gen_insn_start(ctx->base.pc_next);
}

static bool hexagon_tr_breakpoint_check(DisasContextBase *dcbase, CPUState *cpu,
                                        const CPUBreakpoint *bp)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    tcg_gen_movi_tl(hex_gpr[HEX_REG_PC], ctx->base.pc_next);
    ctx->base.is_jmp = DISAS_NORETURN;
    gen_exception_debug();
    /*
     * The address covered by the breakpoint must be included in
     * [tb->pc, tb->pc + tb->size) in order to for it to be
     * properly cleared -- thus we increment the PC here so that
     * the logic setting tb->size below does the right thing.
     */
    ctx->base.pc_next += 4;
    return true;
}


static void hexagon_tr_translate_packet(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);
    CPUHexagonState *env = cpu->env_ptr;

    decode_and_translate_packet(env, ctx);

    if (ctx->base.is_jmp == DISAS_NEXT) {
        target_ulong page_start;

        page_start = ctx->base.pc_first & TARGET_PAGE_MASK;
        if (ctx->base.pc_next - page_start >= TARGET_PAGE_SIZE) {
            ctx->base.is_jmp = DISAS_TOO_MANY;
        }

        /*
         * The CPU log is used to compare against LLDB single stepping,
         * so end the TLB after every packet.
         */
        if (qemu_loglevel_mask(CPU_LOG_TB_CPU)) {
            ctx->base.is_jmp = DISAS_TOO_MANY;
        }
#if HEX_DEBUG
        /* When debugging, only put one packet per TB */
        ctx->base.is_jmp = DISAS_TOO_MANY;
#endif
    }
}

static void hexagon_tr_tb_stop(DisasContextBase *dcbase, CPUState *cpu)
{
    DisasContext *ctx = container_of(dcbase, DisasContext, base);

    switch (ctx->base.is_jmp) {
    case DISAS_TOO_MANY:
        tcg_gen_movi_tl(hex_gpr[HEX_REG_PC], ctx->base.pc_next);
        if (ctx->base.singlestep_enabled) {
            gen_exception_debug();
        } else {
            tcg_gen_exit_tb(NULL, 0);
        }
        break;
    case DISAS_NORETURN:
        break;
    default:
        g_assert_not_reached();
    }
}

static void hexagon_tr_disas_log(const DisasContextBase *dcbase, CPUState *cpu)
{
    qemu_log("IN: %s\n", lookup_symbol(dcbase->pc_first));
    log_target_disas(cpu, dcbase->pc_first, dcbase->tb->size);
}


static const TranslatorOps hexagon_tr_ops = {
    .init_disas_context = hexagon_tr_init_disas_context,
    .tb_start           = hexagon_tr_tb_start,
    .insn_start         = hexagon_tr_insn_start,
    .breakpoint_check   = hexagon_tr_breakpoint_check,
    .translate_insn     = hexagon_tr_translate_packet,
    .tb_stop            = hexagon_tr_tb_stop,
    .disas_log          = hexagon_tr_disas_log,
};

void gen_intermediate_code(CPUState *cs, TranslationBlock *tb, int max_insns)
{
    DisasContext ctx;

    translator_loop(&hexagon_tr_ops, &ctx.base, cs, tb, max_insns);
}

#define NAME_LEN               64
static char new_value_names[TOTAL_PER_THREAD_REGS][NAME_LEN];
#if HEX_DEBUG
static char reg_written_names[TOTAL_PER_THREAD_REGS][NAME_LEN];
#endif
static char new_pred_value_names[NUM_PREGS][NAME_LEN];
static char pred_written_names[NUM_PREGS][NAME_LEN];
static char store_addr_names[STORES_MAX][NAME_LEN];
static char store_width_names[STORES_MAX][NAME_LEN];
static char store_val32_names[STORES_MAX][NAME_LEN];
static char store_val64_names[STORES_MAX][NAME_LEN];

void hexagon_translate_init(void)
{
    int i;

    opcode_init();

    for (i = 0; i < TOTAL_PER_THREAD_REGS; i++) {
        hex_gpr[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, gpr[i]),
            hexagon_regnames[i]);

        sprintf(new_value_names[i], "new_%s", hexagon_regnames[i]);
        hex_new_value[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, new_value[i]),
            new_value_names[i]);

#if HEX_DEBUG
        sprintf(reg_written_names[i], "reg_written_%s", hexagon_regnames[i]);
        hex_reg_written[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, reg_written[i]),
            reg_written_names[i]);
#endif
    }
    for (i = 0; i < NUM_PREGS; i++) {
        hex_pred[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, pred[i]),
            hexagon_prednames[i]);

        sprintf(new_pred_value_names[i], "new_pred_%s", hexagon_prednames[i]);
        hex_new_pred_value[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, new_pred_value[i]),
            new_pred_value_names[i]);

        sprintf(pred_written_names[i], "pred_written_%s", hexagon_prednames[i]);
        hex_pred_written[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, pred_written[i]),
            pred_written_names[i]);
    }
    hex_next_PC = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, next_PC), "next_PC");
    hex_this_PC = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, this_PC), "this_PC");
    hex_slot_cancelled = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, slot_cancelled), "slot_cancelled");
    hex_branch_taken = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, branch_taken), "branch_taken");
    hex_dczero_addr = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, dczero_addr), "dczero_addr");
    llsc_addr = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, llsc_addr), "llsc_addr");
    llsc_val = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, llsc_val), "llsc_val");
    llsc_val_i64 = tcg_global_mem_new_i64(cpu_env,
        offsetof(CPUHexagonState, llsc_val_i64), "llsc_val_i64");
    hex_is_gather_store_insn = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, is_gather_store_insn),
        "is_gather_store_insn");
    hex_gather_issued = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, gather_issued), "gather_issued");
    hex_VRegs_updated_tmp = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, VRegs_updated_tmp), "VRegs_updated_tmp");
    hex_VRegs_updated = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, VRegs_updated), "VRegs_updated");
    hex_VRegs_select = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, VRegs_select), "VRegs_select");
    hex_QRegs_updated = tcg_global_mem_new(cpu_env,
        offsetof(CPUHexagonState, QRegs_updated), "QRegs_updated");
    for (i = 0; i < STORES_MAX; i++) {
        sprintf(store_addr_names[i], "store_addr_%d", i);
        hex_store_addr[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, mem_log_stores[i].va),
            store_addr_names[i]);

        sprintf(store_width_names[i], "store_width_%d", i);
        hex_store_width[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, mem_log_stores[i].width),
            store_width_names[i]);

        sprintf(store_val32_names[i], "store_val32_%d", i);
        hex_store_val32[i] = tcg_global_mem_new(cpu_env,
            offsetof(CPUHexagonState, mem_log_stores[i].data32),
            store_val32_names[i]);

        sprintf(store_val64_names[i], "store_val64_%d", i);
        hex_store_val64[i] = tcg_global_mem_new_i64(cpu_env,
            offsetof(CPUHexagonState, mem_log_stores[i].data64),
            store_val64_names[i]);
    }

    init_genptr();
}
