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

#ifndef HEXAGON_INSN_H
#define HEXAGON_INSN_H

#include "cpu.h"

#define INSTRUCTIONS_MAX 7    /* 2 pairs + loopend */
#define REG_OPERANDS_MAX 5
#define IMMEDS_MAX 2

typedef struct CPUHexagonState CPUHexagonState;
struct Instruction;
struct Packet;
struct DisasContext;

typedef void (*SemanticInsn)(CPUHexagonState *env,
                             struct DisasContext *ctx,
                             struct Instruction *insn,
                             struct Packet *pkt);

struct Instruction {
    SemanticInsn generate;            /* pointer to genptr routine */
    uint8_t regno[REG_OPERANDS_MAX];    /* reg operands including predicates */
    uint16_t opcode;

    uint32_t iclass:6;
    uint32_t slot:3;

    uint32_t which_extended:1;    /* If has an extender, which immediate */
    uint32_t new_value_producer_slot:4;
    uint32_t part1:1;              /*
                              * cmp-jumps are split into two insns.
                              * set for the compare and clear for the jump
                              */
    uint32_t extension_valid:1;   /* Has a constant extender attached */
    uint32_t is_dcop:1;      /* Is a dcacheop */
    uint32_t is_dcfetch:1;   /* Has an A_DCFETCH attribute */
    uint32_t is_load:1;      /* Has A_LOAD attribute */
    uint32_t is_store:1;     /* Has A_STORE attribute */
    uint32_t is_vmem_ld:1;   /* Has an A_LOAD and an A_VMEM attribute */
    uint32_t is_scatgath:1;  /* Has an A_CVI_GATHER or A_CVI_SCATTER attr */
    uint32_t is_memop:1;     /* Has A_MEMOP attribute */
    uint32_t is_dealloc:1;   /* Is a dealloc return or dealloc frame */
    uint32_t is_aia:1;       /* Is a post increment */
    uint32_t is_endloop:1;   /* This is an end of loop */
    uint32_t is_2nd_jump:1;  /* This is the second jump of a dual-jump packet */
    uint32_t hvx_resource:8;
    int32_t immed[IMMEDS_MAX];    /* immediate field */
};

typedef struct Instruction Insn;

struct Packet {
    uint16_t num_insns;
    uint16_t encod_pkt_size_in_bytes;

    /* Pre-decodes about LD/ST */
    uint32_t single_load:1;
    uint32_t dual_load:1;
    uint32_t single_store:1;
    uint32_t dual_store:1;
    uint32_t load_and_store:1;
    uint32_t memop_or_nvstore:1;

    /* Pre-decodes about COF */
    uint32_t pkt_has_cof:1;          /* Has any change-of-flow */
    uint32_t pkt_has_dual_jump:1;
    uint32_t pkt_has_initloop:1;
    uint32_t pkt_has_initloop0:1;
    uint32_t pkt_has_initloop1:1;
    uint32_t pkt_has_endloop:1;
    uint32_t pkt_has_endloop0:1;
    uint32_t pkt_has_endloop1:1;
    uint32_t pkt_has_endloop01:1;
    uint32_t pkt_has_call:1;
    uint32_t pkt_has_jumpr:1;
    uint32_t pkt_has_cjump:1;
    uint32_t pkt_has_cjump_dotnew:1;
    uint32_t pkt_has_cjump_dotold:1;
    uint32_t pkt_has_cjump_newval:1;
    uint32_t pkt_has_duplex:1;
    uint32_t pkt_has_payload:1;      /* Contains a constant extender */
    uint32_t pkt_has_dealloc_return:1;
    uint32_t pkt_has_jumpr_return:1;  /* indirect jump that is a return */
    uint32_t pkt_has_hinted_cof:1;    /* CallRH/JumpRH */
    /* Pre-decodes about SLOTS (4 bits, one per slot) */
    uint8_t valid_slots:4;
    uint8_t valid_slots_ld:4;
    uint8_t valid_slots_st:4;
    uint8_t valid_slots_non_mem:4;

    /* Pre-decodes about SLOTS */
    uint32_t slot0_valid:1;
    uint32_t slot1_valid:1;
    uint32_t slot2_valid:1;
    uint32_t slot3_valid:1;

    /* When a predicate cancels something, track that */
    uint32_t pkt_has_fp_op:1;
    uint32_t pkt_has_fpsp_op:1;
    uint32_t pkt_has_fpdp_op:1;

