/*
 *  Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include <string.h>
#include "arch.h"
#include "regs.h"

reg_field_t reg_field_info[] = {
#define DEF_GLOBAL_REG(TAG,NAME,SYMBOL,NUM,OFFSET)
#define DEF_MMAP_REG(TAG,NAME,SYMBOL,NUM,OFFSET)
#define DEF_REG(TAG,NAME,SYMBOL,NUM,OFFSET)
#define DEF_REG_FIELD(TAG,NAME,START,WIDTH,DESCRIPTION)    \
  {NAME,START,WIDTH,DESCRIPTION},
#include "regs_def.h"
	{NULL, 0, 0}
};

#undef DEF_REG
#undef DEF_GLOBAL_REG
#undef DEF_REG_FIELD
#undef DEF_MMAP_REG
