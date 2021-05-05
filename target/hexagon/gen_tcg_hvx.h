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

#ifndef HEXAGON_GEN_TCG_HVX_H
#define HEXAGON_GEN_TCG_HVX_H

#define fGEN_TCG_V6_vassign(SHORTCODE) \
    tcg_gen_gvec_mov(MO_64, VdV_off, VuV_off, \
                     sizeof(MMVector), sizeof(MMVector))

/* Vector conditional move (true) */
#define fGEN_TCG_V6_vcmov(SHORTCODE) \
    do { \
        TCGv lsb = tcg_temp_new(); \
        TCGLabel *label = gen_new_label(); \
        tcg_gen_andi_tl(lsb, PsV, 1); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, lsb, 0, label); \
        tcg_temp_free(lsb); \
        tcg_gen_gvec_mov(MO_64, VdV_off, VuV_off, \
                         sizeof(MMVector), sizeof(MMVector)); \
        gen_set_label(label); \
    } while (0)

/* Vector conditional move (false) */
#define fGEN_TCG_V6_vncmov(SHORTCODE) \
    do { \
        TCGv lsb = tcg_temp_new(); \
        TCGLabel *label = gen_new_label(); \
        tcg_gen_andi_tl(lsb, PsV, 1); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, lsb, 1, label); \
        tcg_temp_free(lsb); \
        tcg_gen_gvec_mov(MO_64, VdV_off, VuV_off, \
                         sizeof(MMVector), sizeof(MMVector)); \
        gen_set_label(label); \
    } while (0)

