#ifndef HMX_NOEXT_H
#define HMX_NOEXT_H 1

#include "cpu.h"
#include "insn.h"

#define thread_t CPUHexagonState


void *hmx_ext_palloc(processor_t *proc, int slots);
void *hmx_ext_talloc(processor_t *proc, int slots);
int hmx_ext_decode(thread_t *thread, insn_t *insn, size4u_t opcode);

void hmx_ext_init(processor_t *proc);
void hmx_ext_pfree(processor_t *proc, int xa_start, int slots);
void hmx_ext_tfree(processor_t *proc, int xa_start, int slots);

void hmx_acc_ptr_reset(processor_t *proc);


void hmx_ext_rewind(thread_t *thread);
void hmx_ext_commit_regs(thread_t *thread);
void hmx_ext_commit_rewind(thread_t *thread);
void hmx_ext_commit_mem(thread_t *thread, int slot);
void hmx_ext_alloc(processor_t *proc, int slots);

/* Replace execution pointers, opcode already determined */

int hmx_ext_decode_checks(thread_t * thread, packet_t *pkt, exception_info *einfo);
const char * hmx_ext_decode_find_iclass_slots(int opcode);
void hmx_ext_print_regs(thread_t *thread, FILE *fp);
void hmx_ext_print_reg(thread_t *thread, FILE *fp, int rnum);
void hmx_ext_dump_regs(FILE *fp, processor_t *proc, int e);
int hmx_ext_load_vreg(processor_t *proc, int extno, size4u_t regno, size4u_t wordno, size4u_t val);
int hmx_ext_load_qreg(processor_t *proc, int extno, size4u_t regno, size4u_t wordno, size4u_t val);

semantic_insn_t hmx_ext_bq_exec(int opcode); /* BQ */


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

void hmx_ext_analyze_packet(thread_t * thread, packet_t *pkt);

#endif
