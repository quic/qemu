/*
 * Hexagon Baseboard System emulation.
 *
 * Copyright (c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef HEXAGON_MACHINE_CONFIGS_H
#define HEXAGON_MACHINE_CONFIGS_H

uint32_t v66g_1024_cfgtable[] = {
 /* The following values were derived from hexagon-sim, QuRT config files */
 /* and the hexagon scoreboard for v66g_1024 found here: http://go/q6area */
    0xd8000000, /* l2tcm_base         */
    0x0000d400, /* reserved           */
    0x00002000, /* subsystem_base     */
    0xd8050000, /* etm_base           */
    0xd81a0000, /* l2cfg_base         */
    0x00000000, /* reserved2          */
    0xd8200000, /* l1s0_base          */
    0x00003000, /* axi2_lowaddr       */
    0x00000000, /* streamer_base      */
    0xd8190000, /* clade_base         */
    0xd81e0000, /* fastl2vic_base     */
    0x00000080, /* jtlb_size_entries  */
    0x00000001, /* coproc_present     */
    0x00000004, /* ext_contexts       */
    0xd8200000, /* vtcm_base          */
    0x00000100, /* vtcm_size_kb       */
    0x00000400, /* l2tag_size         */
    0x00000400, /* l2ecomem_size      */
    0x0000000f, /* thread_enable_mask */
    0xd81f0000, /* eccreg_base        */
    0x00000080, /* l2line_size        */
    0x00000000, /* tiny_core          */
    0x00000000, /* l2itcm_size        */
    0xd8200000, /* l2itcm_base        */
    0x00000000, /* clade2_base        */
    0x00000000, /* dtm_present        */
    0x00000000, /* dma_version        */
    0x00000080, /* hvx_vec_log_length */
    0x00000000, /* core_id            */
    0x00000000, /* core_count         */
    0x00000000, /* hmx_int8_spatial   */
    0x00000000, /* hmx_int8_depth     */
    0x00000000, /* v2x_mode           */
    0x00000000, /* hmx_int8_rate      */
    0x00000000, /* hmx_fp16_spatial   */
    0x00000000, /* hmx_fp16_depth     */
    0x00000000, /* hmx_fp16_rate      */
    0x00000000, /* hmx_fp16_acc_frac  */
    0x00000000, /* hmx_fp16_acc_int   */
    0x00000000, /* acd_preset         */
    0x00000000, /* mnd_preset         */
    0x00000000, /* l1d_size_kb        */
    0x00000000, /* l1i_size_kb        */
    0x00000000, /* l1d_write_policy   */
    0x00000000, /* vtcm_bank_width    */
    0x00000000, /* reserved3[0]       */
    0x00000000, /* reserved3[1]       */
    0x00000000, /* reserved3[2]       */
    0x00000000, /* hmx_cvt_mpy_size   */
    0x00000000, /* axi3_lowaddr       */
};

uint32_t v66g_1024_extensions[] = {
    0xd8180000, /* cfgbase            */
    0x00000400, /* cfgtable_size      */
    0x00000000, /* l2tcm_size         */
    0xfc910000, /* l2vic_base         */
    0x00001000, /* l2vic_size         */
    0xfc900000, /* csr_base           */
    0xfc921000, /* qtmr_rg0           */
    0xfc922000, /* qtmr_rg1           */

};

uint32_t v68n_1024_cfgtable[] = {
 /* The following values were derived from hexagon-sim, QuRT config files, */
 /* and the hexagon scoreboard for v68n_1024 found here: http://go/q6area  */
    0xd8000000, /* l2tcm_base         */
    0x00000000, /* reserved           */
    0x00002000, /* subsystem_base     */
    0xd8190000, /* etm_base           */
    0xd81a0000, /* l2cfg_base         */
    0x00000000, /* reserved2          */
    0xd8400000, /* l1s0_base          */
    0x00003000, /* axi2_lowaddr       */
    0xd81c0000, /* streamer_base      */
    0xd81d0000, /* clade_base         */
    0xd81e0000, /* fastl2vic_base     */
    0x00000080, /* jtlb_size_entries  */
    0x00000001, /* coproc_present     */
    0x00000004, /* ext_contexts       */
    0xd8400000, /* vtcm_base          */
    0x00001000, /* vtcm_size_kb       */
    0x00000400, /* l2tag_size         */
    0x00000400, /* l2ecomem_size      */
    0x0000003f, /* thread_enable_mask */
    0xd81f0000, /* eccreg_base        */
    0x00000080, /* l2line_size        */
    0x00000000, /* tiny_core          */
    0x00000000, /* l2itcm_size        */
    0xd8200000, /* l2itcm_base        */
    0x00000000, /* clade2_base        */
    0x00000000, /* dtm_present        */
    0x00000001, /* dma_version        */
    0x00000007, /* hvx_vec_log_length */
    0x00000000, /* core_id            */
    0x00000000, /* core_count         */
    0x00000040, /* hmx_int8_spatial   */
    0x00000020, /* hmx_int8_depth     */
    0x1f1f1f1f, /* v2x_mode           */
    0x1f1f1f1f, /* hmx_int8_rate      */
    0x1f1f1f1f, /* hmx_fp16_spatial   */
    0x1f1f1f1f, /* hmx_fp16_depth     */
    0x1f1f1f1f, /* hmx_fp16_rate      */
    0x1f1f1f1f, /* hmx_fp16_acc_frac  */
    0x1f1f1f1f, /* hmx_fp16_acc_int   */
    0x1f1f1f1f, /* acd_preset         */
    0x1f1f1f1f, /* mnd_preset         */
    0x1f1f1f1f, /* l1d_size_kb        */
    0x1f1f1f1f, /* l1i_size_kb        */
    0x1f1f1f1f, /* l1d_write_policy   */
    0x1f1f1f1f, /* vtcm_bank_width    */
    0x1f1f1f1f, /* reserved3[0]       */
    0x1f1f1f1f, /* reserved3[1]       */
    0x1f1f1f1f, /* reserved3[2]       */
    0x1f1f1f1f, /* hmx_cvt_mpy_size   */
    0x1f1f1f1f, /* axi3_lowaddr       */
};