/* Vector add - various forms */
#define fGEN_TCG_V6_vaddb(SHORTCODE) \
    tcg_gen_gvec_add(MO_8, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vaddh(SHORTCYDE) \
    tcg_gen_gvec_add(MO_16, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vaddw(SHORTCODE) \
    tcg_gen_gvec_add(MO_32, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vaddb_dv(SHORTCODE) \
    tcg_gen_gvec_add(MO_8, VddV_off, VuuV_off, VvvV_off, \
                     sizeof(MMVector) * 2, sizeof(MMVector) * 2)

#define fGEN_TCG_V6_vaddh_dv(SHORTCYDE) \
    tcg_gen_gvec_add(MO_16, VddV_off, VuuV_off, VvvV_off, \
                     sizeof(MMVector) * 2, sizeof(MMVector) * 2)

#define fGEN_TCG_V6_vaddw_dv(SHORTCODE) \
    tcg_gen_gvec_add(MO_32, VddV_off, VuuV_off, VvvV_off, \
                     sizeof(MMVector) * 2, sizeof(MMVector) * 2)

/* Vector sub - various forms */
#define fGEN_TCG_V6_vsubb(SHORTCODE) \
    tcg_gen_gvec_sub(MO_8, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vsubh(SHORTCODE) \
    tcg_gen_gvec_sub(MO_16, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vsubw(SHORTCODE) \
    tcg_gen_gvec_sub(MO_32, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vsubb_dv(SHORTCODE) \
    tcg_gen_gvec_sub(MO_8, VddV_off, VuuV_off, VvvV_off, \
                     sizeof(MMVector) * 2, sizeof(MMVector) * 2)

#define fGEN_TCG_V6_vsubh_dv(SHORTCODE) \
    tcg_gen_gvec_sub(MO_16, VddV_off, VuuV_off, VvvV_off, \
                     sizeof(MMVector) * 2, sizeof(MMVector) * 2)

#define fGEN_TCG_V6_vsubw_dv(SHORTCODE) \
    tcg_gen_gvec_sub(MO_32, VddV_off, VuuV_off, VvvV_off, \
                     sizeof(MMVector) * 2, sizeof(MMVector) * 2)

/* Vector shift right - various forms */
#define fGEN_TCG_V6_vasrh(SHORTCODE) \
    do { \
        TCGv shift = tcg_temp_new(); \
        tcg_gen_andi_tl(shift, RtV, 15); \
        tcg_gen_gvec_sars(MO_16, VdV_off, VuV_off, shift, \
                          sizeof(MMVector), sizeof(MMVector)); \
        tcg_temp_free(shift); \
    } while (0)

#define fGEN_TCG_V6_vasrw(SHORTCODE) \
    do { \
        TCGv shift = tcg_temp_new(); \
        tcg_gen_andi_tl(shift, RtV, 31); \
        tcg_gen_gvec_sars(MO_32, VdV_off, VuV_off, shift, \
                          sizeof(MMVector), sizeof(MMVector)); \
        tcg_temp_free(shift); \
    } while (0)

#define fGEN_TCG_V6_vlsrh(SHORTCODE) \
    do { \
        TCGv shift = tcg_temp_new(); \
        tcg_gen_andi_tl(shift, RtV, 15); \
        tcg_gen_gvec_shrs(MO_16, VdV_off, VuV_off, shift, \
                          sizeof(MMVector), sizeof(MMVector)); \
        tcg_temp_free(shift); \
    } while (0)

#define fGEN_TCG_V6_vlsrw(SHORTCODE) \
    do { \
        TCGv shift = tcg_temp_new(); \
        tcg_gen_andi_tl(shift, RtV, 31); \
        tcg_gen_gvec_shrs(MO_32, VdV_off, VuV_off, shift, \
                          sizeof(MMVector), sizeof(MMVector)); \
        tcg_temp_free(shift); \
    } while (0)

/* Vector shift left - various forms */
#define fGEN_TCG_V6_vaslw(SHORTCODE) \
    do { \
        TCGv shift = tcg_temp_new(); \
        tcg_gen_andi_tl(shift, RtV, 31); \
        tcg_gen_gvec_shls(MO_32, VdV_off, VuV_off, shift, \
                          sizeof(MMVector), sizeof(MMVector)); \
        tcg_temp_free(shift); \
    } while (0)

/* Vector max - various forms */
#define fGEN_TCG_V6_vmaxw(SHORTCODE) \
    tcg_gen_gvec_smax(MO_32, VdV_off, VuV_off, VvV_off, \
                      sizeof(MMVector), sizeof(MMVector))

/* Vector min - various forms */
#define fGEN_TCG_V6_vminw(SHORTCODE) \
    tcg_gen_gvec_smin(MO_32, VdV_off, VuV_off, VvV_off, \
                      sizeof(MMVector), sizeof(MMVector))

/* Vector logical ops */
#define fGEN_TCG_V6_vxor(SHORTCODE) \
    tcg_gen_gvec_xor(MO_64, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vand(SHORTCODE) \
    tcg_gen_gvec_and(MO_64, VdV_off, VuV_off, VvV_off, \
                     sizeof(MMVector), sizeof(MMVector))

#define fGEN_TCG_V6_vor(SHORTCODE) \
    tcg_gen_gvec_or(MO_64, VdV_off, VuV_off, VvV_off, \
                    sizeof(MMVector), sizeof(MMVector))

/* Vector splat - various forms */
#define fGEN_TCG_V6_lvsplatw(SHORTCODE) \
    tcg_gen_gvec_dup_i32(MO_32, VdV_off, \
                         sizeof(MMVector), sizeof(MMVector), RtV)

/* Vector loads */
#define fGEN_TCG_V6_vL32b_pi(SHORTCODE)                    SHORTCODE
#define fGEN_TCG_V6_vL32Ub_pi(SHORTCODE)                   SHORTCODE
#define fGEN_TCG_V6_vL32b_cur_pi(SHORTCODE)                SHORTCODE
#define fGEN_TCG_V6_vL32b_tmp_pi(SHORTCODE)                SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_pi(SHORTCODE)                 SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_cur_pi(SHORTCODE)             SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_tmp_pi(SHORTCODE)             SHORTCODE
#define fGEN_TCG_V6_vL32b_ai(SHORTCODE)                    SHORTCODE
#define fGEN_TCG_V6_vL32Ub_ai(SHORTCODE)                   SHORTCODE
#define fGEN_TCG_V6_vL32b_cur_ai(SHORTCODE)                SHORTCODE
#define fGEN_TCG_V6_vL32b_tmp_ai(SHORTCODE)                SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_ai(SHORTCODE)                 SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_cur_ai(SHORTCODE)             SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_tmp_ai(SHORTCODE)             SHORTCODE
#define fGEN_TCG_V6_vL32b_ppu(SHORTCODE)                   SHORTCODE
#define fGEN_TCG_V6_vL32Ub_ppu(SHORTCODE)                  SHORTCODE
#define fGEN_TCG_V6_vL32b_cur_ppu(SHORTCODE)               SHORTCODE
#define fGEN_TCG_V6_vL32b_tmp_ppu(SHORTCODE)               SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_ppu(SHORTCODE)                SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_cur_ppu(SHORTCODE)            SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_tmp_ppu(SHORTCODE)            SHORTCODE

/* Predicated vector loads */
#define fGEN_TCG_PRED_VEC_LOAD(GET_EA, PRED, DSTOFF, INC) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGLabel *label = gen_new_label(); \
        GET_EA; \
        PRED; \
        gen_pred_cancel(LSB, insn->slot); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, label); \
        tcg_temp_free(LSB); \
        gen_vreg_load(ctx, DSTOFF, EA, true); \
        INC; \
        gen_set_label(label); \
    } while (0)

#define fGEN_TCG_PRED_VEC_LOAD_pred_pi \
    fGEN_TCG_PRED_VEC_LOAD(fLSBOLD(PvV), \
                           fEA_REG(RxV), \
                           VdV_off, \
                           fPM_I(RxV, siV * sizeof(MMVector)))
#define fGEN_TCG_PRED_VEC_LOAD_npred_pi \
    fGEN_TCG_PRED_VEC_LOAD(fLSBOLDNOT(PvV), \
                           fEA_REG(RxV), \
                           VdV_off, \
                           fPM_I(RxV, siV * sizeof(MMVector)))

#define fGEN_TCG_V6_vL32b_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_pi
#define fGEN_TCG_V6_vL32b_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_pi
#define fGEN_TCG_V6_vL32b_cur_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_pi
#define fGEN_TCG_V6_vL32b_cur_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_pi
#define fGEN_TCG_V6_vL32b_tmp_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_pi
#define fGEN_TCG_V6_vL32b_tmp_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_pi
#define fGEN_TCG_V6_vL32b_nt_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_pi
#define fGEN_TCG_V6_vL32b_nt_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_pi
#define fGEN_TCG_V6_vL32b_nt_cur_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_pi
#define fGEN_TCG_V6_vL32b_nt_cur_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_pi
#define fGEN_TCG_V6_vL32b_nt_tmp_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_pi
#define fGEN_TCG_V6_vL32b_nt_tmp_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_pi

#define fGEN_TCG_PRED_VEC_LOAD_pred_ai \
    fGEN_TCG_PRED_VEC_LOAD(fLSBOLD(PvV), \
                           fEA_RI(RtV, siV * sizeof(MMVector)), \
                           VdV_off, \
                           do {} while (0))
#define fGEN_TCG_PRED_VEC_LOAD_npred_ai \
    fGEN_TCG_PRED_VEC_LOAD(fLSBOLDNOT(PvV), \
                           fEA_RI(RtV, siV * sizeof(MMVector)), \
                           VdV_off, \
                           do {} while (0))

#define fGEN_TCG_V6_vL32b_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ai
#define fGEN_TCG_V6_vL32b_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ai
#define fGEN_TCG_V6_vL32b_cur_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ai
#define fGEN_TCG_V6_vL32b_cur_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ai
#define fGEN_TCG_V6_vL32b_tmp_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ai
#define fGEN_TCG_V6_vL32b_tmp_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ai
#define fGEN_TCG_V6_vL32b_nt_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ai
#define fGEN_TCG_V6_vL32b_nt_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ai
#define fGEN_TCG_V6_vL32b_nt_cur_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ai
#define fGEN_TCG_V6_vL32b_nt_cur_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ai
#define fGEN_TCG_V6_vL32b_nt_tmp_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ai
#define fGEN_TCG_V6_vL32b_nt_tmp_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ai

#define fGEN_TCG_PRED_VEC_LOAD_pred_ppu \
    fGEN_TCG_PRED_VEC_LOAD(fLSBOLD(PvV), \
                           fEA_REG(RxV), \
                           VdV_off, \
                           fPM_M(RxV, MuV))
#define fGEN_TCG_PRED_VEC_LOAD_npred_ppu \
    fGEN_TCG_PRED_VEC_LOAD(fLSBOLDNOT(PvV), \
                           fEA_REG(RxV), \
                           VdV_off, \
                           fPM_M(RxV, MuV))

#define fGEN_TCG_V6_vL32b_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ppu
#define fGEN_TCG_V6_vL32b_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ppu
#define fGEN_TCG_V6_vL32b_cur_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ppu
#define fGEN_TCG_V6_vL32b_cur_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ppu
#define fGEN_TCG_V6_vL32b_tmp_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ppu
#define fGEN_TCG_V6_vL32b_tmp_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ppu
#define fGEN_TCG_V6_vL32b_nt_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ppu
#define fGEN_TCG_V6_vL32b_nt_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ppu
#define fGEN_TCG_V6_vL32b_nt_cur_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ppu
#define fGEN_TCG_V6_vL32b_nt_cur_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ppu
#define fGEN_TCG_V6_vL32b_nt_tmp_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_pred_ppu
#define fGEN_TCG_V6_vL32b_nt_tmp_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_LOAD_npred_ppu

/* Vector stores */
#define fGEN_TCG_V6_vS32b_pi(SHORTCODE)                    SHORTCODE
#define fGEN_TCG_V6_vS32Ub_pi(SHORTCODE)                   SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_pi(SHORTCODE)                 SHORTCODE
#define fGEN_TCG_V6_vS32b_ai(SHORTCODE)                    SHORTCODE
#define fGEN_TCG_V6_vS32Ub_ai(SHORTCODE)                   SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_ai(SHORTCODE)                 SHORTCODE
#define fGEN_TCG_V6_vS32b_ppu(SHORTCODE)                   SHORTCODE
#define fGEN_TCG_V6_vS32Ub_ppu(SHORTCODE)                  SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_ppu(SHORTCODE)                SHORTCODE

/* New value vector stores */
#define fGEN_TCG_NEWVAL_VEC_STORE(GET_EA, INC) \
    do { \
        GET_EA; \
        gen_vreg_store(ctx, EA, OsN_off, insn->slot, true); \
        INC; \
    } while (0)

#define fGEN_TCG_NEWVAL_VEC_STORE_pi \
    fGEN_TCG_NEWVAL_VEC_STORE(fEA_REG(RxV), fPM_I(RxV, siV * sizeof(MMVector)))

#define fGEN_TCG_V6_vS32b_new_pi(SHORTCODE) \
    fGEN_TCG_NEWVAL_VEC_STORE_pi
#define fGEN_TCG_V6_vS32b_nt_new_pi(SHORTCODE) \
    fGEN_TCG_NEWVAL_VEC_STORE_pi

#define fGEN_TCG_NEWVAL_VEC_STORE_ai \
    fGEN_TCG_NEWVAL_VEC_STORE(fEA_RI(RtV, siV * sizeof(MMVector)), \
                              do { } while (0))

#define fGEN_TCG_V6_vS32b_new_ai(SHORTCODE) \
    fGEN_TCG_NEWVAL_VEC_STORE_ai
#define fGEN_TCG_V6_vS32b_nt_new_ai(SHORTCODE) \
    fGEN_TCG_NEWVAL_VEC_STORE_ai

#define fGEN_TCG_NEWVAL_VEC_STORE_ppu \
    fGEN_TCG_NEWVAL_VEC_STORE(fEA_REG(RxV), fPM_M(RxV, MuV))

#define fGEN_TCG_V6_vS32b_new_ppu(SHORTCODE) \
    fGEN_TCG_NEWVAL_VEC_STORE_ppu
#define fGEN_TCG_V6_vS32b_nt_new_ppu(SHORTCODE) \
    fGEN_TCG_NEWVAL_VEC_STORE_ppu

/* Predicated vector stores */
#define fGEN_TCG_PRED_VEC_STORE(GET_EA, PRED, SRCOFF, ALIGN, INC) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGLabel *label = gen_new_label(); \
        GET_EA; \
        PRED; \
        gen_pred_cancel(LSB, insn->slot); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, label); \
        tcg_temp_free(LSB); \
        gen_vreg_store(ctx, EA, SRCOFF, insn->slot, ALIGN); \
        INC; \
        gen_set_label(label); \
    } while (0)

#define fGEN_TCG_PRED_VEC_STORE_pred_pi(ALIGN) \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLD(PvV), \
                            fEA_REG(RxV), \
                            VsV_off, ALIGN, \
                            fPM_I(RxV, siV * sizeof(MMVector)))
#define fGEN_TCG_PRED_VEC_STORE_npred_pi(ALIGN) \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLDNOT(PvV), \
                            fEA_REG(RxV), \
                            VsV_off, ALIGN, \
                            fPM_I(RxV, siV * sizeof(MMVector)))
#define fGEN_TCG_PRED_VEC_STORE_new_pred_pi \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLD(PvV), \
                            fEA_REG(RxV), \
                            OsN_off, true, \
                            fPM_I(RxV, siV * sizeof(MMVector)))
