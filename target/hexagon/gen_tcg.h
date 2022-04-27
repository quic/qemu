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

#ifndef HEXAGON_GEN_TCG_H
#define HEXAGON_GEN_TCG_H

/*
 * Here is a primer to understand the tag names for load/store instructions
 *
 * Data types
 *      b        signed byte                       r0 = memb(r2+#0)
 *     ub        unsigned byte                     r0 = memub(r2+#0)
 *      h        signed half word (16 bits)        r0 = memh(r2+#0)
 *     uh        unsigned half word                r0 = memuh(r2+#0)
 *      i        integer (32 bits)                 r0 = memw(r2+#0)
 *      d        double word (64 bits)             r1:0 = memd(r2+#0)
 *
 * Addressing modes
 *     _io       indirect with offset              r0 = memw(r1+#4)
 *     _ur       absolute with register offset     r0 = memw(r1<<#4+##variable)
 *     _rr       indirect with register offset     r0 = memw(r1+r4<<#2)
 *     gp        global pointer relative           r0 = memw(gp+#200)
 *     _sp       stack pointer relative            r0 = memw(r29+#12)
 *     _ap       absolute set                      r0 = memw(r1=##variable)
 *     _pr       post increment register           r0 = memw(r1++m1)
 *     _pbr      post increment bit reverse        r0 = memw(r1++m1:brev)
 *     _pi       post increment immediate          r0 = memb(r1++#1)
 *     _pci      post increment circular immediate r0 = memw(r1++#4:circ(m0))
 *     _pcr      post increment circular register  r0 = memw(r1++I:circ(m0))
 */

/* Macros for complex addressing modes */
#define GET_EA_ap \
    do { \
        fEA_IMM(UiV); \
        tcg_gen_movi_tl(ReV, UiV); \
    } while (0)
#define GET_EA_pr \
    do { \
        fEA_REG(RxV); \
        fPM_M(RxV, MuV); \
    } while (0)
#define GET_EA_pbr \
    do { \
        gen_helper_fbrev(EA, RxV); \
        tcg_gen_add_tl(RxV, RxV, MuV); \
    } while (0)
#define GET_EA_pi \
    do { \
        fEA_REG(RxV); \
        fPM_I(RxV, siV); \
    } while (0)
#define GET_EA_pci \
    do { \
        TCGv tcgv_siV = tcg_const_tl(siV); \
        tcg_gen_mov_tl(EA, RxV); \
        gen_helper_fcircadd(RxV, RxV, tcgv_siV, MuV, \
                            hex_gpr[HEX_REG_CS0 + MuN]); \
        tcg_temp_free(tcgv_siV); \
    } while (0)
#define GET_EA_pcr(SHIFT) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        tcg_gen_mov_tl(EA, RxV); \
        gen_read_ireg(ireg, MuV, (SHIFT)); \
        gen_helper_fcircadd(RxV, RxV, ireg, MuV, hex_gpr[HEX_REG_CS0 + MuN]); \
        tcg_temp_free(ireg); \
    } while (0)

/* Byte load instructions */
#define fGEN_TCG_L2_loadrub_io(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrb_io(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadrub_ur(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L4_loadrb_ur(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadrub_rr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L4_loadrb_rr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrubgp(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrbgp(SHORTCODE)        SHORTCODE
#define fGEN_TCG_SL1_loadrub_io(SHORTCODE)     SHORTCODE
#define fGEN_TCG_SL2_loadrb_io(SHORTCODE)      SHORTCODE

/* Half word load instruction */
#define fGEN_TCG_L2_loadruh_io(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrh_io(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadruh_ur(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L4_loadrh_ur(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadruh_rr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L4_loadrh_rr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadruhgp(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrhgp(SHORTCODE)        SHORTCODE
#define fGEN_TCG_SL2_loadruh_io(SHORTCODE)     SHORTCODE
#define fGEN_TCG_SL2_loadrh_io(SHORTCODE)      SHORTCODE

/* Word load instructions */
#define fGEN_TCG_L2_loadri_io(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadri_ur(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadri_rr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrigp(SHORTCODE)        SHORTCODE
#define fGEN_TCG_SL1_loadri_io(SHORTCODE)      SHORTCODE
#define fGEN_TCG_SL2_loadri_sp(SHORTCODE) \
    do { \
        tcg_gen_addi_tl(EA, hex_gpr[HEX_REG_SP], uiV); \
        fLOAD(1, 4, u, EA, RdV); \
    } while (0)

/* Double word load instructions */
#define fGEN_TCG_L2_loadrd_io(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadrd_ur(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L4_loadrd_rr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrdgp(SHORTCODE)        SHORTCODE
#define fGEN_TCG_SL2_loadrd_sp(SHORTCODE) \
    do { \
        tcg_gen_addi_tl(EA, hex_gpr[HEX_REG_SP], uiV); \
        fLOAD(1, 8, u, EA, RddV); \
    } while (0)

/* Instructions with multiple definitions */
#define fGEN_TCG_LOAD_AP(RES, SIZE, SIGN) \
    do { \
        fMUST_IMMEXT(UiV); \
        fEA_IMM(UiV); \
        fLOAD(1, SIZE, SIGN, EA, RES); \
        tcg_gen_movi_tl(ReV, UiV); \
    } while (0)

#define fGEN_TCG_L4_loadrub_ap(SHORTCODE) \
    fGEN_TCG_LOAD_AP(RdV, 1, u)
#define fGEN_TCG_L4_loadrb_ap(SHORTCODE) \
    fGEN_TCG_LOAD_AP(RdV, 1, s)
#define fGEN_TCG_L4_loadruh_ap(SHORTCODE) \
    fGEN_TCG_LOAD_AP(RdV, 2, u)
#define fGEN_TCG_L4_loadrh_ap(SHORTCODE) \
    fGEN_TCG_LOAD_AP(RdV, 2, s)
#define fGEN_TCG_L4_loadri_ap(SHORTCODE) \
    fGEN_TCG_LOAD_AP(RdV, 4, u)
#define fGEN_TCG_L4_loadrd_ap(SHORTCODE) \
    fGEN_TCG_LOAD_AP(RddV, 8, u)

#define fGEN_TCG_L2_loadrub_pci(SHORTCODE)    SHORTCODE
#define fGEN_TCG_L2_loadrb_pci(SHORTCODE)     SHORTCODE
#define fGEN_TCG_L2_loadruh_pci(SHORTCODE)    SHORTCODE
#define fGEN_TCG_L2_loadrh_pci(SHORTCODE)     SHORTCODE
#define fGEN_TCG_L2_loadri_pci(SHORTCODE)     SHORTCODE
#define fGEN_TCG_L2_loadrd_pci(SHORTCODE)     SHORTCODE

#define fGEN_TCG_LOAD_pcr(SHIFT, LOAD) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        tcg_gen_mov_tl(EA, RxV); \
        gen_read_ireg(ireg, MuV, SHIFT); \
        gen_helper_fcircadd(RxV, RxV, ireg, MuV, hex_gpr[HEX_REG_CS0 + MuN]); \
        LOAD; \
        tcg_temp_free(ireg); \
    } while (0)

#define fGEN_TCG_L2_loadrub_pcr(SHORTCODE) \
      fGEN_TCG_LOAD_pcr(0, fLOAD(1, 1, u, EA, RdV))
#define fGEN_TCG_L2_loadrb_pcr(SHORTCODE) \
      fGEN_TCG_LOAD_pcr(0, fLOAD(1, 1, s, EA, RdV))
#define fGEN_TCG_L2_loadruh_pcr(SHORTCODE) \
      fGEN_TCG_LOAD_pcr(1, fLOAD(1, 2, u, EA, RdV))
#define fGEN_TCG_L2_loadrh_pcr(SHORTCODE) \
      fGEN_TCG_LOAD_pcr(1, fLOAD(1, 2, s, EA, RdV))
#define fGEN_TCG_L2_loadri_pcr(SHORTCODE) \
      fGEN_TCG_LOAD_pcr(2, fLOAD(1, 4, u, EA, RdV))
#define fGEN_TCG_L2_loadrd_pcr(SHORTCODE) \
      fGEN_TCG_LOAD_pcr(3, fLOAD(1, 8, u, EA, RddV))

#define fGEN_TCG_L2_loadrub_pr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrub_pbr(SHORTCODE)     SHORTCODE
#define fGEN_TCG_L2_loadrub_pi(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrb_pr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrb_pbr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrb_pi(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadruh_pr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadruh_pbr(SHORTCODE)     SHORTCODE
#define fGEN_TCG_L2_loadruh_pi(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrh_pr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrh_pbr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrh_pi(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadri_pr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadri_pbr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadri_pi(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrd_pr(SHORTCODE)       SHORTCODE
#define fGEN_TCG_L2_loadrd_pbr(SHORTCODE)      SHORTCODE
#define fGEN_TCG_L2_loadrd_pi(SHORTCODE)       SHORTCODE

/*
 * These instructions load 2 bytes and places them in
 * two halves of the destination register.
 * The GET_EA macro determines the addressing mode.
 * The SIGN argument determines whether to zero-extend or
 * sign-extend.
 */
#define fGEN_TCG_loadbXw2(GET_EA, SIGN) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv byte = tcg_temp_new(); \
        GET_EA; \
        fLOAD(1, 2, u, EA, tmp); \
        tcg_gen_movi_tl(RdV, 0); \
        for (int i = 0; i < 2; i++) { \
            gen_set_half(i, RdV, gen_get_byte(byte, i, tmp, (SIGN))); \
        } \
        tcg_temp_free(tmp); \
        tcg_temp_free(byte); \
    } while (0)

#define fGEN_TCG_L2_loadbzw2_io(SHORTCODE) \
    fGEN_TCG_loadbXw2(fEA_RI(RsV, siV), false)
#define fGEN_TCG_L4_loadbzw2_ur(SHORTCODE) \
    fGEN_TCG_loadbXw2(fEA_IRs(UiV, RtV, uiV), false)
#define fGEN_TCG_L2_loadbsw2_io(SHORTCODE) \
    fGEN_TCG_loadbXw2(fEA_RI(RsV, siV), true)
#define fGEN_TCG_L4_loadbsw2_ur(SHORTCODE) \
    fGEN_TCG_loadbXw2(fEA_IRs(UiV, RtV, uiV), true)
#define fGEN_TCG_L4_loadbzw2_ap(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_ap, false)
#define fGEN_TCG_L2_loadbzw2_pr(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pr, false)
#define fGEN_TCG_L2_loadbzw2_pbr(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pbr, false)
#define fGEN_TCG_L2_loadbzw2_pi(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pi, false)
#define fGEN_TCG_L4_loadbsw2_ap(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_ap, true)
#define fGEN_TCG_L2_loadbsw2_pr(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pr, true)
#define fGEN_TCG_L2_loadbsw2_pbr(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pbr, true)
#define fGEN_TCG_L2_loadbsw2_pi(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pi, true)
#define fGEN_TCG_L2_loadbzw2_pci(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pci, false)
#define fGEN_TCG_L2_loadbsw2_pci(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pci, true)
#define fGEN_TCG_L2_loadbzw2_pcr(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pcr(1), false)
#define fGEN_TCG_L2_loadbsw2_pcr(SHORTCODE) \
    fGEN_TCG_loadbXw2(GET_EA_pcr(1), true)

/*
 * These instructions load 4 bytes and places them in
 * four halves of the destination register pair.
 * The GET_EA macro determines the addressing mode.
 * The SIGN argument determines whether to zero-extend or
 * sign-extend.
 */
#define fGEN_TCG_loadbXw4(GET_EA, SIGN) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv byte = tcg_temp_new(); \
        GET_EA; \
        fLOAD(1, 4, u, EA, tmp);  \
        tcg_gen_movi_i64(RddV, 0); \
        for (int i = 0; i < 4; i++) { \
            gen_set_half_i64(i, RddV, gen_get_byte(byte, i, tmp, (SIGN)));  \
        }  \
        tcg_temp_free(tmp); \
        tcg_temp_free(byte); \
    } while (0)

#define fGEN_TCG_L2_loadbzw4_io(SHORTCODE) \
    fGEN_TCG_loadbXw4(fEA_RI(RsV, siV), false)
#define fGEN_TCG_L4_loadbzw4_ur(SHORTCODE) \
    fGEN_TCG_loadbXw4(fEA_IRs(UiV, RtV, uiV), false)
#define fGEN_TCG_L2_loadbsw4_io(SHORTCODE) \
    fGEN_TCG_loadbXw4(fEA_RI(RsV, siV), true)
