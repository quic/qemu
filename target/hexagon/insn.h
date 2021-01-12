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
    uint32_t part1:1;        /*
                              * cmp-jumps are split into two insns.
                              * set for the compare and clear for the jump
                              */
    uint32_t extension_valid:1;   /* Has a constant extender attached */
    uint32_t which_extended:1;    /* If has an extender, which immediate */
    uint32_t is_endloop:1;   /* This is an end of loop */
    uint32_t new_value_producer_slot:4;
    uint32_t hvx_resource:8;
    int32_t immed[IMMEDS_MAX];    /* immediate field */
};

typedef struct Instruction Insn;

struct Packet {
    uint16_t num_insns;
    uint16_t encod_pkt_size_in_bytes;

    /* Pre-decodes about COF */
    uint32_t pkt_has_cof:1;          /* Has any change-of-flow */
    uint32_t pkt_has_endloop:1;

    uint32_t pkt_has_dczeroa:1;

    uint32_t pkt_has_store_s0:1;
    uint32_t pkt_has_store_s1:1;

    uint32_t pkt_has_hvx:1;
    uint32_t pkt_has_extension:1;

    Insn insn[INSTRUCTIONS_MAX];
};

typedef struct Packet Packet;

#endif