#define fGEN_TCG_PRED_VEC_STORE_new_npred_pi \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLDNOT(PvV), \
                            fEA_REG(RxV), \
                            OsN_off, true, \
                            fPM_I(RxV, siV * sizeof(MMVector)))

#define fGEN_TCG_V6_vS32b_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_pi(true)
#define fGEN_TCG_V6_vS32b_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_pi(true)
#define fGEN_TCG_V6_vS32Ub_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_pi(false)
#define fGEN_TCG_V6_vS32Ub_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_pi(false)
#define fGEN_TCG_V6_vS32b_nt_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_pi(true)
#define fGEN_TCG_V6_vS32b_nt_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_pi(true)
#define fGEN_TCG_V6_vS32b_new_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_pred_pi
#define fGEN_TCG_V6_vS32b_new_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_npred_pi
#define fGEN_TCG_V6_vS32b_nt_new_pred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_pred_pi
#define fGEN_TCG_V6_vS32b_nt_new_npred_pi(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_npred_pi

#define fGEN_TCG_PRED_VEC_STORE_pred_ai(ALIGN) \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLD(PvV), \
                            fEA_RI(RtV, siV * sizeof(MMVector)), \
                            VsV_off, ALIGN, \
                            do { } while (0))
#define fGEN_TCG_PRED_VEC_STORE_npred_ai(ALIGN) \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLDNOT(PvV), \
                            fEA_RI(RtV, siV * sizeof(MMVector)), \
                            VsV_off, ALIGN, \
                            do { } while (0))