#define fGEN_TCG_L4_loadbsw4_ur(SHORTCODE) \
    fGEN_TCG_loadbXw4(fEA_IRs(UiV, RtV, uiV), true)
#define fGEN_TCG_L2_loadbzw4_pci(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pci, false)
#define fGEN_TCG_L2_loadbsw4_pci(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pci, true)
#define fGEN_TCG_L2_loadbzw4_pcr(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pcr(2), false)
#define fGEN_TCG_L2_loadbsw4_pcr(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pcr(2), true)
#define fGEN_TCG_L4_loadbzw4_ap(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_ap, false)
#define fGEN_TCG_L2_loadbzw4_pr(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pr, false)
#define fGEN_TCG_L2_loadbzw4_pbr(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pbr, false)
#define fGEN_TCG_L2_loadbzw4_pi(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pi, false)
#define fGEN_TCG_L4_loadbsw4_ap(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_ap, true)
#define fGEN_TCG_L2_loadbsw4_pr(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pr, true)
#define fGEN_TCG_L2_loadbsw4_pbr(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pbr, true)
#define fGEN_TCG_L2_loadbsw4_pi(SHORTCODE) \
    fGEN_TCG_loadbXw4(GET_EA_pi, true)

/*
 * These instructions load a half word, shift the destination right by 16 bits
 * and place the loaded value in the high half word of the destination pair.
 * The GET_EA macro determines the addressing mode.
 */
#define fGEN_TCG_loadalignh(GET_EA) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        GET_EA;  \
        fLOAD(1, 2, u, EA, tmp);  \
        tcg_gen_extu_i32_i64(tmp_i64, tmp); \
        tcg_gen_shri_i64(RyyV, RyyV, 16); \
        tcg_gen_deposit_i64(RyyV, RyyV, tmp_i64, 48, 16); \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fGEN_TCG_L4_loadalignh_ur(SHORTCODE) \
    fGEN_TCG_loadalignh(fEA_IRs(UiV, RtV, uiV))
#define fGEN_TCG_L2_loadalignh_io(SHORTCODE) \
    fGEN_TCG_loadalignh(fEA_RI(RsV, siV))
#define fGEN_TCG_L2_loadalignh_pci(SHORTCODE) \
    fGEN_TCG_loadalignh(GET_EA_pci)
#define fGEN_TCG_L2_loadalignh_pcr(SHORTCODE) \
    fGEN_TCG_loadalignh(GET_EA_pcr(1))
#define fGEN_TCG_L4_loadalignh_ap(SHORTCODE) \
    fGEN_TCG_loadalignh(GET_EA_ap)
#define fGEN_TCG_L2_loadalignh_pr(SHORTCODE) \
    fGEN_TCG_loadalignh(GET_EA_pr)
#define fGEN_TCG_L2_loadalignh_pbr(SHORTCODE) \
    fGEN_TCG_loadalignh(GET_EA_pbr)
#define fGEN_TCG_L2_loadalignh_pi(SHORTCODE) \
    fGEN_TCG_loadalignh(GET_EA_pi)

/* Same as above, but loads a byte instead of half word */
#define fGEN_TCG_loadalignb(GET_EA) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        GET_EA;  \
        fLOAD(1, 1, u, EA, tmp);  \
        tcg_gen_extu_i32_i64(tmp_i64, tmp); \
        tcg_gen_shri_i64(RyyV, RyyV, 8); \
        tcg_gen_deposit_i64(RyyV, RyyV, tmp_i64, 56, 8); \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fGEN_TCG_L2_loadalignb_io(SHORTCODE) \
    fGEN_TCG_loadalignb(fEA_RI(RsV, siV))
#define fGEN_TCG_L4_loadalignb_ur(SHORTCODE) \
    fGEN_TCG_loadalignb(fEA_IRs(UiV, RtV, uiV))
#define fGEN_TCG_L2_loadalignb_pci(SHORTCODE) \
    fGEN_TCG_loadalignb(GET_EA_pci)
#define fGEN_TCG_L2_loadalignb_pcr(SHORTCODE) \
    fGEN_TCG_loadalignb(GET_EA_pcr(0))
#define fGEN_TCG_L4_loadalignb_ap(SHORTCODE) \
    fGEN_TCG_loadalignb(GET_EA_ap)
#define fGEN_TCG_L2_loadalignb_pr(SHORTCODE) \
    fGEN_TCG_loadalignb(GET_EA_pr)
#define fGEN_TCG_L2_loadalignb_pbr(SHORTCODE) \
    fGEN_TCG_loadalignb(GET_EA_pbr)
#define fGEN_TCG_L2_loadalignb_pi(SHORTCODE) \
    fGEN_TCG_loadalignb(GET_EA_pi)

/*
 * Predicated loads
 * Here is a primer to understand the tag names
 *
 * Predicate used
 *      t        true "old" value                  if (p0) r0 = memb(r2+#0)
 *      f        false "old" value                 if (!p0) r0 = memb(r2+#0)
 *      tnew     true "new" value                  if (p0.new) r0 = memb(r2+#0)
 *      fnew     false "new" value                 if (!p0.new) r0 = memb(r2+#0)
 */
#define fGEN_TCG_PRED_LOAD(GET_EA, PRED, SIZE, SIGN) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGLabel *label = gen_new_label(); \
        PRED;  \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, label); \
        tcg_temp_free(LSB); \
        GET_EA; \
        fLOAD(1, SIZE, SIGN, EA, RdV); \
        gen_set_label(label); \
    } while (0)

#define fGEN_TCG_L2_ploadrubt_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 1, u)
#define fGEN_TCG_L2_ploadrubt_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 1, u)
#define fGEN_TCG_L2_ploadrubf_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 1, u)
#define fGEN_TCG_L2_ploadrubf_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 1, u)
#define fGEN_TCG_L2_ploadrubtnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 1, u)
#define fGEN_TCG_L2_ploadrubfnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 1, u)
#define fGEN_TCG_L4_ploadrubt_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 1, u)
#define fGEN_TCG_L4_ploadrubf_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 1, u)
#define fGEN_TCG_L4_ploadrubtnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 1, u)
#define fGEN_TCG_L4_ploadrubfnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 1, u)
#define fGEN_TCG_L2_ploadrubtnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 1, u)
#define fGEN_TCG_L2_ploadrubfnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 1, u)
#define fGEN_TCG_L4_ploadrubt_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 1, u)
#define fGEN_TCG_L4_ploadrubf_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 1, u)
#define fGEN_TCG_L4_ploadrubtnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 1, u)
#define fGEN_TCG_L4_ploadrubfnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 1, u)
#define fGEN_TCG_L2_ploadrbt_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 1, s)
#define fGEN_TCG_L2_ploadrbt_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 1, s)
#define fGEN_TCG_L2_ploadrbf_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 1, s)
#define fGEN_TCG_L2_ploadrbf_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 1, s)
#define fGEN_TCG_L2_ploadrbtnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 1, s)
#define fGEN_TCG_L2_ploadrbfnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 1, s)
#define fGEN_TCG_L4_ploadrbt_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 1, s)
#define fGEN_TCG_L4_ploadrbf_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 1, s)
#define fGEN_TCG_L4_ploadrbtnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 1, s)
#define fGEN_TCG_L4_ploadrbfnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 1, s)
#define fGEN_TCG_L2_ploadrbtnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 1, s)
#define fGEN_TCG_L2_ploadrbfnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD({ fEA_REG(RxV); fPM_I(RxV, siV); }, \
                       fLSBNEWNOT(PtN), 1, s)
#define fGEN_TCG_L4_ploadrbt_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 1, s)
#define fGEN_TCG_L4_ploadrbf_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 1, s)
#define fGEN_TCG_L4_ploadrbtnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 1, s)
#define fGEN_TCG_L4_ploadrbfnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 1, s)

#define fGEN_TCG_L2_ploadruht_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 2, u)
#define fGEN_TCG_L2_ploadruht_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 2, u)
#define fGEN_TCG_L2_ploadruhf_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 2, u)
#define fGEN_TCG_L2_ploadruhf_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 2, u)
#define fGEN_TCG_L2_ploadruhtnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 2, u)
#define fGEN_TCG_L2_ploadruhfnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 2, u)
#define fGEN_TCG_L4_ploadruht_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 2, u)
#define fGEN_TCG_L4_ploadruhf_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 2, u)
#define fGEN_TCG_L4_ploadruhtnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 2, u)
#define fGEN_TCG_L4_ploadruhfnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 2, u)
#define fGEN_TCG_L2_ploadruhtnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 2, u)
#define fGEN_TCG_L2_ploadruhfnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 2, u)
#define fGEN_TCG_L4_ploadruht_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 2, u)
#define fGEN_TCG_L4_ploadruhf_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 2, u)
#define fGEN_TCG_L4_ploadruhtnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 2, u)
#define fGEN_TCG_L4_ploadruhfnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 2, u)
#define fGEN_TCG_L2_ploadrht_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 2, s)
#define fGEN_TCG_L2_ploadrht_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 2, s)
#define fGEN_TCG_L2_ploadrhf_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 2, s)
#define fGEN_TCG_L2_ploadrhf_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 2, s)
#define fGEN_TCG_L2_ploadrhtnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 2, s)
#define fGEN_TCG_L2_ploadrhfnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 2, s)
#define fGEN_TCG_L4_ploadrht_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 2, s)
#define fGEN_TCG_L4_ploadrhf_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 2, s)
#define fGEN_TCG_L4_ploadrhtnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 2, s)
#define fGEN_TCG_L4_ploadrhfnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 2, s)
#define fGEN_TCG_L2_ploadrhtnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 2, s)
#define fGEN_TCG_L2_ploadrhfnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 2, s)
#define fGEN_TCG_L4_ploadrht_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 2, s)
#define fGEN_TCG_L4_ploadrhf_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 2, s)
#define fGEN_TCG_L4_ploadrhtnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 2, s)
#define fGEN_TCG_L4_ploadrhfnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 2, s)

#define fGEN_TCG_L2_ploadrit_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLD(PtV), 4, u)
#define fGEN_TCG_L2_ploadrit_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLD(PtV), 4, u)
#define fGEN_TCG_L2_ploadrif_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV), 4, u)
#define fGEN_TCG_L2_ploadrif_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBOLDNOT(PtV), 4, u)
#define fGEN_TCG_L2_ploadritnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEW(PtN), 4, u)
#define fGEN_TCG_L2_ploadrifnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN), 4, u)
#define fGEN_TCG_L4_ploadrit_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV), 4, u)
#define fGEN_TCG_L4_ploadrif_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV), 4, u)
#define fGEN_TCG_L4_ploadritnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN), 4, u)
#define fGEN_TCG_L4_ploadrifnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN), 4, u)
#define fGEN_TCG_L2_ploadritnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEW(PtN), 4, u)
#define fGEN_TCG_L2_ploadrifnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(GET_EA_pi, fLSBNEWNOT(PtN), 4, u)
#define fGEN_TCG_L4_ploadrit_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLD(PtV), 4, u)
#define fGEN_TCG_L4_ploadrif_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBOLDNOT(PtV), 4, u)
#define fGEN_TCG_L4_ploadritnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEW(PtN), 4, u)
#define fGEN_TCG_L4_ploadrifnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD(fEA_IMM(uiV), fLSBNEWNOT(PtN), 4, u)

/* Predicated loads into a register pair */
#define fGEN_TCG_PRED_LOAD_PAIR(GET_EA, PRED) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGLabel *label = gen_new_label(); \
        PRED;  \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, label); \
        tcg_temp_free(LSB); \
        GET_EA; \
        fLOAD(1, 8, u, EA, RddV); \
        gen_set_label(label); \
    } while (0)

#define fGEN_TCG_L2_ploadrdt_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBOLD(PtV))
#define fGEN_TCG_L2_ploadrdt_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(GET_EA_pi, fLSBOLD(PtV))
#define fGEN_TCG_L2_ploadrdf_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBOLDNOT(PtV))
#define fGEN_TCG_L2_ploadrdf_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(GET_EA_pi, fLSBOLDNOT(PtV))
#define fGEN_TCG_L2_ploadrdtnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBNEW(PtN))
#define fGEN_TCG_L2_ploadrdfnew_io(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RI(RsV, uiV), fLSBNEWNOT(PtN))
#define fGEN_TCG_L4_ploadrdt_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBOLD(PvV))
#define fGEN_TCG_L4_ploadrdf_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBOLDNOT(PvV))
#define fGEN_TCG_L4_ploadrdtnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBNEW(PvN))
#define fGEN_TCG_L4_ploadrdfnew_rr(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_RRs(RsV, RtV, uiV), fLSBNEWNOT(PvN))
#define fGEN_TCG_L2_ploadrdtnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(GET_EA_pi, fLSBNEW(PtN))
#define fGEN_TCG_L2_ploadrdfnew_pi(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(GET_EA_pi, fLSBNEWNOT(PtN))
#define fGEN_TCG_L4_ploadrdt_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBOLD(PtV))
#define fGEN_TCG_L4_ploadrdf_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBOLDNOT(PtV))
#define fGEN_TCG_L4_ploadrdtnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBNEW(PtN))
#define fGEN_TCG_L4_ploadrdfnew_abs(SHORTCODE) \
    fGEN_TCG_PRED_LOAD_PAIR(fEA_IMM(uiV), fLSBNEWNOT(PtN))

