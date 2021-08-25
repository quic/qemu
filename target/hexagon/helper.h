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

#include "internal.h"
#include "helper_protos_generated.h.inc"

DEF_HELPER_1(commit_hmx, void, env)
DEF_HELPER_1(commit_hvx_stores, void, env)
DEF_HELPER_2(commit_store, void, env, int)
DEF_HELPER_2(conv_d2df, f64, env, s64)
DEF_HELPER_2(conv_d2sf, f32, env, s64)
DEF_HELPER_2(conv_df2d, s64, env, f64)
DEF_HELPER_2(conv_df2d_chop, s64, env, f64)
DEF_HELPER_2(conv_df2sf, f32, env, f64)
DEF_HELPER_2(conv_df2ud, i64, env, f64)
DEF_HELPER_2(conv_df2ud_chop, i64, env, f64)
DEF_HELPER_2(conv_df2uw, i32, env, f64)
DEF_HELPER_2(conv_df2uw_chop, i32, env, f64)
DEF_HELPER_2(conv_df2w, s32, env, f64)
DEF_HELPER_2(conv_df2w_chop, s32, env, f64)
DEF_HELPER_2(conv_sf2d, s64, env, f32)
DEF_HELPER_2(conv_sf2d_chop, s64, env, f32)
DEF_HELPER_2(conv_sf2df, f64, env, f32)
DEF_HELPER_2(conv_sf2ud, i64, env, f32)
DEF_HELPER_2(conv_sf2ud_chop, i64, env, f32)
DEF_HELPER_2(conv_sf2uw, i32, env, f32)
DEF_HELPER_2(conv_sf2uw_chop, i32, env, f32)
DEF_HELPER_2(conv_sf2w, s32, env, f32)
DEF_HELPER_2(conv_sf2w_chop, s32, env, f32)
DEF_HELPER_2(conv_ud2df, f64, env, s64)
DEF_HELPER_2(conv_ud2sf, f32, env, s64)
DEF_HELPER_2(conv_uw2df, f64, env, s32)
DEF_HELPER_2(conv_uw2sf, f32, env, s32)
DEF_HELPER_2(conv_w2df, f64, env, s32)
DEF_HELPER_2(conv_w2sf, f32, env, s32)
DEF_HELPER_2(creg_read, i32, env, i32)
DEF_HELPER_2(creg_read_pair, i64, env, i32)
DEF_HELPER_FLAGS_3(debug_check_store_width, TCG_CALL_NO_WG, void, env, int, int)
DEF_HELPER_FLAGS_3(debug_commit_end, TCG_CALL_NO_WG, void, env, int, int)
DEF_HELPER_1(debug_start_packet, void, env)
DEF_HELPER_2(debug_value, void, env, s32)
DEF_HELPER_2(debug_value_i64, void, env, s64)
DEF_HELPER_3(dfadd, f64, env, f64, f64)
DEF_HELPER_3(dfclass, s32, env, f64, s32)
DEF_HELPER_3(dfcmpeq, s32, env, f64, f64)
DEF_HELPER_3(dfcmpge, s32, env, f64, f64)
DEF_HELPER_3(dfcmpgt, s32, env, f64, f64)
DEF_HELPER_3(dfcmpuo, s32, env, f64, f64)
DEF_HELPER_3(dfmax, f64, env, f64, f64)
DEF_HELPER_3(dfmin, f64, env, f64, f64)
DEF_HELPER_3(dfmpyfix, f64, env, f64, f64)
DEF_HELPER_4(dfmpyhh, f64, env, f64, f64, f64)
DEF_HELPER_3(dfsub, f64, env, f64, f64)
DEF_HELPER_FLAGS_1(fbrev, TCG_CALL_NO_RWG_SE, i32, i32)
DEF_HELPER_FLAGS_4(fcircadd, TCG_CALL_NO_RWG_SE, s32, s32, s32, s32, s32)
DEF_HELPER_4(gather_store, void, env, i32, ptr, int)
DEF_HELPER_3(debug_print_vec, void, env, s32, ptr)
DEF_HELPER_3(invalid_width, void, env, i32, i32)
DEF_HELPER_3(merge_inflight_store1s, s32, env, s32, s32)
DEF_HELPER_3(merge_inflight_store1u, s32, env, s32, s32)
DEF_HELPER_3(merge_inflight_store2s, s32, env, s32, s32)
DEF_HELPER_3(merge_inflight_store2u, s32, env, s32, s32)
DEF_HELPER_3(merge_inflight_store4s, s32, env, s32, s32)
DEF_HELPER_3(merge_inflight_store4u, s32, env, s32, s32)
DEF_HELPER_3(merge_inflight_store8u, s64, env, s32, s64)
DEF_HELPER_FLAGS_2(raise_exception, TCG_CALL_NO_RETURN, noreturn, env, i32)
DEF_HELPER_3(sfadd, f32, env, f32, f32)
DEF_HELPER_3(sfclass, s32, env, f32, s32)
DEF_HELPER_3(sfcmpeq, s32, env, f32, f32)
DEF_HELPER_3(sfcmpge, s32, env, f32, f32)
DEF_HELPER_3(sfcmpgt, s32, env, f32, f32)
DEF_HELPER_3(sfcmpuo, s32, env, f32, f32)
DEF_HELPER_3(sffixupd, f32, env, f32, f32)
DEF_HELPER_3(sffixupn, f32, env, f32, f32)
DEF_HELPER_2(sffixupr, f32, env, f32)
DEF_HELPER_4(sffma, f32, env, f32, f32, f32)
DEF_HELPER_4(sffma_lib, f32, env, f32, f32, f32)
DEF_HELPER_5(sffma_sc, f32, env, f32, f32, f32, f32)
DEF_HELPER_4(sffms, f32, env, f32, f32, f32)
DEF_HELPER_4(sffms_lib, f32, env, f32, f32, f32)
DEF_HELPER_2(sfinvsqrta, i64, env, f32)
DEF_HELPER_3(sfmax, f32, env, f32, f32)
DEF_HELPER_3(sfmin, f32, env, f32, f32)
DEF_HELPER_3(sfmpy, f32, env, f32, f32)
DEF_HELPER_3(sfrecipa, i64, env, f32, f32)
DEF_HELPER_3(sfsub, f32, env, f32, f32)
DEF_HELPER_FLAGS_4(vacsh_pred, TCG_CALL_NO_RWG_SE, s32, env, s64, s64, s64)
DEF_HELPER_4(vacsh_val, s64, env, s64, s64, s64)