#define fGEN_TCG_PRED_VEC_STORE_new_pred_ai \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLD(PvV), \
                            fEA_RI(RtV, siV * sizeof(MMVector)), \
                            OsN_off, true, \
                            do { } while (0))
#define fGEN_TCG_PRED_VEC_STORE_new_npred_ai \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLDNOT(PvV), \
                            fEA_RI(RtV, siV * sizeof(MMVector)), \
                            OsN_off, true, \
                            do { } while (0))

#define fGEN_TCG_V6_vS32b_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_ai(true)
#define fGEN_TCG_V6_vS32b_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_ai(true)
#define fGEN_TCG_V6_vS32Ub_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_ai(false)
#define fGEN_TCG_V6_vS32Ub_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_ai(false)
#define fGEN_TCG_V6_vS32b_nt_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_ai(true)
#define fGEN_TCG_V6_vS32b_nt_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_ai(true)
#define fGEN_TCG_V6_vS32b_new_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_pred_ai
#define fGEN_TCG_V6_vS32b_new_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_npred_ai
#define fGEN_TCG_V6_vS32b_nt_new_pred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_pred_ai
#define fGEN_TCG_V6_vS32b_nt_new_npred_ai(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_npred_ai

#define fGEN_TCG_PRED_VEC_STORE_pred_ppu(ALIGN) \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLD(PvV), \
                            fEA_REG(RxV), \
                            VsV_off, ALIGN, \
                            fPM_M(RxV, MuV))