/* load-locked and store-locked */
#define fGEN_TCG_L2_loadw_locked(SHORTCODE) \
    SHORTCODE
#define fGEN_TCG_L4_loadd_locked(SHORTCODE) \
    SHORTCODE
#define fGEN_TCG_S2_storew_locked(SHORTCODE) \
    SHORTCODE
#define fGEN_TCG_S4_stored_locked(SHORTCODE) \
    SHORTCODE

#define fGEN_TCG_STORE(SHORTCODE) \
    do { \
        TCGv HALF = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        SHORTCODE; \
        tcg_temp_free(HALF); \
        tcg_temp_free(BYTE); \
    } while (0)

#define fGEN_TCG_STORE_ap(STORE) \
    do { \
        TCGv HALF = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        { \
            fEA_IMM(UiV); \
            STORE; \
            tcg_gen_movi_tl(ReV, UiV); \
        } \
        tcg_temp_free(HALF); \
        tcg_temp_free(BYTE); \
    } while (0)

#define fGEN_TCG_STORE_pcr(SHIFT, STORE) \
    do { \
        TCGv ireg = tcg_temp_new(); \
        TCGv HALF = tcg_temp_new(); \
        TCGv BYTE = tcg_temp_new(); \
        tcg_gen_mov_tl(EA, RxV); \
        gen_read_ireg(ireg, MuV, SHIFT); \
        gen_helper_fcircadd(RxV, RxV, ireg, MuV, hex_gpr[HEX_REG_CS0 + MuN]); \
        STORE; \
        tcg_temp_free(ireg); \
        tcg_temp_free(HALF); \
        tcg_temp_free(BYTE); \
    } while (0)

#define fGEN_TCG_S2_storerb_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerb_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerb_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 1, EA, fGETBYTE(0, RtV)))
#define fGEN_TCG_S2_storerb_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerb_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerb_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerb_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerb_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(0, fSTORE(1, 1, EA, fGETBYTE(0, RtV)))
#define fGEN_TCG_S4_storerb_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerbnew_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storeirb_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerbgp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS1_storeb_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS2_storebi0(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS2_storebi1(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)

#define fGEN_TCG_S2_storerh_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerh_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerh_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 2, EA, fGETHALF(0, RtV)))
#define fGEN_TCG_S2_storerh_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerh_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerh_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerh_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerh_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(1, fSTORE(1, 2, EA, fGETHALF(0, RtV)))
#define fGEN_TCG_S4_storerh_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storeirh_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerhgp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS2_storeh_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)

#define fGEN_TCG_S2_storerf_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerf_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerf_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 2, EA, fGETHALF(1, RtV)))
#define fGEN_TCG_S2_storerf_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerf_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerf_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerf_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerf_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(1, fSTORE(1, 2, EA, fGETHALF(1, RtV)))
#define fGEN_TCG_S4_storerf_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerfgp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)

#define fGEN_TCG_S2_storeri_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storeri_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storeri_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 4, EA, RtV))
#define fGEN_TCG_S2_storeri_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storeri_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storeri_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storeri_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storeri_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(2, fSTORE(1, 4, EA, RtV))
#define fGEN_TCG_S4_storeri_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerinew_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storeiri_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerigp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS1_storew_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS2_storew_sp(SHORTCODE) \
    do { \
        tcg_gen_addi_tl(EA, hex_gpr[HEX_REG_SP], uiV); \
        fSTORE(1, 4, EA, RtV); \
    } while (0)
#define fGEN_TCG_SS2_storewi0(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS2_storewi1(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)

#define fGEN_TCG_S2_storerd_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerd_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerd_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 8, EA, RttV))
#define fGEN_TCG_S2_storerd_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerd_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerd_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerd_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerd_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(3, fSTORE(1, 8, EA, RttV))
#define fGEN_TCG_S4_storerd_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerdgp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_SS2_stored_sp(SHORTCODE) \
    do { \
        tcg_gen_addi_tl(EA, hex_gpr[HEX_REG_SP], siV); \
        fSTORE(1, 8, EA, RttV); \
    } while (0)

#define fGEN_TCG_S2_storerbnew_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerbnew_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerbnew_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 1, EA, fGETBYTE(0, NtN)))
#define fGEN_TCG_S2_storerbnew_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerbnew_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerbnew_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerbnew_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerbnew_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(0, fSTORE(1, 1, EA, fGETBYTE(0, NtN)))
#define fGEN_TCG_S2_storerbnewgp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)

#define fGEN_TCG_S2_storerhnew_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerhnew_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerhnew_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 2, EA, fGETHALF(0, NtN)))
#define fGEN_TCG_S2_storerhnew_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerhnew_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerhnew_rr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerhnew_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerhnew_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerhnew_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(1, fSTORE(1, 2, EA, fGETHALF(0, NtN)))
#define fGEN_TCG_S2_storerhnewgp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)

#define fGEN_TCG_S2_storerinew_io(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerinew_pi(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerinew_ap(SHORTCODE) \
    fGEN_TCG_STORE_ap(fSTORE(1, 4, EA, NtN))
#define fGEN_TCG_S2_storerinew_pr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S4_storerinew_ur(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerinew_pbr(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerinew_pci(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)
#define fGEN_TCG_S2_storerinew_pcr(SHORTCODE) \
    fGEN_TCG_STORE_pcr(2, fSTORE(1, 4, EA, NtN))
#define fGEN_TCG_S2_storerinewgp(SHORTCODE) \
    fGEN_TCG_STORE(SHORTCODE)

/* Predicated stores */
#define fGEN_TCG_PRED_STORE(GET_EA, PRED, SRC, SIZE, INC) \
    do { \
        TCGv LSB = tcg_temp_local_new(); \
        TCGv BYTE = tcg_temp_local_new(); \
        TCGv HALF = tcg_temp_local_new(); \
        TCGLabel *false_label = gen_new_label(); \
        TCGLabel *end_label = gen_new_label(); \
        GET_EA; \
        PRED;  \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, false_label); \
        INC; \
        fSTORE(1, SIZE, EA, SRC); \
        tcg_gen_br(end_label); \
        gen_set_label(false_label); \
        tcg_gen_ori_tl(hex_slot_cancelled, hex_slot_cancelled, \
                       1 << insn->slot); \
        gen_set_label(end_label); \
        tcg_temp_free(LSB); \
        tcg_temp_free(BYTE); \
        tcg_temp_free(HALF); \
    } while (0)

#define NOINC    do {} while (0)

#define fGEN_TCG_S4_storeirbt_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLD(PvV), SiV, 1, NOINC)
#define fGEN_TCG_S4_storeirbf_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLDNOT(PvV), SiV, 1, NOINC)
#define fGEN_TCG_S4_storeirbtnew_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEW(PvN), SiV, 1, NOINC)
#define fGEN_TCG_S4_storeirbfnew_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEWNOT(PvN), SiV, 1, NOINC)

#define fGEN_TCG_S4_storeirht_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLD(PvV), SiV, 2, NOINC)
#define fGEN_TCG_S4_storeirhf_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLDNOT(PvV), SiV, 2, NOINC)
#define fGEN_TCG_S4_storeirhtnew_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEW(PvN), SiV, 2, NOINC)
#define fGEN_TCG_S4_storeirhfnew_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEWNOT(PvN), SiV, 2, NOINC)

#define fGEN_TCG_S4_storeirit_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLD(PvV), SiV, 4, NOINC)
#define fGEN_TCG_S4_storeirif_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBOLDNOT(PvV), SiV, 4, NOINC)
#define fGEN_TCG_S4_storeiritnew_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEW(PvN), SiV, 4, NOINC)
#define fGEN_TCG_S4_storeirifnew_io(SHORTCODE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), fLSBNEWNOT(PvN), SiV, 4, NOINC)

/*
 * There are a total of 128 pstore instructions.
 * Lots of variations on the same pattern
 * - 4 addressing modes
 * - old or new value of the predicate
 * - old or new value to store
 * - size of the operand (byte, half word, word, double word)
 * - true or false sense of the predicate
 * - half word stores can also use the high half or the register
 *
 * We use the C preprocessor to our advantage when building the combinations.
 *
 * The instructions are organized by addressing mode.  For each addressing mode,
 * there are 4 macros to help out
 * - old predicate, old value
 * - new predicate, old value
 * - old predicate, new value
 * - new predicate, new value
 */

/*
 ****************************************************************************
 * _rr addressing mode (base + index << shift)
 */
#define fGEN_TCG_pstoreX_rr(PRED, VAL, SIZE) \
    fGEN_TCG_PRED_STORE(fEA_RRs(RsV, RuV, uiV), PRED, VAL, SIZE, NOINC)

/* Byte (old value) */
#define fGEN_TCG_pstoreX_rr_byte_old(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, fGETBYTE(0, RtV), 1)

#define fGEN_TCG_S4_pstorerbt_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerbf_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerbtnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerbfnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_old(fLSBNEWNOT(PvN))

/* Byte (new value) */
#define fGEN_TCG_pstoreX_rr_byte_new(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, fGETBYTE(0, NtN), 1)

#define fGEN_TCG_S4_pstorerbnewt_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_new(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerbnewf_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerbnewtnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerbnewfnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_byte_new(fLSBNEWNOT(PvN))

/* Half word (old value) */
#define fGEN_TCG_pstoreX_rr_half_old(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, fGETHALF(0, RtV), 2)

#define fGEN_TCG_S4_pstorerht_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerhf_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerhtnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerhfnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_old(fLSBNEWNOT(PvN))

/* Half word (new value) */
#define fGEN_TCG_pstoreX_rr_half_new(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, fGETHALF(0, NtN), 2)

#define fGEN_TCG_S4_pstorerhnewt_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_new(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerhnewf_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerhnewtnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerhnewfnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_new(fLSBNEWNOT(PvN))

/* High half word */
#define fGEN_TCG_pstoreX_rr_half_high(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, fGETHALF(1, RtV), 2)

#define fGEN_TCG_S4_pstorerft_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_high(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerff_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_high(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerftnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_high(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerffnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_half_high(fLSBNEWNOT(PvN))

/* Word (old value) */
#define fGEN_TCG_pstoreX_rr_word_old(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, RtV, 4)

#define fGEN_TCG_S4_pstorerit_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerif_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstoreritnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerifnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_old(fLSBNEWNOT(PvN))

/* Word (new value) */
#define fGEN_TCG_pstoreX_rr_word_new(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, NtN, 4)

#define fGEN_TCG_S4_pstorerinewt_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_new(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerinewf_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerinewtnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerinewfnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_word_new(fLSBNEWNOT(PvN))

/* Double word (old value) */
#define fGEN_TCG_pstoreX_rr_double_old(PRED) \
    fGEN_TCG_pstoreX_rr(PRED, RttV, 8)

#define fGEN_TCG_S4_pstorerdt_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_double_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerdf_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_double_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerdtnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_double_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerdfnew_rr(SHORTCODE) \
    fGEN_TCG_pstoreX_rr_double_old(fLSBNEWNOT(PvN))

/*
 ****************************************************************************
 * _io addressing mode (addr + increment)
 */
#define fGEN_TCG_pstoreX_io(PRED, VAL, SIZE) \
    fGEN_TCG_PRED_STORE(fEA_RI(RsV, uiV), PRED, VAL, SIZE, NOINC)

/* Byte (old value) */
#define fGEN_TCG_pstoreX_io_byte_old(PRED) \
    fGEN_TCG_pstoreX_io(PRED, fGETBYTE(0, RtV), 1)

#define fGEN_TCG_S2_pstorerbt_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerbf_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerbtnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerbfnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_old(fLSBNEWNOT(PvN))

/* Byte (new value) */
#define fGEN_TCG_pstoreX_io_byte_new(PRED) \
    fGEN_TCG_pstoreX_io(PRED, fGETBYTE(0, NtN), 1)

