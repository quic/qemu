/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#ifndef _HMX_MACROS_H
#define _HMX_MACROS_H

#include "q6v_defines.h"

#define sim_mem_read1(X, Y, ADDR) mem_read1(env, ADDR)
#define sim_mem_read2(X, Y, ADDR) mem_read2(env, ADDR)
#define sim_mem_read4(X, Y, ADDR) mem_read4(env, ADDR)

typedef struct {
    int slot;
} tmp_insn_t;

#define fMX_SELECT_ACCUMULATOR_FXP(ROW,COL,SELECT) THREAD2STRUCT->accum_fxp[ROW][COL].val[SELECT]
#define fMX_SHL16_ACCUMULATOR_FXP(ROW,COL,SELECT) THREAD2STRUCT->future_accum_fxp[ROW][COL].val[SELECT] <<= 16
#define fMX_SELECT_ACCUMULATOR_FLT(ACC,OVF,ROW,COL,SELECT) { ACC=THREAD2STRUCT->accum_flt[ROW][COL].val[SELECT]; OVF=THREAD2STRUCT->accum_flt[ROW][COL].ovf[SELECT]; fMX_DEBUG_LOG(2, "HMX CVT ACC[%02d][%02d][%02d] = %016llx.%016llx OVF=%x", ROW>>1, COL, SELECT, ACC.hi, ACC.lo, OVF); }
#define fMX_CONVERTED_ACCUMULATOR_FXP(ROW,COL,VAL) THREAD2STRUCT->future_accum_fxp[ROW][COL].ub[0] = (size1u_t)VAL
#define fMX_CONVERTED_ACCUMULATOR_FLT(ROW,COL,VAL) THREAD2STRUCT->future_accum_flt[ROW][COL].val[0].w[0] = (size4u_t)VAL
#define fMX_CLEAR_ACC_FXP() THREAD2STRUCT->fxp_commit_state.acc_clear = 1
#define fMX_CLEAR_ACC_FLT() THREAD2STRUCT->flt_commit_state.acc_clear = 1
#define fMX_CLEAR_BOTH_ACC_FXP() { processor_t * proc = thread->processor_ptr; THREAD2STRUCT->fxp_commit_state.acc_clear_both = 1; INC_PSTAT(phmxcvt_clr); }
#define fMX_CLEAR_BOTH_ACC_FLT() { processor_t * proc = thread->processor_ptr; THREAD2STRUCT->flt_commit_state.acc_clear_both = 1; INC_PSTAT(phmxcvt_clr); }
#define fMX_SWAP_ACC_FXP() THREAD2STRUCT->fxp_commit_state.swap_acc = 1
#define fMX_SWAP_ACC_FLT() THREAD2STRUCT->flt_commit_state.swap_acc = 1
#define fMX_NOCLEAR_ACC()
#define fMX_ACT_DESC()
#define fMX_ACTIVATION_INIT(START, RANGE, TYPE, FORMAT,BLOCK_TYPE ) hmx_activation_init(thread, START, RANGE, insn->slot, TYPE, FORMAT, BLOCK_TYPE)
#define fMX_WEIGHT_INIT(START, RANGE, TYPE, BLOCK_TYPE, WEIGHTS_PER_BYTE_LOG) hmx_weight_init(thread, START, RANGE, insn->slot, TYPE, BLOCK_TYPE, WEIGHTS_PER_BYTE_LOG)
#define fMX_fVSATUB(OUT, IN) OUT = fVSATUB(IN);
#define fMX_fVSATUH(OUT, IN) OUT = fVSATUH(IN);
#define fMX_UPDATE_ARRAY_PMU(FLT, BLOCK)
#define fMX_UPDATE_CVT_ARRAY_PMU(FLT)
#define fMX_NOCLEAR_ACCUMULATOR()
#define fMX_NO_SHIFT_ACC(EVEN) {}
#define fMX_MULT(ROW,COL,ACC_SELECT,ACTIVATION,WEIGHT,WEIGHT_VALID,FLT,INPUT_CHANNEL,X_TAP,Y_TAP,BLOCK,DEEP_BLOCK) { if ((!THREAD2STRUCT->limit_execeeded) && (WEIGHT_VALID)) { if (ROW>=0) { if (FLT) { THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT] = hmx_fp16_mac(thread->processor_ptr, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT], &THREAD2STRUCT->future_accum_flt[ROW][COL].ovf[ACC_SELECT], THREAD2STRUCT->usr_fp, ACTIVATION, WEIGHT); int opt = thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_RATE-1; if ((INPUT_CHANNEL & opt) == opt) { hmx_fp16_acc_ovf_check(thread->processor_ptr, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT], &THREAD2STRUCT->future_accum_flt[ROW][COL].ovf[ACC_SELECT], THREAD2STRUCT->usr_fp); } if (thread->processor_ptr->options->hmx_mac_flt_callback) { thread->processor_ptr->options->hmx_mac_flt_callback(thread->system_ptr, thread->processor_ptr, thread->pktid, (ROW>>1), COL, ACC_SELECT, INPUT_CHANNEL, WEIGHT, ACTIVATION, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT].hi, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT].lo, THREAD2STRUCT->future_accum_flt[ROW][COL].ovf[ACC_SELECT], X_TAP, Y_TAP, BLOCK, DEEP_BLOCK); } fMX_DEBUG_LOG(2, "HMX FLT MULT: A: %08x (%4.6f) W: %08x (%4.6f) = ACC[%02d][%02d][%02d] = %016llx.%016llx (%8.8f) ovf=%02x", (size2u_t)ACTIVATION, hmx_fp16_float(ACTIVATION), (size2s_t)WEIGHT, hmx_fp16_float(WEIGHT), ROW>>1, COL, ACC_SELECT, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT].hi, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT].lo, hmx_acc_double(THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT]), THREAD2STRUCT->future_accum_flt[ROW][COL].ovf[ACC_SELECT]); fMX_VERIF_DEBUG_LOG(1, "HMX FLT MULT: pktid:%08x  A: %08x W: %08x ACC[%02d][%02d][%02d]: %016llx.%016llx (%016llx) ovf: %02x ", thread->pktid, (size2u_t)ACTIVATION, (size2s_t)WEIGHT, ROW>>1, COL, ACC_SELECT, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT].hi, THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT].lo, cast16s_to_8s(shiftr128(THREAD2STRUCT->future_accum_flt[ROW][COL].val[ACC_SELECT], 16+66 - (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC+thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT))), THREAD2STRUCT->future_accum_flt[ROW][COL].ovf[ACC_SELECT]); } else { THREAD2STRUCT->future_accum_fxp[ROW][COL].w[ACC_SELECT] += fMPY8US(ACTIVATION, WEIGHT); if (thread->processor_ptr->options->hmx_mac_fxp_callback) { thread->processor_ptr->options->hmx_mac_fxp_callback(thread->system_ptr, thread->processor_ptr, thread->pktid, ROW, COL, ACC_SELECT, INPUT_CHANNEL, WEIGHT, ACTIVATION, THREAD2STRUCT->future_accum_fxp[ROW][COL].w[ACC_SELECT], X_TAP, Y_TAP, BLOCK, DEEP_BLOCK); } fMX_DEBUG_LOG(2, "HMX FXP MULT: A: %08x (%4d) W: %08x (%4d) = ACC[%02d][%02d][%02d] = %08x (%11d)", (size1u_t)ACTIVATION, (size1u_t)ACTIVATION, (size1s_t)WEIGHT, (size1s_t)WEIGHT, ROW, COL, ACC_SELECT, THREAD2STRUCT->future_accum_fxp[ROW][COL].w[ACC_SELECT], THREAD2STRUCT->future_accum_fxp[ROW][COL].w[ACC_SELECT]); fMX_VERIF_DEBUG_LOG(1, "HMX FXP MULT: pktid:%08x A: %08x (%4d) W: %08x (%4d) = ACC[%02d][%02d][%02d] = %08x (%11d)",thread->pktid, (size1u_t)ACTIVATION, (size1u_t)ACTIVATION, (size1s_t)WEIGHT, (size1s_t)WEIGHT, ROW, COL, ACC_SELECT, THREAD2STRUCT->future_accum_fxp[ROW][COL].w[ACC_SELECT], THREAD2STRUCT->future_accum_fxp[ROW][COL].w[ACC_SELECT]); } } else { fMX_DEBUG_LOG(2, "HMX MULT: Dropped due to rollover for row=%d", ROW>>FLT); fMX_VERIF_DEBUG_LOG(1, "HMX MULT: Dropped due to rollover for row=%d", ROW>>FLT); } } else { fMX_DEBUG_LOG(2, "HMX MULT: Dropped due to cycle or weight limit exceed or invalid weight=%d", WEIGHT_VALID); fMX_VERIF_DEBUG_LOG(1, "HMX MULT: Dropped due to cycle or weight limit exceed or invalid weight=%d", WEIGHT_VALID); } }
#define fMX_MULT_ARRAY_PMU(SPATIAL,INPUT_CHANNEL,OUTPUT_CHANNEL,ACTIVATION,WEIGHT,FLT) { if (thread->processor_ptr->arch_proc_options->pmu_enable) { if (SPATIAL>=0) { int val = !( ((size2u_t)ACTIVATION == 0x0000) || ((size2u_t)WEIGHT == 0x0000) ); if (FLT) { val &= !( ((size2u_t)ACTIVATION == 0x8000) || ((size2u_t)WEIGHT == 0x8000) ); } THREAD2STRUCT->mpy_matrix[SPATIAL][INPUT_CHANNEL][OUTPUT_CHANNEL] = val; } } }
#define fDEBUG_VERIF_ACC_PRINT(FLT) { }
#define fMX_GETCHANNELSIZE(PROC) get_hmx_channel_size(PROC)
#define fMX_GETADDRMASK(PROC) ~((1<<(fMX_GETCHANNELSIZE(PROC) + PROC->arch_proc_options->hmx_spatial_size))-1)
#define fMX_GETMASK(OUTPUT,START,Q,FP) { unsigned int spatial_mask = (((1<<thread->processor_ptr->arch_proc_options->hmx_spatial_size) - 1) << fMX_GETCHANNELSIZE(thread->processor_ptr)); unsigned int q_mask = ((1<<Q)-1) ; spatial_mask &= ~(q_mask << fMX_GETCHANNELSIZE(thread->processor_ptr)); spatial_mask |= q_mask; OUTPUT = (START & spatial_mask); }
#define fMX_MASKED_INC(OUTPUT,MASK) { OUTPUT = MASK & (~MASK+1); }
#define fMX_MASKED_INC(OUTPUT,MASK) { OUTPUT = MASK & (~MASK+1); }
#define fMX_INC_MASKED(IN,INC,MASK) (((IN | ~MASK) + (INC & MASK)) & MASK) | (IN & ~MASK)
#define fMX_INC_MASKED_OVERFLOW(OVERFLOW, OUT, IN,INC,MASK) { OVERFLOW = IN; OUT = (((IN | ~MASK) + (INC & MASK)) & MASK) | (IN & ~MASK); OVERFLOW = OUT < OVERFLOW; }
#define fMX_INC_TAP_MASKED(VAL,INC,MASK,DILATE) { VAL = fMX_INC_MASKED(VAL, INC, MASK); if (INC == 0) { VAL = -1; } else if (DILATE && (VAL>=0)) { VAL = fMX_INC_MASKED(VAL, INC, MASK); } }
#define fMX_CHECK_MAC_LIMIT(Y_TAP, X_TAP, BLOCK, BLOCK_END, INPUT_CH, INPUT_END, DEEP_BLOCK, DEEP_BLOCK_END) { if(--THREAD2STRUCT->mac_cycle_limit == 0 ) { warn("HMX Last MAC cycle. Stopping on xtap=0x%x ytap=0x%x channel=0x%x (of 0x%x) block=0x%x.", Y_TAP, X_TAP,INPUT_CH, INPUT_END, BLOCK ); Y_TAP = -1; X_TAP = -1; BLOCK = BLOCK_END; INPUT_CH = INPUT_END; DEEP_BLOCK = DEEP_BLOCK_END; } }
#define fMX_WEIGHTS_RANGE_CHECK(ADDR,TAP,VAL) { if (ADDR > (THREAD2STRUCT->max_weight_pa+1)) { TAP = VAL; THREAD2STRUCT->limit_execeeded = 1; warn("HMX exceeded maximum weights range, stopping HMX processing PA=%llx MAX=%llx", ADDR, THREAD2STRUCT->max_weight_pa); } }
#define fMX_COMPUTER_FILTER_SIZE(SIZE,START,MASK, MASK_INC) { unsigned int tmp = START & MASK; unsigned int done = 0; unsigned int count = 0; while(!done && tmp) { fMX_INC_MASKED_OVERFLOW(done, count, count, MASK_INC, MASK); done = (tmp == count) ? 1 : done; SIZE++; } SIZE++; }
#define fMX_COMPUTER_FILTER_SIZE_ABOVE(SIZE,START,MASK, MASK_INC) { unsigned int done = 0; unsigned int count = START & MASK; while(!done) { fMX_INC_MASKED_OVERFLOW(done, count, count, MASK_INC, MASK); if (!done) SIZE++; } SIZE++; }
#define fMX_INCREMENT_WEIGHT_ADDR(WGT_IDX, WGT_ADDR, WGT_INC, WEIGHT_SCALE) { WGT_IDX = (WGT_IDX + 1) & ((4*(1<<WEIGHT_SCALE))-1); WGT_ADDR = (WGT_IDX==0) ? WGT_ADDR + WGT_INC : WGT_ADDR; }
#define fMXDEBUG_PRINT_TAP(X_TAP,Y_TAP,CH,DEEP_BLOCK) { int x; int y; COMPUTE_MASKED_VAL(x,X_TAP,THREAD2STRUCT->tile_x_mask,12); COMPUTE_MASKED_VAL(y,Y_TAP,THREAD2STRUCT->tile_y_mask,12); fMX_DEBUG_LOG(2, "MX MULT: x_tap=%d y_tap=%d input_channel=%d deep_block=%d: ", x, y, CH,DEEP_BLOCK); fMX_VERIF_DEBUG_LOG(2, "MX MULT: x_tap=%d y_tap=%d input_channel=%d deep_block=%d: ", x, y, CH,DEEP_BLOCK); }
#define fMXDEBUG_PRINT_CH(BLOCK_IDX,CH_START,CH_END,DEEP_END) { fMX_DEBUG_LOG(2, "MX MULT: Block %d Channel Range: [0x%x, 0x%x] Deep Block Count=%d", BLOCK_IDX, CH_START, CH_END, DEEP_END); fMX_VERIF_DEBUG_LOG(2, "MX MULT: Block %d Channel Range: [0x%x, 0x%x] Deep Block Count=%d", BLOCK_IDX, CH_START, CH_END, DEEP_END); }
#define fMX_OPERAND_READY() { int hmx_power_off = (thread->processor_ptr->arch_proc_options->hmx_power_config == 0) ? thread->bq_on : 0; if ((THREAD2STRUCT->operand_ready!=3) || hmx_power_off) { if (thread->mem_access[1].hmx_ma.y_tap == 0) { thread->mem_access[1].cancelled = 1; thread->mem_access[0].cancelled = 1; thread->mem_access[1].valid = 0; thread->mem_access[0].valid = 0; } return; } if (EXCEPTION_DETECTED) return; }
#define fMX_CHECK_OPERANDS_SETUP_ACC() { fMX_OPERAND_READY(); if (!thread->mem_access[1].hmx_ma.flt) { memcpy(THREAD2STRUCT->future_accum_fxp, THREAD2STRUCT->accum_fxp, sizeof(hmx_acc_fxp_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL); } else { memcpy(THREAD2STRUCT->future_accum_flt, THREAD2STRUCT->accum_flt, sizeof(hmx_acc_flt_t)*MAX_ACCUMULATORS_DEPTH*MAX_ACCUMULATORS_SPATIAL); } }
#define fMX_TRUNCATE_TO_12B(VAL) { VAL = ((VAL << 52) >> 52); }
#define fMX_DONOTHING(VAL) { VAL = VAL; }
#define fVDOCHKPAGECROSS(BASE,SUM) if (UNLIKELY(thread->timing_on)) { thread->mem_access[slot].check_page_crosses = 1; thread->mem_access[slot].page_cross_base = BASE; thread->mem_access[slot].page_cross_sum = SUM; }
#define fMX_RNDSAT_ACC(OUT, ACC, OVF, BIAS, EXP_D, SAT_EXP, SAT, TYPE, SPATIAL_IDX, CHANNEL_IDX, ACC_IDX) { THREAD2STRUCT->array_cvt += ((ACC.lo != 0) || (ACC.hi != 0)); OUT = hmx_fp16_cvt(thread->processor_ptr, ACC, OVF, THREAD2STRUCT->usr_fp, BIAS, EXP_D, SAT_EXP, SAT, TYPE==HMX_FP_CVT_HI, TYPE==HMX_FP_CVT_LO); fMX_DEBUG_LOG(2, "HMX_CVT SAT=%d USR=%x: ACC[%02d][%02d][%02d] = %016llx OVF=%d  FP16 OUT=%04x pktid:%08x", (int)SAT, THREAD2STRUCT->usr_fp, (int)(SPATIAL_IDX/2), (int)CHANNEL_IDX, (int)ACC_IDX, cast16s_to_8s(shiftr128(THREAD2STRUCT->accum_flt[SPATIAL_IDX][CHANNEL_IDX].val[ACC_IDX], 16+66 - (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC+thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT))),OVF, OUT, thread->pktid ); fMX_VERIF_DEBUG_LOG(2, "HMX_CVT SAT=%d USR=%x: ACC[%02d][%02d][%02d] = %016llx OVF=%d  FP16 OUT=%04x pktid:%08x", (int)SAT, THREAD2STRUCT->usr_fp, (int)(SPATIAL_IDX/2), (int)CHANNEL_IDX, (int)ACC_IDX, cast16s_to_8s(shiftr128(THREAD2STRUCT->accum_flt[SPATIAL_IDX][CHANNEL_IDX].val[ACC_IDX], 16+66 - (thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_FRAC+thread->processor_ptr->arch_proc_options->QDSP6_MX_FP_ACC_INT))),OVF, OUT, thread->pktid ); }
#define fMX_RNDSAT_ACC_U8(OUT, ACC, BIAS32, EXP, SCALE, RND, SAT, SPATIAL_IDX, CHANNEL_IDX, ACC_IDX) { THREAD2STRUCT->array_cvt += (ACC>0); OUT = hmx_u8_cvt(thread->processor_ptr, ACC, BIAS32, EXP, SCALE, RND, SAT, thread->processor_ptr->arch_proc_options->hmx_v1); fMX_DEBUG_LOG(2, "HMX_CVT: ACC[%02d][%02d][%02d] = %08llx BYTE OUT=%02x pktid:%08x", SPATIAL_IDX, CHANNEL_IDX, ACC_IDX, ACC, OUT, thread->pktid ); fMX_VERIF_DEBUG_LOG(2, "HMX_CVT: ACC[%02d][%02d][%02d] = %08llx BYTE OUT=%02x pktid:%08x", SPATIAL_IDX, CHANNEL_IDX, ACC_IDX, ACC, OUT, thread->pktid ); }
#define fMX_RNDSAT_ACC_U16(OUT, ACC_HL, ACC_LL, BIAS32, EXP, SCALE, RND, SAT, SPATIAL_IDX, CHANNEL_IDX, ACC_IDX) { THREAD2STRUCT->array_cvt += ((ACC_HL>0) + (ACC_LL>0)) ; OUT = hmx_u16_cvt(thread->processor_ptr, ACC_HL, ACC_LL, BIAS32, EXP, SCALE, RND, SAT, thread->processor_ptr->arch_proc_options->QDSP6_MX_CVT_MPY_SZ); fMX_DEBUG_LOG(2, "HMX_CVT: ACC[%02d][%02d][%02d] = HL: %08llx LL: %08llx HALF OUT=%02x pktid:%08x", SPATIAL_IDX, CHANNEL_IDX, ACC_IDX, ACC_HL, ACC_LL, OUT, thread->pktid ); fMX_VERIF_DEBUG_LOG(2, "HMX_CVT: ACC[%02d][%02d][%02d] = HL: %08llx LL: %08llx HALF OUT=%02x pktid:%08x", SPATIAL_IDX, CHANNEL_IDX, ACC_IDX, ACC_HL, ACC_LL, OUT, thread->pktid ); }
#define fMX_RNDSAT_ACC_U16x16(OUT, ACC_HH, ACC_HL, ACC_LH, ACC_LL, BIAS48, EXP, SCALE, RND, SAT, SPATIAL_IDX, CHANNEL_IDX, ACC_IDX) { OUT = hmx_u16x16_cvt(thread->processor_ptr, ACC_HH, ACC_HL, ACC_LH, ACC_LL, BIAS48, EXP, SCALE, RND, SAT, thread->processor_ptr->arch_proc_options->hmx_v1); THREAD2STRUCT->array_cvt += (ACC_LH>0) + (ACC_LL>0) + (ACC_HH>0) + (ACC_HL>0) ; fMX_DEBUG_LOG(2, "HMX_CVT: ACC[%02d][%02d][%02d] = HH: %08llx HL: %08llx LH: %08llx LL: %08llx HALF OUT=%02x pktid:%08x", SPATIAL_IDX, CHANNEL_IDX, ACC_IDX, ACC_HH, ACC_HL, ACC_LH, ACC_LL, OUT, thread->pktid ); fMX_VERIF_DEBUG_LOG(2, "HMX_CVT: ACC[%02d][%02d][%02d] = HH: %08llx HL: %08llx LH: %08llx LL: %08llx HALF OUT=%02x pktid:%08x", SPATIAL_IDX, CHANNEL_IDX, ACC_IDX, ACC_HH, ACC_HL, ACC_LH, ACC_LL, OUT, thread->pktid ); }
#define COMPUTE_MASKED_VAL(VAL,VAL_MASK,MASK,SIZE) { VAL = 0; int j = 0; for (int i = 0; i < SIZE; i++) { if ((MASK >> i) & 0x1) { if (((VAL_MASK & MASK) >> i) & 0x1) { VAL += (1 << j); } j++; } } }
#define fMXLOAD_ACTIVATION(ACTIVATION,X_TAP_IDX,Y_TAP_IDX,X_TILE_IDX,Y_TILE_IDX,IN_CHANNEL_IDX,BLOCK_IDX,Y_TILE_MASK,FLT) { if (!THREAD2STRUCT->limit_execeeded) { int intra_tile_y_tmp = 0; int y_next_tile = 0; if ((X_TAP_IDX >= 0) && (Y_TAP_IDX >= 0)) { fMX_INC_MASKED_OVERFLOW(y_next_tile, intra_tile_y_tmp, Y_TAP_IDX, Y_TILE_IDX, Y_TILE_MASK); intra_tile_y_tmp = intra_tile_y_tmp & 0x7FFFFFFF; } else { y_next_tile = 1; } paddr_t act_pa_offset =0; act_pa_offset += intra_tile_y_tmp; act_pa_offset += X_TILE_IDX; act_pa_offset += y_next_tile * thread->mem_access[1].hmx_ma.dY; act_pa_offset += (IN_CHANNEL_IDX); act_pa_offset += (BLOCK_IDX * thread->processor_ptr->arch_proc_options->hmx_block_size); paddr_t act_pa = thread->mem_access[1].paddr; if (FLT) { ACTIVATION = sim_mem_read2(thread->system_ptr, thread->threadId, act_pa + act_pa_offset); } else { ACTIVATION = sim_mem_read1(thread->system_ptr, thread->threadId, act_pa + act_pa_offset); } if ( (thread->processor_ptr->arch_proc_options->hmxdebuglvl >= 2) && (!thread->bq_on) ) { int x; int y; COMPUTE_MASKED_VAL(x, intra_tile_x, THREAD2STRUCT->tile_x_mask, 12); COMPUTE_MASKED_VAL(y, intra_tile_y, Y_TILE_MASK, 12); fMX_DEBUG_LOG(2, " Tile: x=%d y=%d  Activation PA: 0x%llx ", x, y, act_pa+act_pa_offset ); fMX_VERIF_DEBUG_LOG(2, " Tile: x=%d y=%d  Activation PA: 0x%llx ", x, y, act_pa+act_pa_offset ); } } }
#define fMXLOAD_WEIGHTS(ADDR, LOAD_NEXT, FLT) { if (LOAD_NEXT==0) { int total_wgts = 32 * 4; for(int wgt_idx = 0; wgt_idx < 128; wgt_idx++) { THREAD2STRUCT->wgt_cache[wgt_idx].flt = 0; THREAD2STRUCT->wgt_cache[wgt_idx].valid = 0; } if (ADDR <= THREAD2STRUCT->max_weight_pa) { paddr_t wgt_stride = (FLT) ? 2 : 1; paddr_t wgt_addr = ADDR; for(int wgt_idx = 0; wgt_idx < total_wgts; wgt_idx++, wgt_addr += wgt_stride) { THREAD2STRUCT->wgt_cache[wgt_idx].valid = 1; if (wgt_addr <= THREAD2STRUCT->max_weight_pa) { THREAD2STRUCT->wgt_cache[wgt_idx].val = (FLT) ? sim_mem_read2(thread->system_ptr, thread->threadId, wgt_addr) : sim_mem_read1(thread->system_ptr, thread->threadId, wgt_addr); fMX_DEBUG_LOG(3, "WGT_LOAD [%d of %d] Weight=0x%02x PA=%llx", wgt_idx, total_wgts, (FLT) ? THREAD2STRUCT->wgt_cache[wgt_idx].flt : THREAD2STRUCT->wgt_cache[wgt_idx].fxp, wgt_addr); fMX_VERIF_DEBUG_LOG(2, "WGT_LOAD [%d of %d] Weight=0x%02x PA=%llx", wgt_idx, total_wgts, (FLT) ? THREAD2STRUCT->wgt_cache[wgt_idx].flt : THREAD2STRUCT->wgt_cache[wgt_idx].fxp, wgt_addr); } else { THREAD2STRUCT->force_zero = 1; THREAD2STRUCT->wgt_cache[wgt_idx].val = 0; fMX_DEBUG_LOG(3, "WGT_LOAD SET ZERO for [%d of %d] PA=%llx exceeded limit: %llx", wgt_idx, total_wgts, wgt_addr, THREAD2STRUCT->max_weight_pa); fMX_VERIF_DEBUG_LOG(2, "WGT_LOAD SET ZERO for [%d of %d] PA=%llx exceeded limit: %llx", wgt_idx, total_wgts, wgt_addr, THREAD2STRUCT->max_weight_pa); } } } } else{ fMX_DEBUG_LOG(3,"SKIPPING WEIGHT LOAD"); } }
#define fMXSELECT_WEIGHT(WEIGHT,WEIGHT_VALID,ADDR,SCALE,UNPACK,OUTPUT_IDX,INPUT_IDX,FLT) { int weight_pa_out_mask = 0; int weight_pa_in_mask = 0; int input_offset = 0; int bit_select = 0; int output_offset = 0; int w_idx = 0; if (FLT) { weight_pa_out_mask = ((1<<fMX_GETCHANNELSIZE(thread->processor_ptr))-1) << 1; weight_pa_in_mask = (((1 << 2*fMX_GETCHANNELSIZE(thread->processor_ptr))-1) & ~weight_pa_out_mask); input_offset = (INPUT_IDX & 0x1) | ((INPUT_IDX & 0x3E) << 5); output_offset = OUTPUT_IDX << 1; } else { weight_pa_out_mask = ((1<<fMX_GETCHANNELSIZE(thread->processor_ptr))-1) << 2; weight_pa_in_mask = (((1 << 2*fMX_GETCHANNELSIZE(thread->processor_ptr))-1) & ~weight_pa_out_mask); bit_select = (INPUT_IDX>>2) & ((1<<SCALE)-1); input_offset = (INPUT_IDX & 0x3) | (( (INPUT_IDX>>(2+SCALE))) << 7); output_offset = OUTPUT_IDX << 2; } w_idx = fMX_INC_MASKED(w_idx, output_offset, weight_pa_out_mask); w_idx = fMX_INC_MASKED(w_idx, input_offset, weight_pa_in_mask); if (FLT) { UNPACK(WEIGHT, THREAD2STRUCT->wgt_cache[w_idx].flt, bit_select); } else { UNPACK(WEIGHT, THREAD2STRUCT->wgt_cache[w_idx].fxp, bit_select); } WEIGHT_VALID = THREAD2STRUCT->wgt_cache[w_idx].valid; fMX_DEBUG_LOG(4,"Weight=%x valid=%d input=%d index=%04x bit=%d addr=%llx ", WEIGHT, WEIGHT_VALID, INPUT_IDX, w_idx, bit_select, ADDR + (w_idx<<FLT)); fMX_VERIF_DEBUG_LOG(2,"Weight=%x valid=%d input=%d index=%04x bit=%d addr=%llx ", WEIGHT, WEIGHT_VALID, INPUT_IDX, w_idx, bit_select, ADDR + (w_idx<<FLT)); }
#define fALIGN_CVT(ADDR, FORMAT) ADDR &= (~((1<<(fMX_GETCHANNELSIZE(thread->processor_ptr)+2))-1)) << FORMAT
#define fALIGN_FXP_DEPTH(ADDR,SCALE) ADDR &= ~((thread->processor_ptr->arch_proc_options->hmx_output_depth*4*SCALE) - 1)
#define fALIGN_BLOCK(ADDR) ADDR &= ~((1<<(fMX_GETCHANNELSIZE(thread->processor_ptr)+ thread->processor_ptr->arch_proc_options->hmx_spatial_size))-1)
#define fMX_SIGN_EXTEND_BITS(VAL,SELECT,MASK,EXT) (((size1s_t)(((VAL >> SELECT) & MASK) << EXT)) >> EXT)
#define fMX_UNPACK_NONE(OUT,IN,BIT_SELECT) { OUT = IN; }
#define fMX_UNPACK_BYTE_FROM_BYTE(OUT,IN,BIT_SELECT) { OUT = IN; THREAD2STRUCT->weight_bits = 8; }
#define fMX_UNPACK_SM_FROM_BYTE(OUT,IN,BIT_SELECT) { OUT = ((((size1s_t)IN >> 7)) & 0x7F) ^ IN; THREAD2STRUCT->weight_bits = 8; }
#define fMX_UNPACK_1SBIT_FROM_BYTE(OUT,IN,BIT_SELECT) { OUT = (((size1s_t)IN >> BIT_SELECT) & 0x1) ? -1 : 1 ; if (THREAD2STRUCT->force_zero) { OUT = 0; } THREAD2STRUCT->weight_bits = 1; }
#define fMX_UNPACK_1BIT_FROM_BYTE(OUT,IN,BIT_SELECT) { OUT = (IN >> BIT_SELECT) & 0x1; THREAD2STRUCT->weight_bits = 1; }
#define fMX_UNPACK_NIBBLE_FROM_BYTE(OUT,IN,BIT_SELECT) { OUT = fMX_SIGN_EXTEND_BITS(IN, 4*BIT_SELECT , 0xF, 4); THREAD2STRUCT->weight_bits = 4; }
#define fMX_UNPACK_CRUMB_FROM_BYTE(OUT,IN,BIT_SELECT) { OUT = fMX_SIGN_EXTEND_BITS(IN, 2*BIT_SELECT, 0x3, 6); THREAD2STRUCT->weight_bits = 2; }
#define fMX_UNPACK_SCRUMB_FROM_BYTE(OUT,IN,BIT_SELECT) { OUT = fMX_SIGN_EXTEND_BITS(IN, 2*BIT_SELECT, 0x3, 6); OUT = ((size1s_t)OUT >= 0) ? 2-OUT : OUT; if (THREAD2STRUCT->force_zero) { OUT = 0; } THREAD2STRUCT->weight_bits = 2; }
#define fMX_LOAD_BIAS_LO(OUT_IDX,EA,PA) { paddr_t pa = PA+4*OUT_IDX; size4u_t bias_word = sim_mem_read4(thread->system_ptr, thread->threadId, pa); THREAD2STRUCT->future_bias[OUT_IDX].val[0] = bias_word; THREAD2STRUCT->future_bias[OUT_IDX].val[1] = 0; fMX_DEBUG_LOG(2,"Bias Loaded[%d][0]=%08x ", OUT_IDX, bias_word); fMX_VERIF_DEBUG_LOG(2,"Bias Loaded[%d][0]=%08x ", OUT_IDX, bias_word); }
#define fMX_LOAD_BIAS_HI(OUT_IDX,EA,PA) { paddr_t pa = PA+4*OUT_IDX + 128; size4u_t bias_word = sim_mem_read4(thread->system_ptr, thread->threadId, pa); THREAD2STRUCT->future_bias[OUT_IDX].val[1] = bias_word; fMX_DEBUG_LOG(2,"Bias Loaded[%d][1]=%08x ", OUT_IDX, bias_word); fMX_VERIF_DEBUG_LOG(2,"Bias Loaded[%d][1]=%08x ", OUT_IDX, bias_word); }
#define fMX_STORE_BIAS(OUT_IDX,EA) {}
#define fMX_STORE_BIAS_DOUBLE(OUT_IDX,EA) {}
#define fMX_SELECT_BIAS(OUT_IDX) (THREAD2STRUCT->fxp_commit_state.bias_update ? THREAD2STRUCT->future_bias[OUT_IDX] : THREAD2STRUCT->bias[OUT_IDX])
#define fMX_UPDATE_ACC(FLT) if (FLT) { THREAD2STRUCT->flt_commit_state.acc_update = 1; } else { THREAD2STRUCT->fxp_commit_state.acc_update = 1; }
#define fMX_COMPUTE_ACC_INDEX(OUTPUT_IDX,EVEN_ODD,X_TAP_IDX,X_TILE_IDX,X_TILE_MASK,Y_TILE_IDX,CURRENT_ACC) { int a0 = CURRENT_ACC; int overflow = 0; if (X_TAP_IDX >= 0) { fMX_INC_MASKED_OVERFLOW(overflow, OUTPUT_IDX, X_TAP_IDX, X_TILE_IDX, X_TILE_MASK); EVEN_ODD = (overflow) ? (a0 ^ 0x1) & 0x1 : a0 & 0x1; } else { OUTPUT_IDX = X_TILE_IDX; EVEN_ODD = (a0 ^ 0x1) & 0x1; } OUTPUT_IDX |= Y_TILE_IDX; unsigned int mask = (1<<thread->mem_access[1].hmx_ma.format)-1; unsigned int temp = (OUTPUT_IDX >> fMX_GETCHANNELSIZE(thread->processor_ptr)) & ~mask; OUTPUT_IDX = temp | (OUTPUT_IDX & mask); if (overflow && (thread->mem_access[1].hmx_ma.drop || thread->mem_access[0].hmx_ma.deep)) { OUTPUT_IDX = -1; } }
#define fMX_GET_ACC_INDEX(IDX,Q_OFFSET) { unsigned int temp = (IDX >> fMX_GETCHANNELSIZE(thread->processor_ptr)) & ~((1<<Q_OFFSET)-1); IDX = temp | (IDX & ((1<<Q_OFFSET)-1)); }
#define fVNOSAT_UBYTE(OUT,IN) OUT = IN & 0xFF
#define fVNOSATHF(VAL)
#define CHECK_ACCESS_RANGE(EXC,PADDR,LEN) { vaddr_t page_mask = (1ULL<<thread->mem_access[slot].xlate_info.size)-1; paddr_t page_of_start = (PADDR & ~page_mask); paddr_t page_of_end = ((PADDR + LEN) & ~page_mask); EXC |= (page_of_start != page_of_end); }

#endif