#define fGEN_TCG_PRED_VEC_STORE_npred_ppu(ALIGN) \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLDNOT(PvV), \
                            fEA_REG(RxV), \
                            VsV_off, ALIGN, \
                            fPM_M(RxV, MuV))
#define fGEN_TCG_PRED_VEC_STORE_new_pred_ppu \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLD(PvV), \
                            fEA_REG(RxV), \
                            OsN_off, true, \
                            fPM_M(RxV, MuV))
#define fGEN_TCG_PRED_VEC_STORE_new_npred_ppu \
    fGEN_TCG_PRED_VEC_STORE(fLSBOLDNOT(PvV), \
                            fEA_REG(RxV), \
                            OsN_off, true, \
                            fPM_M(RxV, MuV))

#define fGEN_TCG_V6_vS32b_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_ppu(true)
#define fGEN_TCG_V6_vS32b_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_ppu(true)
#define fGEN_TCG_V6_vS32Ub_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_ppu(false)
#define fGEN_TCG_V6_vS32Ub_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_ppu(false)
#define fGEN_TCG_V6_vS32b_nt_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_pred_ppu(true)
#define fGEN_TCG_V6_vS32b_nt_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_npred_ppu(true)
#define fGEN_TCG_V6_vS32b_new_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_pred_ppu
#define fGEN_TCG_V6_vS32b_new_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_npred_ppu
#define fGEN_TCG_V6_vS32b_nt_new_pred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_pred_ppu
#define fGEN_TCG_V6_vS32b_nt_new_npred_ppu(SHORTCODE) \
    fGEN_TCG_PRED_VEC_STORE_new_npred_ppu