#define fGEN_TCG_S2_pstorerbnewt_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_new(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerbnewf_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerbnewtnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerbnewfnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_byte_new(fLSBNEWNOT(PvN))

/* Half word (old value) */
#define fGEN_TCG_pstoreX_io_half_old(PRED) \
    fGEN_TCG_pstoreX_io(PRED, fGETHALF(0, RtV), 2)

#define fGEN_TCG_S2_pstorerht_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerhf_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerhtnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerhfnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_old(fLSBNEWNOT(PvN))

/* Half word (new value) */
#define fGEN_TCG_pstoreX_io_half_new(PRED) \
    fGEN_TCG_pstoreX_io(PRED, fGETHALF(0, NtN), 2)

#define fGEN_TCG_S2_pstorerhnewt_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_new(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerhnewf_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerhnewtnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerhnewfnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_new(fLSBNEWNOT(PvN))

/* High half word */
#define fGEN_TCG_pstoreX_io_half_high(PRED) \
    fGEN_TCG_pstoreX_io(PRED, fGETHALF(1, RtV), 2)

#define fGEN_TCG_S2_pstorerft_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_high(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerff_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_high(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerftnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_high(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerffnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_half_high(fLSBNEWNOT(PvN))

/* Word (old value) */
#define fGEN_TCG_pstoreX_io_word_old(PRED) \
    fGEN_TCG_pstoreX_io(PRED, RtV, 4)

#define fGEN_TCG_S2_pstorerit_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerif_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstoreritnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerifnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_old(fLSBNEWNOT(PvN))

/* Word (new value) */
#define fGEN_TCG_pstoreX_io_word_new(PRED) \
    fGEN_TCG_pstoreX_io(PRED, NtN, 4)

#define fGEN_TCG_S2_pstorerinewt_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_new(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerinewf_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerinewtnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerinewfnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_word_new(fLSBNEWNOT(PvN))

/* Double word (old value) */
#define fGEN_TCG_pstoreX_io_double_old(PRED) \
    fGEN_TCG_pstoreX_io(PRED, RttV, 8)

#define fGEN_TCG_S2_pstorerdt_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_double_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerdf_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_double_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerdtnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_double_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerdfnew_io(SHORTCODE) \
    fGEN_TCG_pstoreX_io_double_old(fLSBNEWNOT(PvN))

/*
 ****************************************************************************
 * _abs addressing mode (##addr)
 */
#define fGEN_TCG_pstoreX_abs(PRED, VAL, SIZE) \
    fGEN_TCG_PRED_STORE(fEA_IMM(uiV), PRED, VAL, SIZE, NOINC)

/* Byte (old value) */
#define fGEN_TCG_pstoreX_abs_byte_old(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, fGETBYTE(0, RtV), 1)

#define fGEN_TCG_S4_pstorerbt_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerbf_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerbtnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerbfnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_old(fLSBNEWNOT(PvN))

/* Byte (new value) */
#define fGEN_TCG_pstoreX_abs_byte_new(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, fGETBYTE(0, NtN), 1)

#define fGEN_TCG_S4_pstorerbnewt_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_new(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerbnewf_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerbnewtnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerbnewfnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_byte_new(fLSBNEWNOT(PvN))

/* Half word (old value) */
#define fGEN_TCG_pstoreX_abs_half_old(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, fGETHALF(0, RtV), 2)

#define fGEN_TCG_S4_pstorerht_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerhf_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerhtnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerhfnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_old(fLSBNEWNOT(PvN))

/* Half word (new value) */
#define fGEN_TCG_pstoreX_abs_half_new(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, fGETHALF(0, NtN), 2)

#define fGEN_TCG_S4_pstorerhnewt_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_new(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerhnewf_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerhnewtnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerhnewfnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_new(fLSBNEWNOT(PvN))

/* High half word */
#define fGEN_TCG_pstoreX_abs_half_high(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, fGETHALF(1, RtV), 2)

#define fGEN_TCG_S4_pstorerft_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_high(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerff_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_high(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerftnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_high(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerffnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_half_high(fLSBNEWNOT(PvN))

/* Word (old value) */
#define fGEN_TCG_pstoreX_abs_word_old(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, RtV, 4)

#define fGEN_TCG_S4_pstorerit_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerif_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstoreritnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerifnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_old(fLSBNEWNOT(PvN))

/* Word (new value) */
#define fGEN_TCG_pstoreX_abs_word_new(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, NtN, 4)

#define fGEN_TCG_S4_pstorerinewt_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_new(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerinewf_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerinewtnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_new(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerinewfnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_word_new(fLSBNEWNOT(PvN))

/* Double word (old value) */
#define fGEN_TCG_pstoreX_abs_double_old(PRED) \
    fGEN_TCG_pstoreX_abs(PRED, RttV, 8)

#define fGEN_TCG_S4_pstorerdt_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_double_old(fLSBOLD(PvV))
#define fGEN_TCG_S4_pstorerdf_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_double_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S4_pstorerdtnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_double_old(fLSBNEW(PvN))
#define fGEN_TCG_S4_pstorerdfnew_abs(SHORTCODE) \
    fGEN_TCG_pstoreX_abs_double_old(fLSBNEWNOT(PvN))

/*
 ****************************************************************************
 * _pi addressing mode (addr ++ increment)
 */
#define fGEN_TCG_pstoreX_pi(PRED, VAL, SIZE) \
    fGEN_TCG_PRED_STORE(fEA_REG(RxV), PRED, VAL, SIZE, fPM_I(RxV, siV))

/* Byte (old value) */
#define fGEN_TCG_pstoreX_pi_byte_old(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, fGETBYTE(0, RtV), 1)

#define fGEN_TCG_S2_pstorerbt_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerbf_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstorerbtnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_old(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerbfnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_old(fLSBNEWNOT(PvN))

/* Byte (new value) */
#define fGEN_TCG_pstoreX_pi_byte_new(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, fGETBYTE(0, NtN), 1)

#define fGEN_TCG_S2_pstorerbnewt_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_new(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerbnewf_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstorerbnewtnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_new(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerbnewfnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_byte_new(fLSBNEWNOT(PvN))

/* Half word (old value) */
#define fGEN_TCG_pstoreX_pi_half_old(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, fGETHALF(0, RtV), 2)

#define fGEN_TCG_S2_pstorerht_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerhf_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstorerhtnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_old(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerhfnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_old(fLSBNEWNOT(PvN))

/* Half word (new value) */
#define fGEN_TCG_pstoreX_pi_half_new(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, fGETHALF(0, NtN), 2)

#define fGEN_TCG_S2_pstorerhnewt_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_new(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerhnewf_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstorerhnewtnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_new(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerhnewfnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_new(fLSBNEWNOT(PvN))

/* High half word */
#define fGEN_TCG_pstoreX_pi_half_high(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, fGETHALF(1, RtV), 2)

#define fGEN_TCG_S2_pstorerft_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_high(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerff_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_high(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstorerftnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_high(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerffnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_half_high(fLSBNEWNOT(PvN))

/* Word (old value) */
#define fGEN_TCG_pstoreX_pi_word_old(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, RtV, 4)

#define fGEN_TCG_S2_pstorerit_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerif_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstoreritnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_old(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerifnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_old(fLSBNEWNOT(PvN))

/* Word (new value) */
#define fGEN_TCG_pstoreX_pi_word_new(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, NtN, 4)

#define fGEN_TCG_S2_pstorerinewt_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_new(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerinewf_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_new(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstorerinewtnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_new(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerinewfnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_word_new(fLSBNEWNOT(PvN))

/* Double word (old value) */
#define fGEN_TCG_pstoreX_pi_double_old(PRED) \
    fGEN_TCG_pstoreX_pi(PRED, RttV, 8)

#define fGEN_TCG_S2_pstorerdt_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_double_old(fLSBOLD(PvV))
#define fGEN_TCG_S2_pstorerdf_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_double_old(fLSBOLDNOT(PvV))
#define fGEN_TCG_S2_pstorerdtnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_double_old(fLSBNEW(PvN))
#define fGEN_TCG_S2_pstorerdfnew_pi(SHORTCODE) \
    fGEN_TCG_pstoreX_pi_double_old(fLSBNEWNOT(PvN))
/*
 ****************************************************************************
 * Whew!  That's the end of the predicated stores.
 ****************************************************************************
 */

/* We have to brute force memops because they have C math in the semantics */
#define fGEN_TCG_MEMOP(SHORTCODE, SIZE, OP) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        fEA_RI(RsV, uiV); \
        fLOAD(1, SIZE, s, EA, tmp); \
        OP; \
        fSTORE(1, SIZE, EA, tmp); \
        tcg_temp_free(tmp); \
    } while (0)

#define fGEN_TCG_L4_add_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_add_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_add_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_add_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_add_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_add_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_sub_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_sub_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_sub_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_sub_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_sub_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_sub_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_and_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_and_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_and_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_and_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_and_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_and_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_or_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_or_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_or_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_or_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_or_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_or_tl(tmp, tmp, RtV))
#define fGEN_TCG_L4_iadd_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_addi_tl(tmp, tmp, UiV))
#define fGEN_TCG_L4_iadd_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_addi_tl(tmp, tmp, UiV))
#define fGEN_TCG_L4_iadd_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_addi_tl(tmp, tmp, UiV))
#define fGEN_TCG_L4_isub_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_subi_tl(tmp, tmp, UiV))
#define fGEN_TCG_L4_isub_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_subi_tl(tmp, tmp, UiV))
#define fGEN_TCG_L4_isub_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_subi_tl(tmp, tmp, UiV))
#define fGEN_TCG_L4_iand_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_andi_tl(tmp, tmp, ~(1 << UiV)))
#define fGEN_TCG_L4_iand_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_andi_tl(tmp, tmp, ~(1 << UiV)))
#define fGEN_TCG_L4_iand_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_andi_tl(tmp, tmp, ~(1 << UiV)))
#define fGEN_TCG_L4_ior_memopw_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 4, tcg_gen_ori_tl(tmp, tmp, 1 << UiV))
#define fGEN_TCG_L4_ior_memopb_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 1, tcg_gen_ori_tl(tmp, tmp, 1 << UiV))
#define fGEN_TCG_L4_ior_memoph_io(SHORTCODE) \
    fGEN_TCG_MEMOP(SHORTCODE, 2, tcg_gen_ori_tl(tmp, tmp, 1 << UiV))

/* dczeroa clears the 32 byte cache line at the address given */
#define fGEN_TCG_Y2_dczeroa(SHORTCODE) SHORTCODE

/* We have to brute force allocframe because it has C math in the semantics */
#define fGEN_TCG_S2_allocframe(SHORTCODE) \
    do { \
        TCGv_i64 scramble_tmp = tcg_temp_new_i64(); \
        { fEA_RI(RxV, -8); \
          gen_frame_scramble(scramble_tmp); \
          fSTORE(1, 8, EA, scramble_tmp); \
          gen_log_reg_write(HEX_REG_FP, EA); \
          fFRAMECHECK(EA - uiV, EA); \
          tcg_gen_subi_tl(RxV, EA, uiV); \
        } \
        tcg_temp_free_i64(scramble_tmp); \
    } while (0)

#define fGEN_TCG_SS2_allocframe(SHORTCODE) \
    do { \
        TCGv_i64 scramble_tmp = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        { tcg_gen_addi_tl(EA, hex_gpr[HEX_REG_SP], -8); \
          gen_frame_scramble(scramble_tmp); \
          fSTORE(1, 8, EA, scramble_tmp); \
          gen_log_reg_write(HEX_REG_FP, EA); \
          fFRAMECHECK(EA - uiV, EA); \
          tcg_gen_subi_tl(tmp, EA, uiV); \
          gen_log_reg_write(HEX_REG_SP, tmp); \
        } \
        tcg_temp_free_i64(scramble_tmp); \
        tcg_temp_free(tmp); \
    } while (0)

