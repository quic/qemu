/*
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_GEN_TCG_SYS_H
#define HEXAGON_GEN_TCG_SYS_H

/* System mode instructions */
#define fGEN_TCG_Y2_swi(SHORTCODE) \
    gen_helper_swi(cpu_env, RsV)

#define fGEN_TCG_Y2_cswi(SHORTCODE) \
    gen_helper_cswi(cpu_env, RsV)

#define fGEN_TCG_Y2_ciad(SHORTCODE) \
    gen_helper_ciad(cpu_env, RsV)

#define fGEN_TCG_Y4_siad(SHORTCODE) \
    gen_helper_siad(cpu_env, RsV)

#define fGEN_TCG_Y2_wait(SHORTCODE) \
    do { \
        RsV = RsV; \
        gen_helper_wait(cpu_env, tcg_const_tl(ctx->pkt->pc)); \
    } while (0)

#define fGEN_TCG_Y2_resume(SHORTCODE) \
    gen_helper_resume(cpu_env, RsV)

#define fGEN_TCG_Y2_getimask(SHORTCODE) \
    gen_helper_getimask(RdV, cpu_env, RsV)

#define fGEN_TCG_Y2_iassignw(SHORTCODE) \
    gen_helper_iassignw(cpu_env, RsV)

#define fGEN_TCG_Y2_iassignr(SHORTCODE) \
    gen_helper_iassignr(RdV, cpu_env, RsV)

#define fGEN_TCG_Y2_setimask(SHORTCODE) \
    gen_helper_setimask(cpu_env, PtV, RsV)

#define fGEN_TCG_Y4_nmi(SHORTCODE) \
    gen_helper_nmi(cpu_env, RsV)

#define fGEN_TCG_Y2_setprio(SHORTCODE) \
    gen_helper_setprio(cpu_env, PtV, RsV)

#define fGEN_TCG_Y2_start(SHORTCODE) \
    gen_helper_start(cpu_env, RsV)

#define fGEN_TCG_Y2_stop(SHORTCODE) \
    do { \
        RsV = RsV; \
        gen_helper_stop(cpu_env); \
    } while (0)

#define fGEN_TCG_Y2_tfrscrr(SHORTCODE) \
    tcg_gen_mov_tl(RdV, SsV)

#define fGEN_TCG_Y2_tfrsrcr(SHORTCODE) \
    tcg_gen_mov_tl(SdV, RsV)

#define fGEN_TCG_Y4_tfrscpp(SHORTCODE) \
    tcg_gen_mov_i64(RddV, SssV)

#define fGEN_TCG_Y4_tfrspcp(SHORTCODE) \
    tcg_gen_mov_i64(SddV, RssV)

#define fGEN_TCG_G4_tfrgcrr(SHORTCODE) \
    tcg_gen_mov_tl(RdV, GsV)

#define fGEN_TCG_G4_tfrgrcr(SHORTCODE) \
    tcg_gen_mov_tl(GdV, RsV)

#define fGEN_TCG_G4_tfrgcpp(SHORTCODE) \
    tcg_gen_mov_i64(RddV, GssV)

#define fGEN_TCG_G4_tfrgpcp(SHORTCODE) \
    tcg_gen_mov_i64(GddV, RssV)

#define fGEN_TCG_J2_rte(SHORTCODE) \
    do { \
        TCGv pkt_has_multi_cof = tcg_constant_tl(ctx->pkt->pkt_has_multi_cof); \
        TCGv PC = tcg_constant_tl(ctx->pkt->pc); \
        gen_helper_rte(cpu_env, pkt_has_multi_cof, PC); \
    } while (0)

#define fGEN_TCG_Y2_crswap0(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        tcg_gen_mov_tl(tmp, RxV); \
        tcg_gen_mov_tl(RxV, hex_t_sreg[HEX_SREG_SGP0]); \
        tcg_gen_mov_tl(hex_t_sreg_new_value[HEX_SREG_SGP0], tmp); \
        tcg_temp_free(tmp); \
    } while (0)

#define fGEN_TCG_Y4_crswap1(SHORTCODE) \
    do { \
        TCGv tmp = tcg_temp_new(); \
        tcg_gen_mov_tl(tmp, RxV); \
        tcg_gen_mov_tl(RxV, hex_t_sreg[HEX_SREG_SGP1]); \
        tcg_gen_mov_tl(hex_t_sreg_new_value[HEX_SREG_SGP1], tmp); \
        tcg_temp_free(tmp); \
    } while (0)

#define fGEN_TCG_Y4_crswap10(SHORTCODE) \
    do { \
        g_assert_not_reached(); \
        TCGv_i64 tmp = tcg_temp_new_i64(); \
        tcg_gen_mov_i64(tmp, RxxV); \
        tcg_gen_concat_i32_i64(RxxV, \
                               hex_t_sreg[HEX_SREG_SGP0], \
                               hex_t_sreg[HEX_SREG_SGP1]); \
        tcg_gen_extrl_i64_i32(hex_t_sreg_new_value[HEX_SREG_SGP0], tmp); \
        tcg_gen_extrh_i64_i32(hex_t_sreg_new_value[HEX_SREG_SGP1], tmp); \
        tcg_temp_free_i64(tmp); \
    } while (0)

#endif
