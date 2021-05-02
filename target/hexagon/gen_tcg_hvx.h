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
#define fGEN_TCG_V6_vasrw(SHORTCODE) \
    do { \
        TCGv shift = tcg_temp_new(); \
        tcg_gen_andi_tl(shift, RtV, 31); \
        tcg_gen_gvec_sars(MO_32, VdV_off, VuV_off, shift, \
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
#define fGEN_TCG_V6_vL32b_cur_pi(SHORTCODE)            SHORTCODE
#define fGEN_TCG_V6_vL32b_nt_cur_pi(SHORTCODE)         SHORTCODE
#define fGEN_TCG_V6_vL32b_cur_pi(SHORTCODE)            SHORTCODE
#define fGEN_TCG_V6_vL32b_tmp_pi(SHORTCODE)            SHORTCODE
#define fGEN_TCG_V6_vL32b_pi(SHORTCODE)                SHORTCODE

#define fGEN_TCG_V6_vL32b_cur_ai(SHORTCODE)            SHORTCODE
#define fGEN_TCG_V6_vL32b_tmp_ai(SHORTCODE)            SHORTCODE
#define fGEN_TCG_V6_vL32b_ai(SHORTCODE)                SHORTCODE

#define fGEN_TCG_V6_vL32b_cur_ppu(SHORTCODE)           SHORTCODE
#define fGEN_TCG_V6_vL32b_tmp_ppu(SHORTCODE)           SHORTCODE
#define fGEN_TCG_V6_vL32b_ppu(SHORTCODE)               SHORTCODE

#endif