#if !defined(CONFIG_USER_ONLY)
DEF_HELPER_1(check_hmx, void, env)
DEF_HELPER_1(check_hvx, void, env)
DEF_HELPER_1(checkforguest, void, env)
DEF_HELPER_1(checkforpriv, void, env)
DEF_HELPER_2(ciad, void, env, i32)
DEF_HELPER_2(clear_run_mode, void, env, i32)
DEF_HELPER_2(fresume, void, env, i32)
DEF_HELPER_2(fstart, void, env, i32)
DEF_HELPER_2(fwait, void, env, i32)
DEF_HELPER_1(get_ready_count, i32, env)
DEF_HELPER_2(getimask, i32, env, i32)
DEF_HELPER_2(greg_read, i32, env, i32)
DEF_HELPER_2(greg_read_pair, i64, env, i32)
DEF_HELPER_2(iassignr, i32, env, i32)
DEF_HELPER_2(iassignw, void, env, i32)
DEF_HELPER_1(inc_gcycle_xt, void, env)
DEF_HELPER_3(modify_ssr, void, env, i32, i32)
DEF_HELPER_2(nmi, void, env, i32)
DEF_HELPER_1(pending_interrupt, void, env)
DEF_HELPER_6(probe_pkt_stores, void, env, int, int, int, int, int)
DEF_HELPER_3(raise_stack_overflow, void, env, i32, i32)
DEF_HELPER_1(resched, void, env)
DEF_HELPER_3(setimask, void, env, i32, i32)
DEF_HELPER_3(setprio, void, env, i32, i32)
DEF_HELPER_2(sreg_read, i32, env, i32)
DEF_HELPER_2(sreg_read_pair, i64, env, i32)
DEF_HELPER_3(sreg_write, void, env, i32, i32)
DEF_HELPER_3(sreg_write_pair, void, env, i32, i64)
DEF_HELPER_2(swi, void, env, i32)
#endif