uint32_t v68n_1024_extensions[] = {
    0xde000000, /* cfgbase            */
    0x00000400, /* cfgtable_size      */
    0x00000000, /* l2tcm_size         */
    0xfc910000, /* l2vic_base         */
    0x00001000, /* l2vic_size         */
    0xfc900000, /* csr_base           */
    0xfc921000, /* qtmr_rg0           */
    0xfc922000, /* qtmr_rg1           */
};

uint32_t v69na_1024_cfgtable[] = {
 /* The following values were derived from hexagon-sim, QuRT config files, */
 /* and the hexagon scoreboard for v69na_1024 found here: http://go/q6area */
    0xd8000000, /* l2tcm_base         */
    0x00000000, /* reserved           */
    0x00002000, /* subsystem_base     */
    0xd8190000, /* etm_base           */
    0xd81a0000, /* l2cfg_base         */
    0x00000000, /* reserved2          */
    0xd8400000, /* l1s0_base          */
    0x00003000, /* axi2_lowaddr       */
    0xd81c0000, /* streamer_base      */
    0xd81d0000, /* clade_base         */
    0xd81e0000, /* fastl2vic_base     */
    0x00000080, /* jtlb_size_entries  */
    0x00000001, /* coproc_present     */
    0x00000004, /* ext_contexts       */
    0xd9000000, /* vtcm_base          */
    0x00002000, /* vtcm_size_kb       */
    0x00000400, /* l2tag_size         */
    0x00000400, /* l2ecomem_size      */
    0x0000003f, /* thread_enable_mask */
    0xd81f0000, /* eccreg_base        */
    0x00000080, /* l2line_size        */
    0x00000000, /* tiny_core          */
    0x00000000, /* l2itcm_size        */
    0xd8200000, /* l2itcm_base        */
    0xd81b0000, /* clade2_base        */
    0x00000000, /* dtm_present        */
    0x00000001, /* dma_version        */
    0x00000007, /* hvx_vec_log_length */
    0x00000000, /* core_id            */
    0x00000000, /* core_count         */
    0x00000040, /* hmx_int8_spatial   */
    0x00000020, /* hmx_int8_depth     */
    0x1f1f1f1f, /* v2x_mode           */
    0x1f1f1f1f, /* hmx_int8_rate      */
    0x1f1f1f1f, /* hmx_fp16_spatial   */
    0x1f1f1f1f, /* hmx_fp16_depth     */
    0x1f1f1f1f, /* hmx_fp16_rate      */
    0x1f1f1f1f, /* hmx_fp16_acc_frac  */
    0x1f1f1f1f, /* hmx_fp16_acc_int   */
    0x1f1f1f1f, /* acd_preset         */
    0x1f1f1f1f, /* mnd_preset         */
    0x1f1f1f1f, /* l1d_size_kb        */
    0x1f1f1f1f, /* l1i_size_kb        */
    0x1f1f1f1f, /* l1d_write_policy   */
    0x1f1f1f1f, /* vtcm_bank_width    */
    0x1f1f1f1f, /* reserved3[0]       */
    0x1f1f1f1f, /* reserved3[1]       */
    0x1f1f1f1f, /* reserved3[2]       */
    0x1f1f1f1f, /* hmx_cvt_mpy_size   */
    0x1f1f1f1f, /* axi3_lowaddr       */
};

uint32_t v69na_1024_extensions[] = {
    0xfc900000, /* csr_base           */
    0x00000400, /* cfgtable_size      */
    0x00000000, /* l2tcm_size         */
    0xd81e0000, /* l2vic_base         */
    0x00001000, /* l2vic_size         */
    0xfc900000, /* csr_base           */
    0xfc921000, /* qtmr_rg0           */
    0xfc922000, /* qtmr_rg1           */
};

#endif
