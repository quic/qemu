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

#ifndef _HMX_MACROS_H
#define _HMX_MACROS_H
#include "q6v_defines.h"
#include "arch_options_calc.h"


#define sim_mem_read1(X, Y, ADDR) mem_read1(env, ADDR)
#define sim_mem_read2(X, Y, ADDR) mem_read2(env, ADDR)
#define sim_mem_read4(X, Y, ADDR) mem_read4(env, ADDR)
#define sim_mem_read8(X, Y, ADDR) mem_read8(env, ADDR)
#define tmp_Insn Insn;

#define fMX_NONE()
#define fMX_HMX_FLT()
#define fMX_HMX_FLT_EXP()
#define fMX_SET_PKTID(STATE,THREAD) STATE->pktid = THREAD->pktid
#define fMX_SUB_COLS()
#define fMX_SHL16_ACCUMULATOR_FXP(ROW,COL,SELECT) { THREAD2HMXSTRUCT->future_accum_fxp[ROW][COL].w[SELECT+0] <<= 16; THREAD2HMXSTRUCT->future_accum_fxp[ROW][COL].w[SELECT+2] <<= 16; }
#define fMX_CLEAR_BOTH_ACC_FXP() { THREAD2HMXSTRUCT->fxp_commit_state.acc_clear_both = 1; /* if (thread->processor_ptr->verif_en) fMX_DEBUG_LOG(2, "HMX ACC CLEAR : FXP ACC BOTH pktid=0x%08x", thread->pktid ); */ }
#define fMX_CLEAR_BOTH_ACC_FLT() { THREAD2HMXSTRUCT->flt_commit_state.acc_clear_both = 1; /* if (thread->processor_ptr->verif_en) fMX_DEBUG_LOG(2, "HMX ACC CLEAR : FLT ACC BOTH pktid=0x%08x", thread->pktid ); */ }
#define fMX_SWAP_ACC_FXP() { THREAD2HMXSTRUCT->fxp_commit_state.swap_acc = 1; /* if (thread->processor_ptr->verif_en) fMX_DEBUG_LOG(2, "HMX ACC FXP SWAP : pktid=0x%08x", thread->pktid ); */ }
#define fMX_SWAP_ACC_FLT() { THREAD2HMXSTRUCT->flt_commit_state.swap_acc = 1; /* if (thread->processor_ptr->verif_en) fMX_DEBUG_LOG(2, "HMX ACC FLT SWAP : pktid=0x%08x", thread->pktid ); */ }
#define fMX_NOCLEAR_ACC()
#define fMX_CVT_WR_XLATE(EA, START, RANGE, ACCESS_TYPE) { hmx_mxmem_wr_xlate(thread, insn->slot, EA, START, RANGE, ACCESS_TYPE); if (EXCEPTION_DETECTED) return; }
#define fMX_WGT_XLATE(START, RANGE, WEIGHTS_PER_BYTE_LOG, OUTPUT_CH_SCALE) hmx_wgt_init(thread, START, RANGE, insn->slot, WEIGHTS_PER_BYTE_LOG, OUTPUT_CH_SCALE)
#define fMX_ACT_XLATE(START, RANGE) hmx_act_init(thread, START, RANGE, insn->slot)
#define fMX_CVT(STATE, RS, TYPE, SUBCHANNEL_SELECT, CVT_TYPE) hmx_acc_convert(STATE, RS, TYPE, SUBCHANNEL_SELECT, CVT_TYPE)
#define fMX_NOCLEAR_ACCUMULATOR()
#define fMX_GET_HMX_STATE(STATE) hmx_state_t * STATE = THREAD2HMXSTRUCT
#define fMX_MULTIPLY(STATE,WEIGHTS_PER_BYTE_LOG,WGT_PER_WORD,UNPACK,TYPE,MULT_TYPE,OUTPUT_CHANNEL_SCALE) { CALL_HMX_CMD(hmx_multiply,env, STATE, WEIGHTS_PER_BYTE_LOG, WGT_PER_WORD, UNPACK, TYPE, MULT_TYPE, OUTPUT_CHANNEL_SCALE); }
#define fMX_ACT_PARAMETERS(STATE, RSV, RTV, TYPE, FORMAT_OFFSET, BLOCK_TYPE) { CALL_HMX_CMD(hmx_act_paramaters,STATE, RSV, RTV, insn->slot, TYPE, FORMAT_OFFSET, BLOCK_TYPE); }
#define fMX_WGT_PARAMETERS(STATE, RSV, RTV, TYPE, BLOCK_TYPE, WEIGHTS_PER_BYTE_LOG, OUTPUT_CHANNEL_SCALE, UNPACK, USR) { CALL_HMX_CMD(hmx_wgt_paramaters,STATE, RSV, RTV, insn->slot, TYPE, BLOCK_TYPE, WEIGHTS_PER_BYTE_LOG, OUTPUT_CHANNEL_SCALE, UNPACK, USR); }
#define fMX_CVT_TX_PARAMETERS(STATE, USR, TYPE, FB_DST) { CALL_HMX_CMD(hmx_cvt_tx_parameters,STATE, USR, TYPE, FB_DST); }
#define fMX_CVT_WR_PARAMETERS(STATE, RSV, RTV, TYPE, FORMAT_OFFSET, DIRECTION) { CALL_HMX_CMD(hmx_cvt_mem_parameters,STATE, RSV, RTV, TYPE, FORMAT_OFFSET, DIRECTION); }
#define fMX_MAC_TIMING_MODE_INFO(THREAD, STATE) { if (THREAD->bq_on) { hmx_populate_hmx_mac_maptr(THREAD, STATE); return; } }
#define fMX_CVT_TIMING_MODE_INFO(THREAD, STATE) { if (THREAD->bq_on) { hmx_populate_hmx_maptr(THREAD, STATE); return; } }
#define fDEBUG_VERIF_ACC_PRINT(FLT) { /* if (!state_ptr->is_flt) { for(int row_idx = 0; row_idx < state_ptr->QDSP6_MX_ROWS; row_idx++) { for(int col_idx = 0; col_idx < state_ptr->QDSP6_MX_COLS; col_idx++) { if (state_ptr->redundant_acc) { DEBUG_PRINT(1, "FINAL HMX MULT: FXP ACC[%02d][%02d] [0].hi=0x%08x .lo=0x%08x  [1].hi=0x%08x .lo=0x%08x", row_idx, col_idx, state_ptr->future_accum_fxp[row_idx][col_idx].w[0], state_ptr->future_accum_fxp[row_idx][col_idx].w[2], state_ptr->future_accum_fxp[row_idx][col_idx].w[1], state_ptr->future_accum_fxp[row_idx][col_idx].w[3]); } else { DEBUG_PRINT(1, "FINAL HMX MULT: FXP ACC[%02d][%02d] [0]=0x%08x [1]=0x%08x", row_idx, col_idx, state_ptr->future_accum_fxp[row_idx][col_idx].w[0],state_ptr->future_accum_fxp[row_idx][col_idx].w[1]); } } } } else { for(int row_idx = 0; row_idx < state_ptr->QDSP6_MX_ROWS; row_idx+=2) { for(int col_idx = 0; col_idx < state_ptr->QDSP6_MX_COLS; col_idx++) { DEBUG_PRINT(1, "FINAL HMX MULT: FLT ACC[%02d][%02d] [0].exp=0x%04x .sig=%08x .ovf=%d [1].exp=0x%04x .sig=%08x .ovf=%d", row_idx>>1, col_idx, (uint16_t)state_ptr->future_accum_flt[row_idx][col_idx].xfp[0].exp, (int32_t)state_ptr->future_accum_flt[row_idx][col_idx].xfp[0].sig, (int16_t)state_ptr->future_accum_flt[row_idx][col_idx].xfp[0].status.inf, (uint16_t)state_ptr->future_accum_flt[row_idx][col_idx].xfp[1].exp, (int32_t)state_ptr->future_accum_flt[row_idx][col_idx].xfp[1].sig, (int16_t)state_ptr->future_accum_flt[row_idx][col_idx].xfp[1].status.inf); } } } */ }
#define fMX_GETCHANNELSIZE(PROC) get_hmx_channel_size(PROC)
#define fMX_OPERAND_READY() { int hmx_power_off = (thread->processor_ptr->arch_proc_options->hmx_power_config == 0) ? thread->bq_on : 0; if ((THREAD2HMXSTRUCT->operand_ready!=3) || hmx_power_off) { if (THREAD2HMXSTRUCT->y_tap == 0) { thread->mem_access[1].cancelled = 1; thread->mem_access[0].cancelled = 1; thread->mem_access[1].valid = 0; thread->mem_access[0].valid = 0; } return; } if (EXCEPTION_DETECTED) return; }
#define fMX_CHECK_OPERANDS_SETUP_ACC() { fMX_OPERAND_READY(); if (!THREAD2HMXSTRUCT->is_flt) { memcpy(THREAD2HMXSTRUCT->future_accum_fxp, THREAD2HMXSTRUCT->accum_fxp, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL); } else { memcpy(THREAD2HMXSTRUCT->future_accum_flt, THREAD2HMXSTRUCT->accum_flt, sizeof(hmx_acc_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL); } }
#define fMX_DONOTHING(VAL) { VAL = VAL; }
#define UNLIKELY(X) __builtin_expect((X), 0)
#define fVDOCHKPAGECROSS(BASE,SUM) if (UNLIKELY(thread->timing_on)) { thread->mem_access[slot].check_page_crosses = 1; thread->mem_access[slot].page_cross_base = BASE; thread->mem_access[slot].page_cross_sum = SUM; }
#define fALIGN_CVT(ADDR, FORMAT) ADDR &= (~((1<<(fMX_GETCHANNELSIZE(thread->processor_ptr)+2))-1)) << FORMAT
#define fALIGN_FXP_DEPTH(ADDR,SCALE) ADDR &= ~((thread->processor_ptr->arch_proc_options->QDSP6_MX_COLS*4*SCALE) - 1)
#define fMX_LOAD_BIAS_LO(OUT_IDX,EA,BIAS_IDX,PA) { paddr_t pa = PA+4*OUT_IDX; size4u_t bias_word = sim_mem_read4(thread->system_ptr, thread->threadId, pa); THREAD2HMXSTRUCT->future_bias[BIAS_IDX][OUT_IDX].val[0] = bias_word; THREAD2HMXSTRUCT->future_bias[BIAS_IDX][OUT_IDX].val[1] = 0; fMX_DEBUG_LOG(2,"Bias Loaded[%d][%d][0]=%08x", BIAS_IDX, OUT_IDX, bias_word); /* if (thread->processor_ptr->verif_en){ int slot_tmp = thread->cur_slot; thread->cur_slot = 0; if (thread->processor_ptr->options->sim_vtcm_memory_callback) { thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, 4, DREAD, bias_word); } thread->cur_slot = slot_tmp; warn("hmx bias lo loaded: bias_idx: %d output_channel: %x PA=%llx val=%x", BIAS_IDX, OUT_IDX, pa, bias_word); } */ }
#define fMX_LOAD_BIAS_HI(OUT_IDX,EA,BIAS_IDX,PA) { paddr_t pa = PA+4*OUT_IDX + 128; size4u_t bias_word = sim_mem_read4(thread->system_ptr, thread->threadId, pa); THREAD2HMXSTRUCT->future_bias[BIAS_IDX][OUT_IDX].val[1] = bias_word; fMX_DEBUG_LOG(2,"Bias Loaded[%d][%d][1]=%08x", BIAS_IDX, OUT_IDX, bias_word); /* if (thread->processor_ptr->verif_en) { int slot_tmp = thread->cur_slot; thread->cur_slot = 0; if (thread->processor_ptr->options->sim_vtcm_memory_callback) { thread->processor_ptr->options->sim_vtcm_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId, 0, pa, 4, DREAD, bias_word); } thread->cur_slot = slot_tmp; warn("hmx bias hi loaded: bias_idx: %d output_channel: %x PA=%llx val=%x", BIAS_IDX, OUT_IDX, pa, bias_word); } */ }
#define fMX_STORE_BIAS(OUT_IDX,BIAS_IDX,EA) {}
#define fMX_STORE_BIAS_DOUBLE(OUT_IDX,BIAS_IDX,EA) {}
#define fMX_GET_ACC_INDEX(IDX,Q_OFFSET) { unsigned int temp = (IDX >> fMX_GETCHANNELSIZE(thread->processor_ptr)) & ~((1<<Q_OFFSET)-1); IDX = temp | (IDX & ((1<<Q_OFFSET)-1)); }
#define CHECK_ACCESS_RANGE(EXC,PADDR,LEN) do { \
    vaddr_t page_mask = (1ULL<<22)-1; \
    paddr_t page_of_start = (PADDR & ~page_mask); \
    paddr_t page_of_end = ((PADDR + LEN) & ~page_mask); \
    EXC |= (page_of_start != page_of_end); \
} while (0)
#define fMX_HIDEHTML()
#endif
