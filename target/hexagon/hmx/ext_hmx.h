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
#ifndef HMX_NOEXT_H
#define HMX_NOEXT_H 1
#include "hmx/hmx_coproc.h"
#include "hmx/hmx_insn.h"

void *hmx_ext_palloc(processor_t *proc, int slots);
void *hmx_ext_talloc(processor_t *proc, int slots);
int hmx_ext_decode(thread_t *thread, Insn *insn, size4u_t opcode);

void hmx_ext_init(processor_t *proc, int extno);
void hmx_ext_pfree(processor_t *proc, int xa_start, int slots);
void hmx_ext_tfree(processor_t *proc, int xa_start, int slots);

void hmx_acc_ptr_reset(processor_t *proc);


void hmx_ext_rewind(thread_t *thread, int extno);
void hmx_ext_commit_regs(thread_t *thread, int extno);
void hmx_ext_commit_rewind(thread_t *thread);
void hmx_ext_commit_mem(thread_t *thread, int slot, int extno);
void hmx_ext_alloc(processor_t *proc, int slots);

/* Replace execution pointers, opcode already determined */

int hmx_ext_decode_checks(thread_t *thread, Packet *pkt,
                          hex_exception_info *einfo);
const char * hmx_ext_decode_find_iclass_slots(int opcode);
void hmx_ext_print_regs(thread_t *thread, FILE *fp, int extno);
void hmx_ext_print_reg(thread_t *thread, FILE *fp, int rnum, int extno);
void hmx_ext_dump_regs(FILE *fp, processor_t *proc, int extno);
int hmx_ext_load_vreg(processor_t *proc, int extno, size4u_t regno, size4u_t wordno, size4u_t val);
int hmx_ext_load_qreg(processor_t *proc, int extno, size4u_t regno, size4u_t wordno, size4u_t val);

int hmx_ext_get_acc_flt_qformat(thread_t *thread, int spatial_idx, size4u_t channel_idx, size4u_t acc_idx, size8s_t * integer, size8u_t * fractional, size4u_t * ovf);
int hmx_ext_set_acc_flt_qformat(thread_t *thread, int spatial_idx, size4u_t channel_idx, size4u_t acc_idx, size8s_t integer, size8u_t fractional, size4u_t ovf);

int	hmx_ext_set_fp_acc(thread_t *thread,  int spatial_index, int channel_index, int acc_index, size4s_t exponent, size8s_t significand_hi, size8u_t significand_lo, size4u_t ovf);
int hmx_ext_get_fp_acc(thread_t *thread,  int spatial_index, int channel_index, int acc_index, size4s_t * exponent, size8s_t * significand_hi, size8u_t * significand_lo, size4u_t * ovf);

size4u_t hmx_ext_set_cvt_state(thread_t *thread, size4u_t age, size4u_t spatial_idx, size4u_t channel_idx, size4u_t state_index, size4u_t val);
size4u_t hmx_ext_get_cvt_state(thread_t *thread, size4u_t age, size4u_t spatial_idx, size4u_t channel_idx, size4u_t state_index);

int hmx_read_flt_acc_idx(thread_t *thread);
int hmx_read_fxp_acc_idx(thread_t *thread);

void hmx_age_cvt_state(thread_t *thread);


SemanticInsn hmx_ext_bq_exec(int opcode); /* BQ */


const unsigned char *
hmx_ext_get_acc_bytes(const thread_t *thread, int extno, size4u_t regno);

const unsigned char *
hmx_ext_get_bias_bytes(const thread_t *thread, int extno, size4u_t regno);

int hmx_ext_get_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t *result);
int hmx_ext_set_acc(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t val);

int hmx_ext_get_acc_flt(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t *result);
int hmx_ext_set_acc_flt(thread_t *thread, int spatial_index, size4u_t channel_index, size4u_t wordno, size4u_t val);

int hmx_ext_get_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t *result);
int hmx_ext_set_bias(thread_t *thread, int arrayno, size4u_t channel_index, size4u_t wordno, size4u_t val);

int hmx_ext_get_ovf(thread_t *thread, int extno, size4u_t regno, size4u_t wordno, size4u_t *result);
int hmx_ext_set_ovf(thread_t *thread, int extno, size4u_t regno, size4u_t wordno, size4u_t val);

void hmx_reset(processor_t *proc, thread_t *thread);

void hmx_ext_analyze_packet(thread_t * thread, Packet *pkt);

#endif
