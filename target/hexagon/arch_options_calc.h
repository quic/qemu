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
