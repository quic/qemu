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

    /* Pre-decodes about COF */
    bool pkt_has_cof;          /* Has any change-of-flow */
    bool pkt_has_endloop;
    bool pkt_has_dczeroa;

    /* When a predicate cancels something, track that */
    bool pkt_has_fp_op;
    /* load store for slots */
    bool pkt_has_load_s0;
    bool pkt_has_load_s1;
    bool pkt_has_scalar_store_s0;
    bool pkt_has_scalar_store_s1;

    uint32_t pkt_hmx_st_ct:2; /* pkt has how many non vmem stores */
    uint32_t pkt_hmx_ld_ct:2; /* pkt has how many non vmem and non zmem loads */

    bool pkt_has_hvx;
    bool pkt_has_hmx;
    Insn *vhist_insn;

#ifndef CONFIG_USER_ONLY
    bool pkt_has_sys_visibility;
    bool can_do_io;
#endif
	/* This MUST be the last thing in this structure */
	Insn insn[INSTRUCTIONS_MAX];
};

typedef struct Packet Packet;

#endif