/* Also have to brute force the deallocframe variants */
#define fGEN_TCG_L2_deallocframe(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        { \
          fEA_REG(RsV); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          tcg_gen_mov_i64(RddV, gen_frame_unscramble(tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          gen_log_reg_write(HEX_REG_SP, tmp); \
        } \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fGEN_TCG_SL2_deallocframe(SHORTCODE) \
    do { \
        TCGv WORD = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        { \
          fEA_REG(gen_read_reg(tmp, HEX_REG_FP)); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          gen_frame_unscramble(tmp_i64); \
          gen_log_reg_write(HEX_REG_LR, fGETWORD(1, tmp_i64)); \
          gen_log_reg_write(HEX_REG_FP, fGETWORD(0, tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          gen_log_reg_write(HEX_REG_SP, tmp); \
        } \
        tcg_temp_free(WORD); \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
    } while (0)

#define fGEN_TCG_L4_return(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv WORD = tcg_temp_new(); \
        { \
          fEA_REG(RsV); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          tcg_gen_mov_i64(RddV, gen_frame_unscramble(tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          gen_log_reg_write(HEX_REG_SP, tmp); \
          gen_write_new_pc(pkt, fGETWORD(1, RddV)); \
        } \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(WORD); \
    } while (0)

#define fGEN_TCG_SL2_return(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv WORD = tcg_temp_new(); \
        { \
          fEA_REG(gen_read_reg(tmp, HEX_REG_FP)); \
          fLOAD(1, 8, u, EA, tmp_i64); \
          gen_frame_unscramble(tmp_i64); \
          gen_log_reg_write(HEX_REG_LR, fGETWORD(1, tmp_i64)); \
          gen_log_reg_write(HEX_REG_FP, fGETWORD(0, tmp_i64)); \
          tcg_gen_addi_tl(tmp, EA, 8); \
          gen_log_reg_write(HEX_REG_SP, tmp); \
          gen_write_new_pc(pkt, fGETWORD(1, tmp_i64)); \
        } \
        tcg_temp_free(tmp); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(WORD); \
    } while (0)

/*
 * Conditional returns follow the same predicate naming convention as
 * predicated loads above
 */
#define fGEN_TCG_COND_RETURN(PRED) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv WORD = tcg_temp_new(); \
        TCGLabel *skip = gen_new_label(); \
        fEA_REG(RsV); \
        PRED; \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, skip); \
        tcg_temp_free(LSB); \
        fLOAD(1, 8, u, EA, tmp_i64); \
        tcg_gen_mov_i64(RddV, gen_frame_unscramble(tmp_i64)); \
        tcg_gen_addi_tl(tmp, EA, 8); \
        gen_log_reg_write(HEX_REG_SP, tmp); \
        gen_write_new_pc(pkt, fGETWORD(1, RddV)); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(tmp); \
        tcg_temp_free(WORD); \
        gen_set_label(skip); \
    } while (0)

#define fGEN_TCG_L4_return_t(SHORTCODE) \
    fGEN_TCG_COND_RETURN(fLSBOLD(PvV))
#define fGEN_TCG_L4_return_f(SHORTCODE) \
    fGEN_TCG_COND_RETURN(fLSBOLDNOT(PvV))
#define fGEN_TCG_L4_return_tnew_pt(SHORTCODE) \
    fGEN_TCG_COND_RETURN(fLSBNEW(PvN))
#define fGEN_TCG_L4_return_fnew_pt(SHORTCODE) \
    fGEN_TCG_COND_RETURN(fLSBNEWNOT(PvN))
#define fGEN_TCG_L4_return_tnew_pnt(SHORTCODE) \
    fGEN_TCG_COND_RETURN(fLSBNEW(PvN))
#define fGEN_TCG_L4_return_tnew_pnt(SHORTCODE) \
    fGEN_TCG_COND_RETURN(fLSBNEW(PvN))
#define fGEN_TCG_L4_return_fnew_pnt(SHORTCODE) \
    fGEN_TCG_COND_RETURN(fLSBNEWNOT(PvN))

#define fGEN_TCG_COND_RETURN_SUBINSN(PRED) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGv_i64 RddV = tcg_temp_new_i64(); \
        TCGv_i64 tmp_i64 = tcg_temp_new_i64(); \
        TCGv tmp = tcg_temp_new(); \
        TCGv WORD = tcg_temp_new(); \
        TCGLabel *skip = gen_new_label(); \
        fEA_REG(gen_read_reg(tmp, HEX_REG_FP)); \
        PRED; \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, skip); \
        fLOAD(1, 8, u, EA, tmp_i64); \
        tcg_gen_mov_i64(RddV, gen_frame_unscramble(tmp_i64)); \
        tcg_temp_free(LSB); \
        tcg_gen_addi_tl(tmp, EA, 8); \
        gen_log_reg_write(HEX_REG_SP, tmp); \
        gen_write_new_pc(pkt, fGETWORD(1, RddV)); \
        gen_log_reg_write_pair(HEX_REG_FP, RddV); \
        tcg_temp_free_i64(RddV); \
        tcg_temp_free_i64(tmp_i64); \
        tcg_temp_free(tmp); \
        tcg_temp_free(WORD); \
        gen_set_label(skip); \
    } while (0)

#define fGEN_TCG_SL2_return_t(SHORTCODE) \
    fGEN_TCG_COND_RETURN_SUBINSN(fLSBOLD(gen_read_preg(tmp, 0)))
#define fGEN_TCG_SL2_return_f(SHORTCODE) \
    fGEN_TCG_COND_RETURN_SUBINSN(fLSBOLDNOT(gen_read_preg(tmp, 0)))
#define fGEN_TCG_SL2_return_tnew(SHORTCODE) \
    fGEN_TCG_COND_RETURN_SUBINSN(fLSBNEW0)
#define fGEN_TCG_SL2_return_fnew(SHORTCODE) \
    fGEN_TCG_COND_RETURN_SUBINSN(fLSBNEW0NOT)

/*
 * Mathematical operations with more than one definition require
 * special handling
 */
#define fGEN_TCG_A5_ACS(SHORTCODE) \
    do { \
        gen_helper_vacsh_pred(PeV, cpu_env, RxxV, RssV, RttV); \
        gen_helper_vacsh_val(RxxV, cpu_env, RxxV, RssV, RttV); \
    } while (0)

/*
 * Approximate reciprocal
 * r3,p1 = sfrecipa(r0, r1)
 *
 * The helper packs the 2 32-bit results into a 64-bit value,
 * so unpack them into the proper results.
 */
#define fGEN_TCG_F2_sfrecipa(SHORTCODE) \
    do { \
        TCGv_i64 tmp = tcg_temp_new_i64(); \
        gen_helper_sfrecipa(tmp, cpu_env, RsV, RtV);  \
        tcg_gen_extrh_i64_i32(RdV, tmp); \
        tcg_gen_extrl_i64_i32(PeV, tmp); \
        tcg_temp_free_i64(tmp); \
    } while (0)

/*
 * Approximation of the reciprocal square root
 * r1,p0 = sfinvsqrta(r0)
 *
 * The helper packs the 2 32-bit results into a 64-bit value,
 * so unpack them into the proper results.
 */
#define fGEN_TCG_F2_sfinvsqrta(SHORTCODE) \
    do { \
        TCGv_i64 tmp = tcg_temp_new_i64(); \
        gen_helper_sfinvsqrta(tmp, cpu_env, RsV); \
        tcg_gen_extrh_i64_i32(RdV, tmp); \
        tcg_gen_extrl_i64_i32(PeV, tmp); \
        tcg_temp_free_i64(tmp); \
    } while (0)

/*
 * Add or subtract with carry.
 * Predicate register is used as an extra input and output.
 * r5:4 = add(r1:0, r3:2, p1):carry
 */
#define fGEN_TCG_A4_addp_c(SHORTCODE) \
    do { \
        TCGv_i64 carry = tcg_temp_new_i64(); \
        TCGv_i64 zero = tcg_const_i64(0); \
        tcg_gen_extu_i32_i64(carry, PxV); \
        tcg_gen_andi_i64(carry, carry, 1); \
        tcg_gen_add2_i64(RddV, carry, RssV, zero, carry, zero); \
        tcg_gen_add2_i64(RddV, carry, RddV, carry, RttV, zero); \
        tcg_gen_extrl_i64_i32(PxV, carry); \
        gen_8bitsof(PxV, PxV); \
        tcg_temp_free_i64(carry); \
        tcg_temp_free_i64(zero); \
    } while (0)

/* r5:4 = sub(r1:0, r3:2, p1):carry */
#define fGEN_TCG_A4_subp_c(SHORTCODE) \
    do { \
        TCGv_i64 carry = tcg_temp_new_i64(); \
        TCGv_i64 zero = tcg_const_i64(0); \
        TCGv_i64 not_RttV = tcg_temp_new_i64(); \
        tcg_gen_extu_i32_i64(carry, PxV); \
        tcg_gen_andi_i64(carry, carry, 1); \
        tcg_gen_not_i64(not_RttV, RttV); \
        tcg_gen_add2_i64(RddV, carry, RssV, zero, carry, zero); \
        tcg_gen_add2_i64(RddV, carry, RddV, carry, not_RttV, zero); \
        tcg_gen_extrl_i64_i32(PxV, carry); \
        gen_8bitsof(PxV, PxV); \
        tcg_temp_free_i64(carry); \
        tcg_temp_free_i64(zero); \
        tcg_temp_free_i64(not_RttV); \
    } while (0)

/*
 * Compare each of the 8 unsigned bytes
 * The minimum is placed in each byte of the destination.
 * Each bit of the predicate is set true if the bit from the first operand
 * is greater than the bit from the second operand.
 * r5:4,p1 = vminub(r1:0, r3:2)
 */
#define fGEN_TCG_A6_vminub_RdP(SHORTCODE) \
    do { \
        TCGv left = tcg_temp_new(); \
        TCGv right = tcg_temp_new(); \
        TCGv tmp = tcg_temp_new(); \
        tcg_gen_movi_tl(PeV, 0); \
        tcg_gen_movi_i64(RddV, 0); \
        for (int i = 0; i < 8; i++) { \
            gen_get_byte_i64(left, i, RttV, false); \
            gen_get_byte_i64(right, i, RssV, false); \
            tcg_gen_setcond_tl(TCG_COND_GT, tmp, left, right); \
            tcg_gen_deposit_tl(PeV, PeV, tmp, i, 1); \
            tcg_gen_umin_tl(tmp, left, right); \
            gen_set_byte_i64(i, RddV, tmp); \
        } \
        tcg_temp_free(left); \
        tcg_temp_free(right); \
        tcg_temp_free(tmp); \
    } while (0)

#define fGEN_TCG_J2_call(SHORTCODE) \
    gen_call(pkt, riV)
#define fGEN_TCG_J2_callr(SHORTCODE) \
    gen_callr(pkt, RsV)

#define fGEN_TCG_J2_callt(SHORTCODE) \
    gen_pred_call(pkt, PuV, true, riV)
#define fGEN_TCG_J2_callf(SHORTCODE) \
    gen_pred_call(pkt, PuV, false, riV)
#define fGEN_TCG_J2_callrt(SHORTCODE) \
    gen_pred_callr(pkt, PuV, true, RsV)
#define fGEN_TCG_J2_callrf(SHORTCODE) \
    gen_pred_callr(pkt, PuV, false, RsV)

#define fGEN_TCG_J2_loop0r(SHORTCODE) \
    gen_loop0r(pkt, RsV, riV, insn)
#define fGEN_TCG_J2_loop1r(SHORTCODE) \
    gen_loop1r(pkt, RsV, riV, insn)
#define fGEN_TCG_J2_loop0i(SHORTCODE) \
    gen_loop0i(pkt, UiV, riV, insn)
#define fGEN_TCG_J2_loop1i(SHORTCODE) \
    gen_loop1i(pkt, UiV, riV, insn)

#define fGEN_TCG_J2_endloop0(SHORTCODE) \
    gen_endloop0(pkt)
#define fGEN_TCG_J2_endloop1(SHORTCODE) \
    gen_endloop1(pkt)
#define fGEN_TCG_J2_endloop01(SHORTCODE) \
    gen_endloop01(pkt)

/*
 * Compound compare and jump instructions
 * Here is a primer to understand the tag names
 *
 * Comparison
 *      cmpeqi   compare equal to an immediate
 *      cmpgti   compare greater than an immediate
 *      cmpgtiu  compare greater than an unsigned immediate
 *      cmpeqn1  compare equal to negative 1
 *      cmpgtn1  compare greater than negative 1
 *      cmpeq    compare equal (two registers)
 *
 * Condition
 *      tp0      p0 is true     p0 = cmp.eq(r0,#5); if (p0.new) jump:nt address
 *      fp0      p0 is false    p0 = cmp.eq(r0,#5); if (!p0.new) jump:nt address
 *      tp1      p1 is true     p1 = cmp.eq(r0,#5); if (p1.new) jump:nt address
 *      fp1      p1 is false    p1 = cmp.eq(r0,#5); if (!p1.new) jump:nt address
 *
 * Prediction (not modelled in qemu)
 *      _nt      not taken
 *      _t       taken
 */
