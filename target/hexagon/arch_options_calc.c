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

//#ifndef _ARCH_OPTIONS_CALC_H_
//#define _ARCH_OPTIONS_CALC_H_

#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch_options_calc.h"
#include <math.h>

#define min(a,b) ((a > b) ? a: b)

/**********   Arch Options Calculated Functions   **********/

/* HMX depth size in log2 */
int get_hmx_channel_size(const processor_t *proc) {
	return log2((float)proc->arch_proc_options->QDSP6_MX_CHANNELS);
}

int get_hmx_block_bit(processor_t *proc) {
	return get_hmx_channel_size(proc) + proc->arch_proc_options->hmx_spatial_size;
}

//int get_hmx_act_buf(processor_t *proc) {
//	return (proc->arch_proc_options->QDSP6_VX_VTCM_ILVS != 0 ) ? proc->arch_proc_options->QDSP6_MX_ACT_BUF*16/proc->arch_proc_options->QDSP6_VX_VTCM_ILVS : 8;
//}
//#endif
