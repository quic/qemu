/* SPDX-License-Identifier: GPL-2.0-or-later */
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

#include "qemu/osdep.h"
#include "migration/cpu.h"
#include "cpu.h"

const VMStateDescription vmstate_hexagon_cpu = {
    .name = "cpu",
    .version_id = 0,
    .minimum_version_id = 0,
    .fields = (VMStateField[]) {

        VMSTATE_UINTTL_ARRAY(env.gpr, HexagonCPU, TOTAL_PER_THREAD_REGS),
        VMSTATE_UINTTL_ARRAY(env.pred, HexagonCPU, NUM_PREGS),
        VMSTATE_UINTTL_ARRAY(env.vstore_pending, HexagonCPU, VSTORES_MAX),
        VMSTATE_UINTTL_ARRAY(env.t_sreg, HexagonCPU, NUM_SREGS),
        VMSTATE_UINTTL_ARRAY(env.t_sreg_written, HexagonCPU, NUM_SREGS),
        VMSTATE_UINTTL_ARRAY(env.greg, HexagonCPU, NUM_GREGS),
        VMSTATE_UINTTL_ARRAY(env.greg_written, HexagonCPU, NUM_GREGS),
        VMSTATE_UINTTL_ARRAY(env.reg_written, HexagonCPU, TOTAL_PER_THREAD_REGS),

        VMSTATE_VARRAY_UINT32(env.g_sreg, HexagonCPU, vmstate_num_g_sreg, 0,
                              vmstate_info_uint32, uint32_t),
        VMSTATE_VARRAY_UINT32(env.g_gcycle, HexagonCPU, vmstate_num_g_gcycle, 0,
                              vmstate_info_uint32, uint32_t),

        VMSTATE_UINTTL(env.cause_code, HexagonCPU),
        VMSTATE_UINTTL(env.last_pc_dumped, HexagonCPU),
        VMSTATE_UINTTL(env.stack_start, HexagonCPU),
        VMSTATE_UINTTL(env.wait_next_pc, HexagonCPU),
        VMSTATE_UINTTL(env.new_value_usr, HexagonCPU),
        VMSTATE_UINTTL(env.llsc_addr, HexagonCPU),
        VMSTATE_UINTTL(env.llsc_val, HexagonCPU),
        VMSTATE_UINTTL(env.imprecise_exception, HexagonCPU),

        VMSTATE_UINT8(env.bq_on, HexagonCPU),
        VMSTATE_UINT8(env.slot_cancelled, HexagonCPU),

        VMSTATE_UINT16(env.nmi_threads, HexagonCPU),

        VMSTATE_UINT32(env.last_cpu, HexagonCPU),
        VMSTATE_UINT32(env.exe_arch, HexagonCPU),
        VMSTATE_UINT32(env.vtcm_size, HexagonCPU),
        VMSTATE_UINT32(env.timing_on, HexagonCPU),
        VMSTATE_UINT32(env.threadId, HexagonCPU),

        VMSTATE_INT32(env.status, HexagonCPU),
        VMSTATE_INT32(env.memfd_fd, HexagonCPU),
        VMSTATE_INT32(env.slot, HexagonCPU),

        VMSTATE_UINT64(env.llsc_val_i64, HexagonCPU),
        VMSTATE_UINT64(env.t_cycle_count, HexagonCPU),
        VMSTATE_UINT64(env.pktid, HexagonCPU),

        VMSTATE_BOOL(env.vtcm_pending, HexagonCPU),
        VMSTATE_BOOL(env.ss_pending, HexagonCPU),

        VMSTATE_ARRAY(env.vtmp.ud, HexagonCPU, MAX_VEC_SIZE_BYTES / 8, 0,
                      vmstate_info_uint64, uint64_t),

        VMSTATE_END_OF_LIST()
    },
};