/* Masked vector stores */
#define fGEN_TCG_V6_vS32b_qpred_pi(SHORTCODE)              SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_qpred_pi(SHORTCODE)           SHORTCODE
#define fGEN_TCG_V6_vS32b_qpred_ai(SHORTCODE)              SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_qpred_ai(SHORTCODE)           SHORTCODE
#define fGEN_TCG_V6_vS32b_qpred_ppu(SHORTCODE)             SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_qpred_ppu(SHORTCODE)          SHORTCODE
#define fGEN_TCG_V6_vS32b_nqpred_pi(SHORTCODE)             SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_nqpred_pi(SHORTCODE)          SHORTCODE
#define fGEN_TCG_V6_vS32b_nqpred_ai(SHORTCODE)             SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_nqpred_ai(SHORTCODE)          SHORTCODE
#define fGEN_TCG_V6_vS32b_nqpred_ppu(SHORTCODE)            SHORTCODE
#define fGEN_TCG_V6_vS32b_nt_nqpred_ppu(SHORTCODE)         SHORTCODE

/* Store release not modelled in qemu, but need to suppress compiler warnings */
#define fGEN_TCG_V6_vS32b_srls_pi(SHORTCODE) \
    do { \
        siV = siV; \
    } while (0)
#define fGEN_TCG_V6_vS32b_srls_ai(SHORTCODE) \
    do { \
        RtV = RtV; \
        siV = siV; \
    } while (0)
#define fGEN_TCG_V6_vS32b_srls_ppu(SHORTCODE) \
    do { \
        MuV = MuV; \
    } while (0)

#endif