#define fGEN_TCG_J4_cmpeqi_tp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqi_fp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqi_tp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqi_fp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqi_tp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqi_fp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqi_tp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqi_fp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_tp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GT, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_fp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GT, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_tp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GT, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_fp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GT, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_tp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GT, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_fp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GT, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_tp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GT, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgti_fp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GT, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_tp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GTU, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_fp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GTU, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_tp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GTU, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_fp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 0, TCG_COND_GTU, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_tp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GTU, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_fp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GTU, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_tp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GTU, true, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpgtui_fp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmpi_jmp(ctx, pkt, insn, 1, TCG_COND_GTU, false, RsV, UiV, riV)
#define fGEN_TCG_J4_cmpeqn1_tp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, true, RsV, riV)
#define fGEN_TCG_J4_cmpeqn1_fp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, false, RsV, riV)
#define fGEN_TCG_J4_cmpeqn1_tp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, true, RsV, riV)
#define fGEN_TCG_J4_cmpeqn1_fp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, false, RsV, riV)
#define fGEN_TCG_J4_cmpeqn1_tp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, true, RsV, riV)
#define fGEN_TCG_J4_cmpeqn1_fp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, false, RsV, riV)
#define fGEN_TCG_J4_cmpeqn1_tp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, true, RsV, riV)
#define fGEN_TCG_J4_cmpeqn1_fp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_EQ, false, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_tp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_GT, true, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_fp0_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_GT, false, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_tp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_GT, true, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_fp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 0, TCG_COND_GT, false, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_tp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_GT, true, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_fp1_jump_nt(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_GT, false, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_tp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_GT, true, RsV, riV)
#define fGEN_TCG_J4_cmpgtn1_fp1_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_n1_jmp(ctx, pkt, insn, 1, TCG_COND_GT, false, RsV, riV)
#define fGEN_TCG_J4_cmpeq_tp0_jump_t(SHORTCODE) \
    gen_cmpnd_cmp_jmp(ctx, pkt, insn, 0, TCG_COND_EQ, true, RsV, RtV, riV)

/* p0 = cmp.eq(r0, #7) */
#define fGEN_TCG_SA1_cmpeqi(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        gen_comparei(TCG_COND_EQ, tmp, RsV, uiV); \
        gen_log_pred_write(ctx, 0, tmp); \
        tcg_temp_free(tmp); \
    } while (0)

/* r0 = add(r29,#8) */
#define fGEN_TCG_SA1_addsp(SHORTCODE) \
    tcg_gen_addi_tl(RdV, hex_gpr[HEX_REG_SP], uiV)

/* r0 = add(r0, r1) */
#define fGEN_TCG_SA1_addrx(SHORTCODE) \
    tcg_gen_add_tl(RxV, RxV, RsV)

#define fGEN_TCG_A2_add(SHORTCODE) \
    tcg_gen_add_tl(RdV, RsV, RtV)

#define fGEN_TCG_A2_sub(SHORTCODE) \
    tcg_gen_sub_tl(RdV, RtV, RsV)

#define fGEN_TCG_A2_addp(SHORTCODE) \
    tcg_gen_add_i64(RddV, RssV, RttV)

/* r0 = sub(#10, r1) */
#define fGEN_TCG_A2_subri(SHORTCODE) \
    tcg_gen_subfi_tl(RdV, siV, RsV)

#define fGEN_TCG_A2_addi(SHORTCODE) \
    tcg_gen_addi_tl(RdV, RsV, siV)

#define fGEN_TCG_SA1_addi(SHORTCODE) \
    tcg_gen_addi_tl(RxV, RxV, siV)

#define fGEN_TCG_A2_and(SHORTCODE) \
    tcg_gen_and_tl(RdV, RsV, RtV)

#define fGEN_TCG_A2_andir(SHORTCODE) \
    tcg_gen_andi_tl(RdV, RsV, siV)

#define fGEN_TCG_A2_xor(SHORTCODE) \
    tcg_gen_xor_tl(RdV, RsV, RtV)

#define fGEN_TCG_A2_maxu(SHORTCODE) \
    tcg_gen_umax_tl(RdV, RsV, RtV)

/* Transfer instructions */
#define fGEN_TCG_A2_tfr(SHORTCODE) \
    tcg_gen_mov_tl(RdV, RsV)
#define fGEN_TCG_SA1_tfr(SHORTCODE) \
    tcg_gen_mov_tl(RdV, RsV)
#define fGEN_TCG_A2_tfrsi(SHORTCODE) \
    tcg_gen_movi_tl(RdV, siV)
#define fGEN_TCG_A2_tfrcrr(SHORTCODE) \
    tcg_gen_mov_tl(RdV, CsV)
#define fGEN_TCG_A2_tfrrcr(SHORTCODE) \
    tcg_gen_mov_tl(CdV, RsV)

#define fGEN_TCG_A2_nop(SHORTCODE) \
    do { } while (0)

/* Compare instructions */
#define fGEN_TCG_C2_cmpeq(SHORTCODE) \
    gen_compare(TCG_COND_EQ, PdV, RsV, RtV)
#define fGEN_TCG_C4_cmpneq(SHORTCODE) \
    gen_compare(TCG_COND_NE, PdV, RsV, RtV)
#define fGEN_TCG_C2_cmpgt(SHORTCODE) \
    gen_compare(TCG_COND_GT, PdV, RsV, RtV)
#define fGEN_TCG_C2_cmpgtu(SHORTCODE) \
    gen_compare(TCG_COND_GTU, PdV, RsV, RtV)
#define fGEN_TCG_C4_cmplte(SHORTCODE) \
    gen_compare(TCG_COND_LE, PdV, RsV, RtV)
#define fGEN_TCG_C4_cmplteu(SHORTCODE) \
    gen_compare(TCG_COND_LEU, PdV, RsV, RtV)
#define fGEN_TCG_C2_cmpeqp(SHORTCODE) \
    gen_compare_i64(TCG_COND_EQ, PdV, RssV, RttV)
#define fGEN_TCG_C2_cmpgtp(SHORTCODE) \
    gen_compare_i64(TCG_COND_GT, PdV, RssV, RttV)
#define fGEN_TCG_C2_cmpgtup(SHORTCODE) \
    gen_compare_i64(TCG_COND_GTU, PdV, RssV, RttV)
#define fGEN_TCG_C2_cmpeqi(SHORTCODE) \
    gen_comparei(TCG_COND_EQ, PdV, RsV, siV)
#define fGEN_TCG_C2_cmpgti(SHORTCODE) \
    gen_comparei(TCG_COND_GT, PdV, RsV, siV)
#define fGEN_TCG_C2_cmpgtui(SHORTCODE) \
    gen_comparei(TCG_COND_GTU, PdV, RsV, uiV)
#define fGEN_TCG_A4_cmpbgtui(SHORTCODE) \
    do { \
        TCGv byte = tcg_temp_new(); \
        tcg_gen_extract_tl(byte, RsV, 0, 8); \
        gen_comparei(TCG_COND_GTU, PdV, byte, uiV); \
        tcg_temp_free(byte); \
    } while (0)

#define fGEN_TCG_SA1_zxtb(SHORTCODE) \
    tcg_gen_ext8u_tl(RdV, RsV)

#define fGEN_TCG_J2_jump(SHORTCODE) \
    gen_jump(pkt, riV)
#define fGEN_TCG_J2_jumpr(SHORTCODE) \
    gen_write_new_pc(pkt, RsV)

#define fGEN_TCG_cond_jump(COND) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        COND; \
        gen_cond_jump(pkt, LSB, riV); \
        tcg_temp_free(LSB); \
    } while (0)

#define fGEN_TCG_J2_jumpt(SHORTCODE) \
    fGEN_TCG_cond_jump(fLSBOLD(PuV))
#define fGEN_TCG_J2_jumpf(SHORTCODE) \
    fGEN_TCG_cond_jump(fLSBOLDNOT(PuV))
#define fGEN_TCG_J2_jumpfnewpt(SHORTCODE) \
    fGEN_TCG_cond_jump(fLSBNEWNOT(PuN))
#define fGEN_TCG_J2_jumpfnew(SHORTCODE) \
    fGEN_TCG_cond_jump(fLSBNEWNOT(PuN))

#define fGEN_TCG_J2_jumprfnew(SHORTCODE) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        tcg_gen_andi_tl(LSB, PuN, 1); \
        tcg_gen_xori_tl(LSB, LSB, 1); \
        gen_cond_jumpr(pkt, LSB, RsV); \
        tcg_temp_free(LSB); \
    } while (0)

#define fGEN_TCG_J2_jumptnew(SHORTCODE) \
    gen_cond_jump(pkt, PuN, riV)
#define fGEN_TCG_J2_jumptnewpt(SHORTCODE) \
    gen_cond_jump(pkt, PuN, riV)

/*
 * New value compare & jump instructions
 * if ([!]COND(r0.new, r1) jump:t address
 * if ([!]COND(r0.new, #7) jump:t address
 */
#define fGEN_TCG_J4_cmpgt_f_jumpnv_t(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_LE, NsN, RtV, riV)
#define fGEN_TCG_J4_cmpeq_f_jumpnv_nt(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_NE, NsN, RtV, riV)
#define fGEN_TCG_J4_cmpeq_t_jumpnv_nt(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_EQ, NsN, RtV, riV)
#define fGEN_TCG_J4_cmplt_f_jumpnv_nt(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_GE, NsN, RtV, riV)
#define fGEN_TCG_J4_cmpgt_t_jumpnv_t(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_GT, NsN, RtV, riV)
#define fGEN_TCG_J4_cmpeqi_t_jumpnv_nt(SHORTCODE) \
    gen_cmpi_jumpnv(pkt, TCG_COND_EQ, NsN, UiV, riV)
#define fGEN_TCG_J4_cmpltu_f_jumpnv_t(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_GEU, NsN, RtV, riV)
#define fGEN_TCG_J4_cmpgtui_t_jumpnv_t(SHORTCODE) \
    gen_cmpi_jumpnv(pkt, TCG_COND_GTU, NsN, UiV, riV)
#define fGEN_TCG_J4_cmpeq_f_jumpnv_t(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_NE, NsN, RtV, riV)
#define fGEN_TCG_J4_cmpeqi_f_jumpnv_t(SHORTCODE) \
    gen_cmpi_jumpnv(pkt, TCG_COND_NE, NsN, UiV, riV)
#define fGEN_TCG_J4_cmpgtu_t_jumpnv_t(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_GTU, NsN, RtV, riV)
#define fGEN_TCG_J4_cmpgtu_f_jumpnv_t(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_LEU, NsN, RtV, riV)
#define fGEN_TCG_J4_cmplt_t_jumpnv_t(SHORTCODE) \
    gen_cmp_jumpnv(pkt, TCG_COND_LT, NsN, RtV, riV)

/* r0 = r1 ; jump address */
#define fGEN_TCG_J4_jumpsetr(SHORTCODE) \
    do { \
        tcg_gen_mov_tl(RdV, RsV); \
        gen_jump(pkt, riV); \
    } while (0)

/* r0 = lsr(r1, #5) */
#define fGEN_TCG_S2_lsr_i_r(SHORTCODE) \
    fLSHIFTR(RdV, RsV, uiV, 4_4)

/* r0 += lsr(r1, #5) */
#define fGEN_TCG_S2_lsr_i_r_acc(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        fLSHIFTR(tmp, RsV, uiV, 4_4); \
        tcg_gen_add_tl(RxV, RxV, tmp); \
        tcg_temp_free(tmp); \
    } while (0)

/* r0 ^= lsr(r1, #5) */
#define fGEN_TCG_S2_lsr_i_r_xacc(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        fLSHIFTR(tmp, RsV, uiV, 4_4); \
        tcg_gen_xor_tl(RxV, RxV, tmp); \
        tcg_temp_free(tmp); \
    } while (0)

/* r0 = asr(r1, #5) */
#define fGEN_TCG_S2_asr_i_r(SHORTCODE) \
    fASHIFTR(RdV, RsV, uiV, 4_4)

/* r0 = addasl(r1, r2, #3) */
#define fGEN_TCG_S2_addasl_rrri(SHORTCODE) \
    do { \
        fASHIFTL(RdV, RsV, uiV, 4_4); \
        tcg_gen_add_tl(RdV, RtV, RdV); \
    } while (0)

/* r0 |= asl(r1, r2) */
#define fGEN_TCG_S2_asl_r_r_or(SHORTCODE) \
    gen_asl_r_r_or(RxV, RsV, RtV)