    /* Contains a cacheop */
    uint32_t pkt_has_cacheop:1;
    uint32_t pkt_has_dczeroa:1;
    uint32_t pkt_has_ictagop:1;
    uint32_t pkt_has_icflushop:1;
    uint32_t pkt_has_dcflushop:1;
    uint32_t pkt_has_dctagop:1;
    uint32_t pkt_has_l2flushop:1;
    uint32_t pkt_has_l2tagop:1;

    /* load store for slots */
    uint32_t pkt_has_load_s0:1;
    uint32_t pkt_has_load_s1:1;
    uint32_t pkt_has_scalar_store_s0:1;
    uint32_t pkt_has_scalar_store_s1:1;

    /* Misc */
    uint32_t num_rops:4;            /* Num risc ops in the packet */
	uint32_t pkt_has_long_latency_insn:1;
	uint32_t pkt_page_is_stable:1;	/* If this PA is 'stable' from the host */
	uint32_t pkt_has_vecx:1;
	uint32_t pkt_has_l1s_scalar:1; /* Is there a scalar load store going to l1s */
    uint32_t pkt_has_vtcm_access:1; /* Is a vmem access going to VTCM */
    uint32_t pkt_has_vmemu_access:1; /* VMEMU access, different from double access */
    uint32_t pkt_access_count:2;    /* Is a vmem access going to VTCM */
    uint32_t pkt_ldaccess_l2:2;     /* vmem ld access to l2 */
    uint32_t pkt_ldaccess_vtcm:2;   /* vmem ld access to vtcm */
    uint32_t double_access_vec:1; /* double vector access for v and z load */
  	uint32_t pkt_vmem_ld_ct:2; /* pkt has how many vmem loads */
  	uint32_t pkt_vmem_st_ct:2; /* pkt has how many vmem stores */
	uint32_t pkt_zmem_ld_ct:2; /* pkt has how many zmem loads */
	uint32_t pkt_has_scatgath:1; /* pkt has scatter gather */
	uint32_t pkt_has_vmemu:1; /* pkt has unaligned vmem, this is different from pkt_has_vmemu_access which is runtime  */
    uint32_t pkt_nonvmem_st_ct:2; /* pkt has how many non vmem stores */
    uint32_t pkt_nonvmem_nonzmem_ld_ct:2; /* pkt has how many non vmem and non zmem loads */
    uint32_t pkt_hmx_st_ct:2; /* pkt has how many non vmem stores */
    uint32_t pkt_hmx_ld_ct:2; /* pkt has how many non vmem and non zmem loads */
    uint8_t pkt_memport_ct:2; /*pkt use number of mem ports*/
    uint8_t pkt_memport_s0:1; /*pkt use mem port by instruction on slot 0*/
    uint8_t pkt_memport_s1:1; /*pkt use mem port by instruction on slot 1*/
    uint8_t invalid_new_target:1; /*
                                   * Packet loads a new value from invalid instr
                                   * (likely .tmp)
                                   */

    uint8_t pkt_has_dword_store:1;
    uint8_t pkt_has_dword_load:1;
    uint32_t pkt_hvx_va:4;
    uint32_t pkt_hvx_vx:4;
    uint32_t pkt_hvx_vp:4;
    uint32_t pkt_hvx_vs:4;
    uint32_t pkt_hvx_all:4;
    uint32_t pkt_hvx_none:4;
	uint32_t pkt_hvx_insn:4;
	uint32_t pkt_hvx_vs_3src:1;
	uint32_t pkt_hvx_va_2src:1;

    uint32_t pkt_has_hvx:1;
    uint32_t pkt_has_hmx:1;
    Insn *vhist_insn;
    uint32_t pkt_has_extension:1;


	uint32_t native_pkt:1;
	uint32_t total_memop:2;
	struct Packet *taken_ptr;	/* predicted next packet */
	struct Packet *fallthrough_ptr;	/* predicted fall-through */

#ifndef CONFIG_USER_ONLY
    uint32_t pkt_has_sys_visibility:1;
    uint32_t can_do_io:1;
#endif
    uint32_t num_insns1; // part1 and loop end are excluded
	/* This MUST be the last thing in this structure */
	Insn insn[INSTRUCTIONS_MAX];
};

typedef struct Packet Packet;

#endif
