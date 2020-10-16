/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

DEF_HELPER_FLAGS_2(raise_exception, TCG_CALL_NO_RETURN, noreturn, env, i32)
DEF_HELPER_1(debug_start_packet, void, env)
DEF_HELPER_FLAGS_3(debug_check_store_width, TCG_CALL_NO_WG, void, env, int, int)
DEF_HELPER_2(commit_store, void, env, int)
DEF_HELPER_1(commit_hvx_stores, void, env)
DEF_HELPER_FLAGS_3(debug_commit_end, TCG_CALL_NO_WG, void, env, int, int)
DEF_HELPER_FLAGS_2(sfrecipa_val, TCG_CALL_NO_RWG_SE, s32, s32, s32)
DEF_HELPER_FLAGS_2(sfrecipa_pred, TCG_CALL_NO_RWG_SE, s32, s32, s32)
DEF_HELPER_FLAGS_1(sfinvsqrta_val, TCG_CALL_NO_RWG_SE, s32, s32)
DEF_HELPER_FLAGS_1(sfinvsqrta_pred, TCG_CALL_NO_RWG_SE, s32, s32)
DEF_HELPER_4(vacsh_val, s64, env, s64, s64, s64)
DEF_HELPER_4(vacsh_pred, s32, env, s64, s64, s64)

/* Floating point */
DEF_HELPER_2(conv_sf2df, f64, env, f32)
DEF_HELPER_2(conv_df2sf, f32, env, f64)
DEF_HELPER_2(conv_uw2sf, f32, env, s32)
DEF_HELPER_2(conv_uw2df, f64, env, s32)
DEF_HELPER_2(conv_w2sf, f32, env, s32)
DEF_HELPER_2(conv_w2df, f64, env, s32)
DEF_HELPER_2(conv_ud2sf, f32, env, s64)
DEF_HELPER_2(conv_ud2df, f64, env, s64)
DEF_HELPER_2(conv_d2sf, f32, env, s64)
DEF_HELPER_2(conv_d2df, f64, env, s64)
DEF_HELPER_2(conv_sf2uw, s32, env, f32)
DEF_HELPER_2(conv_sf2w, s32, env, f32)
DEF_HELPER_2(conv_sf2ud, s64, env, f32)
DEF_HELPER_2(conv_sf2d, s64, env, f32)
DEF_HELPER_2(conv_df2uw, s32, env, f64)
DEF_HELPER_2(conv_df2w, s32, env, f64)
DEF_HELPER_2(conv_df2ud, s64, env, f64)
DEF_HELPER_2(conv_df2d, s64, env, f64)
DEF_HELPER_2(conv_sf2uw_chop, s32, env, f32)
DEF_HELPER_2(conv_sf2w_chop, s32, env, f32)
DEF_HELPER_2(conv_sf2ud_chop, s64, env, f32)
DEF_HELPER_2(conv_sf2d_chop, s64, env, f32)
DEF_HELPER_2(conv_df2uw_chop, s32, env, f64)
DEF_HELPER_2(conv_df2w_chop, s32, env, f64)
DEF_HELPER_2(conv_df2ud_chop, s64, env, f64)
DEF_HELPER_2(conv_df2d_chop, s64, env, f64)
DEF_HELPER_3(sfadd, f32, env, f32, f32)
DEF_HELPER_3(sfsub, f32, env, f32, f32)

#include "helper_protos_generated.h"
