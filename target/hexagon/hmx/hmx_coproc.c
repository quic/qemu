/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "hmx/opcodes.h"
#include "hmx/hmx_hex_arch_types.h"
#include "hmx/hmx_coproc.h"
#include "hmx/hmx.h"
#include "hmx/hmx_system.h"
#include "hmx/hmx_macros.h"
#include "hmx/macros_auto.h"
#include "hmx/ext_hmx.h"
#include "hmx/hmx_int_ops.h"

#define THREAD2STRUCT ((hmx_state_t *)(thread->processor_ptr->shared_extptr))

/* see arch_options.def */
static arch_proc_opt_t stc_arch_proc_options = {
    .vtcm_size = VTCM_SIZE,
    .vtcm_offset = VTCM_OFFSET,
    .hmxdebuglvl = 0,
    .hmx_output_depth = 32,
    .hmx_spatial_size = 6,
    .hmx_channel_size = 5,
    .hmx_block_size = 2048,
    .hmx_mxmem_debug_acc_preload = 0,
    .pmu_enable = 0,
    .hmxdebugfile = 0,
    .hmx_mxmem_debug = 0,
    .hmxaccpreloadfile = 0,
    .hmxarray_new = 0,
    .hmxmpytrace = 0,
    .hmx_v1 = 0,
    .hmx_power_config = 0,
    .hmx_8x4_mpy_mode = 0,
    .hmx_group_conv_mode = 1,
    .xfp_inexact_enable = 1,
    .xfp_cvt_frac = 13,
    .xfp_cvt_int = 3,
    .QDSP6_MX_FP_PRESENT   = 1,
    .QDSP6_MX_RATE = 16,
    .QDSP6_MX_CHANNELS = 32,
    .QDSP6_MX_ROWS = 64,
    .QDSP6_MX_COLS = 32,
    .QDSP6_MX_CVT_MPY_SZ = 10,
    .QDSP6_MX_SUB_COLS = 2,
    .QDSP6_MX_ACCUM_WIDTH = 32,
    .QDSP6_MX_CVT_WIDTH = 12,
    .QDSP6_MX_FP_RATE = 8,
    .QDSP6_MX_FP_ACC_INT = 8,
    .QDSP6_MX_FP_ACC_FRAC = 22,
    .QDSP6_MX_FP_ACC_EXP = 7,
    .QDSP6_MX_FP_ROWS = 32,
    .QDSP6_MX_FP_COLS = 32,
    .QDSP6_MX_FP_ACC_NORM = 3,
    .QDSP6_MX_PARALLEL_GRPS = 4,
    .QDSP6_MX_NUM_BIAS_GRPS = 4,
    .QDSP6_VX_VEC_SZ = 1024,
};

static options_struct stc_options = {
    .hmx_mac_fxp_callback = (void *)0,
    .hmx_mac_flt_callback = (void *)0,
    .hmx_cvt_fxp_state_callback = (void *)0,
    .hmx_cvt_state_transfer_callback = (void *)0,
    .hmx_cvt_state_write_callback = (void *)0,
    .hmx_wgt_decomp_callback = (void *)0,
};

static processor_t stc_proc_env = {
    .arch_proc_options = &stc_arch_proc_options,
    .options = &stc_options,
    .shared_extptr = (void *)0,
};

static system_t stc_system = {
    .vtcm_haddr = (void *)0,
    .vtcm_base = 0,
};

thread_t glb_thread_env = {
    .processor_ptr = &stc_proc_env,
    .system_ptr = &stc_system,
};

#define thread (&glb_thread_env)

static void hmx_coproc_init(void *vtcm_haddr, hwaddr vtcm_base, uint32_t reg_usr)

{
    stc_system.vtcm_haddr = vtcm_haddr;
    stc_system.vtcm_base = vtcm_base;
    thread->reg_usr = reg_usr;
    stc_proc_env.shared_extptr = (hmx_state_t *)hmx_ext_palloc(thread->processor_ptr, 0);
    hmx_configure_state(thread);
}

static void hmx_coproc_reset(void)

{
    hmx_reset(&stc_proc_env, thread);
}


static void hmx_coproc_commit(void)
{
    hmx_ext_commit_regs(thread, 0);
    hmx_ext_commit_mem(thread, 0, 0);
}

/* NOTE: the rpc server should call this function not on the qemu side */
void hmx_coproc(CoprocArgs args)
{
     switch (args.opcode) {
 #include "coproc_cases_generated.c.inc"
        break;
    case COPROC_INIT:
        hmx_coproc_init(args.vtcm_haddr, args.vtcm_base, args.reg_usr);
        break;
    case COPROC_RESET:
        hmx_coproc_reset();
        break;
    case COPROC_COMMIT:
        hmx_coproc_commit();
        break;
    default:
        /* Ignore unknown opcode */
        break;
    }
}

