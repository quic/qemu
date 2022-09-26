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

#ifndef _ARCH_OPTIONS_CALC_H_
#define _ARCH_OPTIONS_CALC_H_

#ifndef CONFIG_USER_ONLY

#define VTCM_CFG_BASE_OFF 0x38
#define VTCM_CFG_SIZE_OFF 0x3c

#define in_vtcm_space(proc, paddr, warning) \
    in_vtcm_space_impl(thread, paddr, warning)

static inline bool in_vtcm_space_impl(thread_t *thread, paddr_t paddr,
    int warning)

{
    static bool init_needed = true;
    static paddr_t vtcm_base;
    static target_ulong vtcm_size;

    if (init_needed == true) {
        init_needed = false;
        hwaddr cfgbase = (hwaddr)ARCH_GET_SYSTEM_REG(thread, HEX_SREG_CFGBASE)
            << 16;
        cpu_physical_memory_read(cfgbase + VTCM_CFG_BASE_OFF, &vtcm_base,
            sizeof(target_ulong));
        vtcm_base <<= 16;
        cpu_physical_memory_read(cfgbase + VTCM_CFG_SIZE_OFF, &vtcm_size,
            sizeof(target_ulong));
        vtcm_size *= 1024;
    }

    return paddr >= vtcm_base && paddr < (vtcm_base + vtcm_size);
}
#else
#define in_vtcm_space(proc, paddr, warning) 1
#endif

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

int get_ext_contexts(processor_t *proc);

/**********   End ofArch Options Calculated Functions   **********/

#endif
