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

#ifdef HMX_STANDALONE_BUILD
#include "hmx/hmx_hex_arch_types.h"
#else
#include "qemu/osdep.h"
#include "exec/cpu_ldst.h"
#include "exec/exec-all.h"
#include "cpu.h"
#endif
#include "coproc.h"

/* NOTE: hmx_coproc doesnt actualy exist in qemu build it is on server */
extern void hmx_coproc(CoprocArgs args);

void coproc(CoprocArgs args)
{
    hmx_coproc(args); /* NOTE: this should be the actual rpc call to server */
}

