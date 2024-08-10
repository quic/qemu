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
        VMSTATE_CPU(),
        VMSTATE_UINTTL_ARRAY(env.gpr, HexagonCPU, TOTAL_PER_THREAD_REGS),
        VMSTATE_UINTTL_ARRAY(env.pred, HexagonCPU, NUM_PREGS),
        VMSTATE_UINTTL_ARRAY(env.t_sreg, HexagonCPU, NUM_SREGS),
        VMSTATE_UINTTL_ARRAY(env.t_sreg_written, HexagonCPU, NUM_SREGS),
        VMSTATE_UINTTL_ARRAY(env.greg, HexagonCPU, NUM_GREGS),
        VMSTATE_UINTTL_ARRAY(env.greg_written, HexagonCPU, NUM_GREGS),
        VMSTATE_UINTTL(env.next_PC, HexagonCPU),
        VMSTATE_UINTTL(env.tlb_lock_state, HexagonCPU),
        VMSTATE_UINTTL(env.k0_lock_state, HexagonCPU),
        VMSTATE_UINTTL(env.threadId, HexagonCPU),
        VMSTATE_END_OF_LIST()
    },
};

