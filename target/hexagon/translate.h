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

#ifndef HEXAGON_TRANSLATE_H
#define HEXAGON_TRANSLATE_H

#include "qemu/bitmap.h"
#include "cpu.h"
#include "exec/translator.h"
#include "tcg/tcg-op.h"
#include "internal.h"

typedef struct DisasContext {
    DisasContextBase base;
    uint32_t num_packets;
    uint32_t num_insns;
    uint32_t num_hvx_insns;
    uint32_t num_hmx_insns;
    uint32_t mem_idx;
    int reg_log[REG_WRITES_MAX];
    int reg_log_idx;
    DECLARE_BITMAP(regs_written, TOTAL_PER_THREAD_REGS);    // mgl needs init
    int greg_log[GREG_WRITES_MAX];
    int greg_log_idx;
    int sreg_log[SREG_WRITES_MAX];
    int sreg_log_idx;
    int preg_log[PRED_WRITES_MAX];
    int preg_log_idx;
    DECLARE_BITMAP(pregs_written, NUM_PREGS);   // mgl needs init
    uint8_t store_width[STORES_MAX];
    int ctx_temp_vregs_idx;
    int ctx_temp_qregs_idx;
    int vreg_log[NUM_VREGS];
    int vreg_is_predicated[NUM_VREGS];
    int vreg_log_idx;
    DECLARE_BITMAP(vregs_updated_tmp, NUM_VREGS);   // mgl needs init
    DECLARE_BITMAP(vregs_updated, NUM_VREGS);       // mgl needs init
    DECLARE_BITMAP(vregs_select, NUM_VREGS);        // mgl needs init
    int qreg_log[NUM_QREGS];
    int qreg_is_predicated[NUM_QREGS];
    int qreg_log_idx;
    bool s1_store_processed;
} DisasContext;

static inline void ctx_log_reg_write(DisasContext *ctx, int rnum)
{
#if HEX_DEBUG
    if (test_bit(rnum, ctx->regs_written)) {
        HEX_DEBUG_LOG("WARNING: Multiple writes to r%d\n", rnum);
    }
#endif
    ctx->reg_log[ctx->reg_log_idx] = rnum;
    ctx->reg_log_idx++;
    set_bit(rnum, ctx->regs_written);
}

static inline void ctx_log_reg_write_pair(DisasContext *ctx, int rnum)
{
    ctx_log_reg_write(ctx, rnum ^ 0);
    ctx_log_reg_write(ctx, rnum ^ 1);
}

static inline void ctx_log_greg_write(DisasContext *ctx, int rnum)
{
    ctx->greg_log[ctx->greg_log_idx] = rnum;
    ctx->greg_log_idx++;
}


static inline void ctx_log_sreg_write(DisasContext *ctx, int rnum)
{
    ctx->sreg_log[ctx->sreg_log_idx] = rnum;
    ctx->sreg_log_idx++;
}

static inline void ctx_log_pred_write(DisasContext *ctx, int pnum)
{
    ctx->preg_log[ctx->preg_log_idx] = pnum;
    ctx->preg_log_idx++;
    set_bit(pnum, ctx->pregs_written);
}

static inline bool is_preloaded(DisasContext *ctx, int num)
{
    return test_bit(num, ctx->regs_written);
}

static inline void ctx_log_vreg_write(DisasContext *ctx,
                                      int rnum, VRegWriteType type,
                                      bool is_predicated)
{
    if (type != EXT_TMP) {
        ctx->vreg_log[ctx->vreg_log_idx] = rnum;
        ctx->vreg_is_predicated[ctx->vreg_log_idx] = is_predicated;
        ctx->vreg_log_idx++;

        set_bit(rnum, ctx->vregs_updated);
    }
    if (type == EXT_NEW) {
        set_bit(rnum, ctx->vregs_select);
    }
    if (type == EXT_TMP) {
        set_bit(rnum, ctx->vregs_updated_tmp);
    }
}