#define fGEN_TCG_S2_asr_r_r_sat(SHORTCODE) \
    gen_asr_r_r_sat(RdV, RsV, RtV)

#define fGEN_TCG_S2_asl_r_r_sat(SHORTCODE) \
    gen_asl_r_r_sat(RdV, RsV, RtV)

/* r0 = asl(r1, #5) */
#define fGEN_TCG_S2_asl_i_r(SHORTCODE) \
    fASHIFTL(RdV, RsV, uiV, 4_4)

/* r0 |= asl(r1, #5) */
#define fGEN_TCG_S2_asl_i_r_or(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        fASHIFTL(tmp, RsV, uiV, 4_4); \
        tcg_gen_or_tl(RxV, RxV, tmp); \
        tcg_temp_free(tmp); \
    } while (0)

/* r0 = vsplatb(r1) */
#define fGEN_TCG_S2_vsplatrb(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        int i; \
        tcg_gen_movi_tl(RdV, 0); \
        tcg_gen_andi_tl(tmp, RsV, 0xff); \
        for (i = 0; i < 4; i++) { \
            tcg_gen_shli_tl(RdV, RdV, 8); \
            tcg_gen_or_tl(RdV, RdV, tmp); \
        } \
        tcg_temp_free(tmp); \
    } while (0)

#define fGEN_TCG_SA1_seti(SHORTCODE) \
    tcg_gen_movi_tl(RdV, uiV)

#define fGEN_TCG_S2_insert(SHORTCODE) \
    do { \
        int width = uiV; \
        int offset = UiV; \
        if (width != 0) { \
            if (offset + width > 32) { \
                width = 32 - offset; \
            } \
            tcg_gen_deposit_i32(RxV, RxV, RsV, offset, width); \
        } \
    } while (0)

#define fGEN_TCG_S2_extractu(SHORTCODE) \
    do { \
        unsigned int ofs = UiV; \
        unsigned int len = uiV; \
        if (len == 0) { \
            tcg_gen_movi_tl(RdV, 0); \
        } else { \
            if (ofs + len > 32) { \
                len = 32 - ofs; \
            } \
            tcg_gen_extract_i32(RdV, RsV, ofs, len); \
        } \
    } while (0)
#define fGEN_TCG_S2_extractup(SHORTCODE) \
    do { \
        unsigned int ofs = UiV; \
        unsigned int len = uiV; \
        if (len == 0) { \
            tcg_gen_movi_i64(RddV, 0); \
        } else { \
            if (ofs + len > 64) { \
                len = 64 - ofs; \
            } \
            tcg_gen_extract_i64(RddV, RssV, ofs, len); \
        } \
    } while (0)

#define fGEN_TCG_A2_combinew(SHORTCODE) \
    tcg_gen_concat_i32_i64(RddV, RtV, RsV)
#define fGEN_TCG_A2_combineii(SHORTCODE) \
    do { \
        TCGv tmp_lo = tcg_const_tl(SiV); \
        TCGv tmp_hi = tcg_const_tl(siV); \
        tcg_gen_concat_i32_i64(RddV, tmp_lo, tmp_hi); \
        tcg_temp_free(tmp_lo); \
        tcg_temp_free(tmp_hi); \
    } while (0)
#define fGEN_TCG_A2_combine_ll(SHORTCODE) \
    do { \
        tcg_gen_mov_tl(RdV, RsV); \
        tcg_gen_deposit_tl(RdV, RdV, RtV, 16, 16); \
    } while (0)
#define fGEN_TCG_A2_combine_lh(SHORTCODE) \
    do { \
        gen_get_half(RdV, 1, RsV, false); \
        tcg_gen_deposit_tl(RdV, RdV, RtV, 16, 16); \
    } while (0)
#define fGEN_TCG_A4_combineri(SHORTCODE) \
    do { \
        TCGv tmp_lo = tcg_const_tl(siV); \
        tcg_gen_concat_i32_i64(RddV, tmp_lo, RsV); \
        tcg_temp_free(tmp_lo); \
    } while (0)
#define fGEN_TCG_A4_combineir(SHORTCODE) \
    do { \
        TCGv tmp_hi = tcg_const_tl(siV); \
        tcg_gen_concat_i32_i64(RddV, RsV, tmp_hi); \
        tcg_temp_free(tmp_hi); \
    } while (0)
#define fGEN_TCG_A4_combineii(SHORTCODE) \
    do { \
        TCGv tmp_lo = tcg_const_tl(UiV); \
        TCGv tmp_hi = tcg_const_tl(siV); \
        tcg_gen_concat_i32_i64(RddV, tmp_lo, tmp_hi); \
        tcg_temp_free(tmp_lo); \
        tcg_temp_free(tmp_hi); \
    } while (0)

#define fGEN_TCG_SA1_combine0i(SHORTCODE) \
    do { \
        TCGv tmp_lo = tcg_const_tl(uiV); \
        TCGv zero = tcg_const_tl(0); \
        tcg_gen_concat_i32_i64(RddV, tmp_lo, zero); \
        tcg_temp_free(tmp_lo); \
        tcg_temp_free(zero); \
    } while (0)

/* r0 = or(#8, asl(r0, #5)) */
#define fGEN_TCG_S4_ori_asl_ri(SHORTCODE) \
    do { \
        tcg_gen_shli_tl(RxV, RxV, UiV); \
        tcg_gen_ori_tl(RxV, RxV, uiV); \
    } while (0)

/* r0 = add(r1, sub(#6, r2)) */
#define fGEN_TCG_S4_subaddi(SHORTCODE) \
    do { \
        tcg_gen_sub_tl(RdV, RsV, RuV); \
        tcg_gen_addi_tl(RdV, RdV, siV); \
    } while (0)

#define fGEN_TCG_SA1_inc(SHORTCODE) \
    tcg_gen_addi_tl(RdV, RsV, 1)

#define fGEN_TCG_SA1_dec(SHORTCODE) \
    tcg_gen_subi_tl(RdV, RsV, 1)

/* if (p0.new) r0 = #0 */
#define fGEN_TCG_SA1_clrtnew(SHORTCODE) \
    do { \
        TCGLabel *skip = gen_new_label(); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, hex_new_pred_value[0], 0, skip); \
        tcg_gen_movi_tl(RdV, 0); \
        gen_set_label(skip); \
    } while (0)

/* r0 = add(r1 , mpyi(#6, r2)) */
#define fGEN_TCG_M4_mpyri_addr_u2(SHORTCODE) \
    do { \
        tcg_gen_muli_tl(RdV, RsV, uiV); \
        tcg_gen_add_tl(RdV, RuV, RdV); \
    } while (0)

/* Predicated add instructions */
#define GEN_TCG_padd(PRED, ADD) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGLabel *skip = gen_new_label(); \
        PRED; \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, skip); \
        tcg_temp_free(LSB); \
        ADD; \
        gen_set_label(skip); \
    } while (0)

#define fGEN_TCG_A2_paddt(SHORTCODE) \
    GEN_TCG_padd(fLSBOLD(PuV), tcg_gen_add_tl(RdV, RsV, RtV))
#define fGEN_TCG_A2_paddf(SHORTCODE) \
    GEN_TCG_padd(fLSBOLDNOT(PuV), tcg_gen_add_tl(RdV, RsV, RtV))
#define fGEN_TCG_A2_paddit(SHORTCODE) \
    GEN_TCG_padd(fLSBOLD(PuV), tcg_gen_addi_tl(RdV, RsV, siV))
#define fGEN_TCG_A2_paddif(SHORTCODE) \
    GEN_TCG_padd(fLSBOLDNOT(PuV), tcg_gen_addi_tl(RdV, RsV, siV))
#define fGEN_TCG_A2_paddtnew(SHORTCODE) \
    GEN_TCG_padd(fLSBNEW(PuN), tcg_gen_add_tl(RdV, RsV, RtV))
#define fGEN_TCG_A2_paddfnew(SHORTCODE) \
    GEN_TCG_padd(fLSBNEWNOT(PuN), tcg_gen_add_tl(RdV, RsV, RtV))
#define fGEN_TCG_A2_padditnew(SHORTCODE) \
    GEN_TCG_padd(fLSBNEW(PuN), tcg_gen_addi_tl(RdV, RsV, siV))
#define fGEN_TCG_A2_psubtnew(SHORTCODE) \
    GEN_TCG_padd(fLSBNEW(PuN), tcg_gen_sub_tl(RdV, RtV, RsV))
#define fGEN_TCG_A2_pxorf(SHORTCODE) \
    GEN_TCG_padd(fLSBOLDNOT(PuV), tcg_gen_xor_tl(RdV, RsV, RtV))

/* Conditional move instructions */
#define fGEN_TCG_COND_MOVE(VAL) \
    do { \
        TCGv LSB = tcg_temp_new(); \
        TCGLabel *skip = gen_new_label(); \
        VAL; \
        tcg_gen_brcondi_tl(TCG_COND_EQ, LSB, 0, skip); \
        tcg_gen_movi_tl(RdV, siV); \
        gen_set_label(skip); \
    } while (0)

#define fGEN_TCG_C2_cmoveit(SHORTCODE) \
    fGEN_TCG_COND_MOVE(fLSBOLD(PuV))
#define fGEN_TCG_C2_cmovenewit(SHORTCODE) \
    fGEN_TCG_COND_MOVE(fLSBNEW(PuN))
#define fGEN_TCG_C2_cmovenewif(SHORTCODE) \
    fGEN_TCG_COND_MOVE(fLSBNEWNOT(PuN))

/* r0 = mux(p0, #3, #5) */
#define fGEN_TCG_C2_muxii(SHORTCODE) \
    do { \
        TCGv zero = tcg_const_tl(0); \
        TCGv tval = tcg_const_tl(siV); \
        TCGv fval = tcg_const_tl(SiV); \
        TCGv lsb = tcg_temp_new(); \
        tcg_gen_andi_tl(lsb, PuV, 1); \
        tcg_gen_movcond_tl(TCG_COND_NE, RdV, lsb, zero, tval, fval); \
        tcg_temp_free(zero); \
        tcg_temp_free(tval); \
        tcg_temp_free(fval); \
        tcg_temp_free(lsb); \
    } while (0)

/* r0 = mux(p0, r1, #5) */
#define fGEN_TCG_C2_muxir(SHORTCODE) \
    do { \
        TCGv zero = tcg_const_tl(0); \
        TCGv fval = tcg_const_tl(siV); \
        TCGv lsb = tcg_temp_new(); \
        tcg_gen_andi_tl(lsb, PuV, 1); \
        tcg_gen_movcond_tl(TCG_COND_NE, RdV, lsb, zero, RsV, fval); \
        tcg_temp_free(zero); \
        tcg_temp_free(fval); \
        tcg_temp_free(lsb); \
    } while (0)

/* r0 = mux(p0, #3, r1) */
#define fGEN_TCG_C2_muxri(SHORTCODE) \
    do { \
        TCGv zero = tcg_const_tl(0); \
        TCGv tval = tcg_const_tl(siV); \
        TCGv lsb = tcg_temp_new(); \
        tcg_gen_andi_tl(lsb, PuV, 1); \
        tcg_gen_movcond_tl(TCG_COND_NE, RdV, lsb, zero, tval, RsV); \
        tcg_temp_free(zero); \
        tcg_temp_free(tval); \
        tcg_temp_free(lsb); \
    } while (0)

/* p0 = tstbit(r0, #5) */
#define fGEN_TCG_S2_tstbit_i(SHORTCODE) \
    do { \
        tcg_gen_andi_tl(PdV, RsV, (1 << uiV)); \
        gen_8bitsof(PdV, PdV); \
    } while (0)

/* p0 = !tstbit(r0, #5) */
#define fGEN_TCG_S4_ntstbit_i(SHORTCODE) \
    do { \
        tcg_gen_andi_tl(PdV, RsV, (1 << uiV)); \
        gen_8bitsof(PdV, PdV); \
        tcg_gen_xori_tl(PdV, PdV, 0xff); \
    } while (0)

/* r0 = setbit(r1, #5) */
#define fGEN_TCG_S2_setbit_i(SHORTCODE) \
    tcg_gen_ori_tl(RdV, RsV, 1 << uiV)

/* r0 = togglebit(r1, #5) */
#define fGEN_TCG_S2_togglebit_i(SHORTCODE) \
    tcg_gen_xori_tl(RdV, RsV, 1 << uiV)

/* r0 += add(r1, #8) */
#define fGEN_TCG_M2_accii(SHORTCODE) \
    do { \
        tcg_gen_add_tl(RxV, RxV, RsV); \
        tcg_gen_addi_tl(RxV, RxV, siV); \
    } while (0)

