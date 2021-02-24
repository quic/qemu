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

#ifndef _ARCH_OPTIONS_CALC_H_
#define _ARCH_OPTIONS_CALC_H_

#include "global_types.h"

#define in_vtcm_space(proc, paddr, warning) 1

struct ProcessorState;
enum {
	HIDE_WARNING,
	SHOW_WARNING
};
/**********   Arch Options Calculated Functions   **********/

/* HMX depth size in log2 */
int get_hmx_channel_size(const struct ProcessorState *proc);

int get_hmx_vtcm_act_credits(struct ProcessorState *proc);

int get_hmx_block_bit(struct ProcessorState *proc);

int get_hmx_act_buf(struct ProcessorState *proc);

/**********   End ofArch Options Calculated Functions   **********/

#endif