static inline void ctx_log_vreg_write_pair(DisasContext *ctx,
                                           int rnum, VRegWriteType type,
                                           bool is_predicated)
{
    ctx_log_vreg_write(ctx, rnum ^ 0, type, is_predicated);
    ctx_log_vreg_write(ctx, rnum ^ 1, type, is_predicated);
}

static inline void ctx_log_qreg_write(DisasContext *ctx,
                                      int rnum, int is_predicated)
{
    ctx->qreg_log[ctx->qreg_log_idx] = rnum;
    ctx->qreg_is_predicated[ctx->qreg_log_idx] = is_predicated;
    ctx->qreg_log_idx++;
}

extern TCGv hex_gpr[TOTAL_PER_THREAD_REGS];
extern TCGv hex_pred[NUM_PREGS];
extern TCGv hex_next_PC;
extern TCGv hex_this_PC;
extern TCGv hex_slot_cancelled;
extern TCGv hex_branch_taken;
extern TCGv hex_new_value[TOTAL_PER_THREAD_REGS];
extern TCGv hex_reg_written[TOTAL_PER_THREAD_REGS];
extern TCGv hex_new_pred_value[NUM_PREGS];
extern TCGv hex_pred_written;
extern TCGv hex_store_addr[STORES_MAX];
extern TCGv hex_store_width[STORES_MAX];
extern TCGv hex_store_val32[STORES_MAX];
extern TCGv_i64 hex_store_val64[STORES_MAX];
extern TCGv hex_dczero_addr;
extern TCGv hex_llsc_addr;
extern TCGv hex_llsc_val;
extern TCGv_i64 hex_llsc_val_i64;
extern TCGv hex_gather_issued;
extern TCGv hex_VRegs_updated_tmp;
extern TCGv hex_VRegs_updated;
extern TCGv hex_VRegs_select;
extern TCGv hex_QRegs_updated;
extern TCGv hex_greg[NUM_GREGS];
extern TCGv hex_greg_new_value[NUM_GREGS];
extern TCGv hex_greg_written[NUM_GREGS];
extern TCGv hex_t_sreg[NUM_SREGS];
extern TCGv hex_t_sreg_new_value[NUM_SREGS];
extern TCGv hex_t_sreg_written[NUM_SREGS];
extern TCGv hex_cache_tags[CACHE_TAGS_MAX];
extern TCGv hex_vstore_addr[VSTORES_MAX];
extern TCGv hex_vstore_size[VSTORES_MAX];
extern TCGv hex_vstore_pending[VSTORES_MAX];
#ifndef CONFIG_USER_ONLY
extern TCGv hex_slot;
extern TCGv hex_imprecise_exception;
#endif

static inline void gen_slot_cancelled_check(TCGv check, int slot_num)
{
    TCGv mask = tcg_const_tl(1 << slot_num);
    TCGv one = tcg_const_tl(1);
    TCGv zero = tcg_const_tl(0);

    tcg_gen_and_tl(mask, hex_slot_cancelled, mask);
    tcg_gen_movcond_tl(TCG_COND_NE, check, mask, zero, one, zero);

    tcg_temp_free(one);
    tcg_temp_free(zero);
    tcg_temp_free(mask);
}

static inline TCGv gen_read_sreg(TCGv result, int num)
{
    tcg_gen_mov_tl(result, hex_t_sreg[num]);
    return result;
}

static inline void gen_log_sreg_write(int snum, TCGv val)

{
    tcg_gen_mov_tl(hex_t_sreg_new_value[snum], val);
}

extern void gen_exception(int excp);
extern void gen_exception_end_tb(DisasContext *ctx, int excp);
extern void gen_exception_debug(void);
bool is_gather_store_insn(Insn *insn, Packet *pkt);

extern void gen_memcpy(TCGv_ptr dest, TCGv_ptr src, size_t n);

void process_store(DisasContext *ctx, Packet *pkt, int slot_num);
#endif