/* p0 = bitsclr(r1, #6) */
#define fGEN_TCG_C2_bitsclri(SHORTCODE) \
    do { \
        TCGv zero = tcg_const_tl(0); \
        tcg_gen_andi_tl(PdV, RsV, uiV); \
        gen_compare(TCG_COND_EQ, PdV, PdV, zero); \
        tcg_temp_free(zero); \
    } while (0)

#define fGEN_TCG_S2_cl0(SHORTCODE) \
    tcg_gen_clzi_tl(RdV, RsV, 32)

#define fGEN_TCG_A2_abs(SHORTCODE) \
    tcg_gen_abs_tl(RdV, RsV)

/* r3:2 = bitsplit(r1, #5) */
#define fGEN_TCG_A4_bitspliti(SHORTCODE) \
    do { \
        int bits = uiV; \
        if (bits == 0) { \
            tcg_gen_extu_i32_i64(RddV, RsV); \
            tcg_gen_shli_i64(RddV, RddV, 32); \
        } else { \
            TCGv lo = tcg_temp_new(); \
            TCGv hi = tcg_temp_new(); \
            tcg_gen_extract_tl(lo, RsV, 0, bits); \
            tcg_gen_extract_tl(hi, RsV, bits, 32 - bits); \
            tcg_gen_concat_i32_i64(RddV, lo, hi); \
            tcg_temp_free(lo); \
            tcg_temp_free(hi); \
        } \
    } while (0)

#define fGEN_TCG_SL2_jumpr31(SHORTCODE) \
    gen_write_new_pc(pkt, hex_gpr[HEX_REG_LR])

#define fGEN_TCG_SL2_jumpr31_tnew(SHORTCODE) \
    gen_cond_jumpr(pkt, hex_new_pred_value[0], hex_gpr[HEX_REG_LR])

/*
 * r5:4 = valignb(r1:0, r3:2, p0)
 *
 * Predicate register contains the shift amount in bytes
 * Shift RssV right
 * Shift RttV left
 * Combine the parts into RddV
 *
 * Note the special handling when the shift amount is zero
 *     Just copy RssV into Rddv
 */

#define fGEN_TCG_S2_valignrb(SHORTCODE) \
    do { \
        TCGv shift = tcg_temp_local_new(); \
        TCGv_i64 shift_i64 = tcg_temp_new_i64(); \
        TCGv_i64 tmp = tcg_temp_new_i64(); \
        TCGLabel *skip = gen_new_label(); \
        tcg_gen_andi_tl(shift, PuV, 0x7); \
        tcg_gen_mov_i64(RddV, RssV); \
        tcg_gen_brcondi_tl(TCG_COND_EQ, shift, 0, skip); \
        tcg_gen_muli_i32(shift, shift, 8); \
        tcg_gen_extu_i32_i64(shift_i64, shift); \
        tcg_gen_shr_i64(RddV, RssV, shift_i64); \
        tcg_gen_subfi_i64(shift_i64, 64, shift_i64); \
        tcg_gen_shl_i64(tmp, RttV, shift_i64); \
        tcg_gen_or_i64(RddV, RddV, tmp); \
        gen_set_label(skip); \
        tcg_temp_free(shift); \
        tcg_temp_free_i64(shift_i64); \
        tcg_temp_free_i64(tmp); \
    } while (0)

/* Not modelled in qemu, but need to suppress compiler warnings */
#define fGEN_TCG_Y2_dcfetchbo(SHORTCODE) \
    do { \
        uiV = uiV; \
        RsV = RsV; \
    } while (0)

/*
 * Add vector of words
 * r5:4 = vaddw(r1:0, r3:2, p0)
 */
#define fGEN_TCG_A2_vaddw(SHORTCODE) \
    do { \
        TCGv_i64 left = tcg_temp_new_i64(); \
        TCGv_i64 right = tcg_temp_new_i64(); \
        tcg_gen_movi_i64(RddV, 0); \
        for (int i = 0; i < 2; i++) { \
            tcg_gen_sextract_i64(left, RssV, i * 32, 32); \
            tcg_gen_sextract_i64(right, RttV, i * 32, 32); \
            tcg_gen_add_i64(left, left, right); \
            tcg_gen_deposit_i64(RddV, RddV, left, i * 32, 32); \
        } \
        tcg_temp_free_i64(left); \
        tcg_temp_free_i64(right); \
    } while (0)

/* p2 = and(p0, !p1) */
#define fGEN_TCG_C2_andn(SHORTCODE) \
    tcg_gen_andc_tl(PdV, PtV, PsV)

/*  r0 = mpyi(r1, r2) */
#define fGEN_TCG_M2_mpyi(SHORTCODE) \
    tcg_gen_mul_tl(RdV, RsV, RtV)

/*  r0 += mpyi(r1, r2) */
#define fGEN_TCG_M2_maci(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        tcg_gen_mul_tl(tmp, RsV, RtV); \
        tcg_gen_add_tl(RxV, RxV, tmp); \
        tcg_temp_free(tmp); \
    } while (0)

/* r0 += add(r1, r2) */
#define fGEN_TCG_M2_acci(SHORTCODE) \
    do { \
        tcg_gen_add_tl(RxV, RxV, RsV); \
        tcg_gen_add_tl(RxV, RxV, RtV); \
    } while (0)

#define fGEN_TCG_A2_sath(SHORTCODE) \
    gen_sat(RdV, RsV, true, 16)
#define fGEN_TCG_A2_satuh(SHORTCODE) \
    gen_sat(RdV, RsV, false, 16)
#define fGEN_TCG_A2_satb(SHORTCODE) \
    gen_sat(RdV, RsV, true, 8)
#define fGEN_TCG_A2_satub(SHORTCODE) \
    gen_sat(RdV, RsV, false, 8)

/* Floating point */
#define fGEN_TCG_F2_conv_sf2df(SHORTCODE) \
    gen_helper_conv_sf2df(RddV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_df2sf(SHORTCODE) \
    gen_helper_conv_df2sf(RdV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_uw2sf(SHORTCODE) \
    gen_helper_conv_uw2sf(RdV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_uw2df(SHORTCODE) \
    gen_helper_conv_uw2df(RddV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_w2sf(SHORTCODE) \
    gen_helper_conv_w2sf(RdV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_w2df(SHORTCODE) \
    gen_helper_conv_w2df(RddV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_ud2sf(SHORTCODE) \
    gen_helper_conv_ud2sf(RdV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_ud2df(SHORTCODE) \
    gen_helper_conv_ud2df(RddV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_d2sf(SHORTCODE) \
    gen_helper_conv_d2sf(RdV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_d2df(SHORTCODE) \
    gen_helper_conv_d2df(RddV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_sf2uw(SHORTCODE) \
    gen_helper_conv_sf2uw(RdV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_sf2w(SHORTCODE) \
    gen_helper_conv_sf2w(RdV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_sf2ud(SHORTCODE) \
    gen_helper_conv_sf2ud(RddV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_sf2d(SHORTCODE) \
    gen_helper_conv_sf2d(RddV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_df2uw(SHORTCODE) \
    gen_helper_conv_df2uw(RdV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_df2w(SHORTCODE) \
    gen_helper_conv_df2w(RdV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_df2ud(SHORTCODE) \
    gen_helper_conv_df2ud(RddV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_df2d(SHORTCODE) \
    gen_helper_conv_df2d(RddV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_sf2uw_chop(SHORTCODE) \
    gen_helper_conv_sf2uw_chop(RdV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_sf2w_chop(SHORTCODE) \
    gen_helper_conv_sf2w_chop(RdV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_sf2ud_chop(SHORTCODE) \
    gen_helper_conv_sf2ud_chop(RddV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_sf2d_chop(SHORTCODE) \
    gen_helper_conv_sf2d_chop(RddV, cpu_env, RsV)
#define fGEN_TCG_F2_conv_df2uw_chop(SHORTCODE) \
    gen_helper_conv_df2uw_chop(RdV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_df2w_chop(SHORTCODE) \
    gen_helper_conv_df2w_chop(RdV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_df2ud_chop(SHORTCODE) \
    gen_helper_conv_df2ud_chop(RddV, cpu_env, RssV)
#define fGEN_TCG_F2_conv_df2d_chop(SHORTCODE) \
    gen_helper_conv_df2d_chop(RddV, cpu_env, RssV)
#define fGEN_TCG_F2_sfadd(SHORTCODE) \
    gen_helper_sfadd(RdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfsub(SHORTCODE) \
    gen_helper_sfsub(RdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfcmpeq(SHORTCODE) \
    gen_helper_sfcmpeq(PdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfcmpgt(SHORTCODE) \
    gen_helper_sfcmpgt(PdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfcmpge(SHORTCODE) \
    gen_helper_sfcmpge(PdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfcmpuo(SHORTCODE) \
    gen_helper_sfcmpuo(PdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfmax(SHORTCODE) \
    gen_helper_sfmax(RdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfmin(SHORTCODE) \
    gen_helper_sfmin(RdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sfclass(SHORTCODE) \
    do { \
        TCGv imm = tcg_const_tl(uiV); \
        gen_helper_sfclass(PdV, cpu_env, RsV, imm); \
        tcg_temp_free(imm); \
    } while (0)
#define fGEN_TCG_F2_sffixupn(SHORTCODE) \
    gen_helper_sffixupn(RdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sffixupd(SHORTCODE) \
    gen_helper_sffixupd(RdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sffixupr(SHORTCODE) \
    gen_helper_sffixupr(RdV, cpu_env, RsV)
#define fGEN_TCG_F2_dfadd(SHORTCODE) \
    gen_helper_dfadd(RddV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfsub(SHORTCODE) \
    gen_helper_dfsub(RddV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfmax(SHORTCODE) \
    gen_helper_dfmax(RddV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfmin(SHORTCODE) \
    gen_helper_dfmin(RddV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfcmpeq(SHORTCODE) \
    gen_helper_dfcmpeq(PdV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfcmpgt(SHORTCODE) \
    gen_helper_dfcmpgt(PdV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfcmpge(SHORTCODE) \
    gen_helper_dfcmpge(PdV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfcmpuo(SHORTCODE) \
    gen_helper_dfcmpuo(PdV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfclass(SHORTCODE) \
    do { \
        TCGv imm = tcg_const_tl(uiV); \
        gen_helper_dfclass(PdV, cpu_env, RssV, imm); \
        tcg_temp_free(imm); \
    } while (0)
#define fGEN_TCG_F2_sfmpy(SHORTCODE) \
    gen_helper_sfmpy(RdV, cpu_env, RsV, RtV)
#define fGEN_TCG_F2_sffma(SHORTCODE) \
    gen_helper_sffma(RxV, cpu_env, RxV, RsV, RtV)
#define fGEN_TCG_F2_sffma_sc(SHORTCODE) \
    gen_helper_sffma_sc(RxV, cpu_env, RxV, RsV, RtV, PuV)
#define fGEN_TCG_F2_sffms(SHORTCODE) \
    gen_helper_sffms(RxV, cpu_env, RxV, RsV, RtV)
#define fGEN_TCG_F2_sffma_lib(SHORTCODE) \
    gen_helper_sffma_lib(RxV, cpu_env, RxV, RsV, RtV)
#define fGEN_TCG_F2_sffms_lib(SHORTCODE) \
    gen_helper_sffms_lib(RxV, cpu_env, RxV, RsV, RtV)

#define fGEN_TCG_F2_dfmpyfix(SHORTCODE) \
    gen_helper_dfmpyfix(RddV, cpu_env, RssV, RttV)
#define fGEN_TCG_F2_dfmpyhh(SHORTCODE) \
    gen_helper_dfmpyhh(RxxV, cpu_env, RxxV, RssV, RttV)

/* Nothing to do for these in qemu, need to suppress compiler warnings */
#define fGEN_TCG_Y4_l2fetch(SHORTCODE) \
    do { \
        RsV = RsV; \
        RtV = RtV; \
    } while (0)
#define fGEN_TCG_Y5_l2fetch(SHORTCODE) \
    do { \
        RsV = RsV; \
    } while (0)

#define fGEN_TCG_J2_trap0(SHORTCODE) \
    do { \
        uiV = uiV; \
        tcg_gen_movi_tl(hex_gpr[HEX_REG_PC], pkt->pc); \
        TCGv excp = tcg_const_tl(HEX_EXCP_TRAP0); \
        gen_helper_raise_exception(cpu_env, excp); \
        tcg_temp_free(excp); \
    } while (0)
#endif
