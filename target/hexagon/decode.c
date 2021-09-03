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

#include "qemu/osdep.h"
#include "qemu/log.h"
#include "iclass.h"
#include "opcodes.h"
#include "attribs.h"
#include "genptr.h"
#include "decode.h"
#include "insn.h"
#include "printinsn.h"
#include "mmvec/decode_ext_mmvec.h"

#define fZXTN(N, M, VAL) ((VAL) & ((1LL << (N)) - 1))

enum {
    EXT_IDX_noext = 0,
    EXT_IDX_noext_AFTER = 4,
    EXT_IDX_mmvec = 4,
    EXT_IDX_mmvec_AFTER = 8,
    XX_LAST_EXT_IDX
};

/*
 *  Certain operand types represent a non-contiguous set of values.
 *  For example, the compound compare-and-jump instruction can only access
 *  registers R0-R7 and R16-23.
 *  This table represents the mapping from the encoding to the actual values.
 */

#define DEF_REGMAP(NAME, ELEMENTS, ...) \
    static const unsigned int DECODE_REGISTER_##NAME[ELEMENTS] = \
    { __VA_ARGS__ };
        /* Name   Num Table */
DEF_REGMAP(R_16,  16, 0, 1, 2, 3, 4, 5, 6, 7, 16, 17, 18, 19, 20, 21, 22, 23)
DEF_REGMAP(R__8,  8,  0, 2, 4, 6, 16, 18, 20, 22)
DEF_REGMAP(R_8,   8,  0, 1, 2, 3, 4, 5, 6, 7)

#define DECODE_MAPPED_REG(REGNO, NAME) \
    insn->regno[REGNO] = DECODE_REGISTER_##NAME[insn->regno[REGNO]];

typedef struct {
    const struct DectreeTable *table_link;
    const struct DectreeTable *table_link_b;
    Opcode opcode;
    enum {
        DECTREE_ENTRY_INVALID,
        DECTREE_TABLE_LINK,
        DECTREE_SUBINSNS,
        DECTREE_EXTSPACE,
        DECTREE_TERMINAL
    } type;
} DectreeEntry;

typedef struct DectreeTable {
    unsigned int (*lookup_function)(int startbit, int width, uint32_t opcode);
    unsigned int size;
    unsigned int startbit;
    unsigned int width;
    const DectreeEntry table[];
} DectreeTable;

#define DECODE_NEW_TABLE(TAG, SIZE, WHATNOT) \
    static const DectreeTable dectree_table_##TAG;

#define TABLE_LINK(TABLE)                     /* NOTHING */
#define TERMINAL(TAG, ENC)                    /* NOTHING */
#define SUBINSNS(TAG, CLASSA, CLASSB, ENC)    /* NOTHING */
#define EXTSPACE(TAG, ENC)                    /* NOTHING */
#define INVALID()                             /* NOTHING */
#define DECODE_END_TABLE(...)                 /* NOTHING */
#define DECODE_MATCH_INFO(...)                /* NOTHING */
#define DECODE_LEGACY_MATCH_INFO(...)         /* NOTHING */
#define DECODE_OPINFO(...)                    /* NOTHING */

#include "dectree_generated.h.inc"

#undef DECODE_OPINFO
#undef DECODE_MATCH_INFO
#undef DECODE_LEGACY_MATCH_INFO
#undef DECODE_END_TABLE
#undef INVALID
#undef TERMINAL
#undef SUBINSNS
#undef EXTSPACE
#undef TABLE_LINK
#undef DECODE_NEW_TABLE
#undef DECODE_SEPARATOR_BITS

#define DECODE_SEPARATOR_BITS(START, WIDTH) NULL, START, WIDTH
#define DECODE_NEW_TABLE_HELPER(TAG, SIZE, FN, START, WIDTH) \
    static const DectreeTable dectree_table_##TAG = { \
        .size = SIZE, \
        .lookup_function = FN, \
        .startbit = START, \
        .width = WIDTH, \
        .table = {
#define DECODE_NEW_TABLE(TAG, SIZE, WHATNOT) \
    DECODE_NEW_TABLE_HELPER(TAG, SIZE, WHATNOT)

#define TABLE_LINK(TABLE) \
    { .type = DECTREE_TABLE_LINK, .table_link = &dectree_table_##TABLE },
#define TERMINAL(TAG, ENC) \
    { .type = DECTREE_TERMINAL, .opcode = TAG  },
#define SUBINSNS(TAG, CLASSA, CLASSB, ENC) \
    { \
        .type = DECTREE_SUBINSNS, \
        .table_link = &dectree_table_DECODE_SUBINSN_##CLASSA, \
        .table_link_b = &dectree_table_DECODE_SUBINSN_##CLASSB \
    },
#define EXTSPACE(TAG, ENC) { .type = DECTREE_EXTSPACE },
#define INVALID() { .type = DECTREE_ENTRY_INVALID, .opcode = XX_LAST_OPCODE },

#define DECODE_END_TABLE(...) } };

#define DECODE_MATCH_INFO(...)                /* NOTHING */
#define DECODE_LEGACY_MATCH_INFO(...)         /* NOTHING */
#define DECODE_OPINFO(...)                    /* NOTHING */

#include "dectree_generated.h.inc"

#undef DECODE_OPINFO
#undef DECODE_MATCH_INFO
#undef DECODE_LEGACY_MATCH_INFO
#undef DECODE_END_TABLE
#undef INVALID
#undef TERMINAL
#undef SUBINSNS
#undef EXTSPACE
#undef TABLE_LINK
#undef DECODE_NEW_TABLE
#undef DECODE_NEW_TABLE_HELPER
#undef DECODE_SEPARATOR_BITS

static const DectreeTable dectree_table_DECODE_EXT_EXT_noext = {
    .size = 1, .lookup_function = NULL, .startbit = 0, .width = 0,
    .table = {
        { .type = DECTREE_ENTRY_INVALID, .opcode = XX_LAST_OPCODE },
    }
};

static const DectreeTable *ext_trees[XX_LAST_EXT_IDX];

static void decode_ext_init(void)
{
    int i;
    for (i = EXT_IDX_noext; i < EXT_IDX_noext_AFTER; i++) {
        ext_trees[i] = &dectree_table_DECODE_EXT_EXT_noext;
    }
    for (i = EXT_IDX_mmvec; i < EXT_IDX_mmvec_AFTER; i++) {
        ext_trees[i] = &dectree_table_DECODE_EXT_EXT_mmvec;
    }
}

typedef struct {
    uint32_t mask;
    uint32_t match;
} DecodeITableEntry;

#define DECODE_NEW_TABLE(TAG, SIZE, WHATNOT)  /* NOTHING */
#define TABLE_LINK(TABLE)                     /* NOTHING */
#define TERMINAL(TAG, ENC)                    /* NOTHING */
#define SUBINSNS(TAG, CLASSA, CLASSB, ENC)    /* NOTHING */
#define EXTSPACE(TAG, ENC)                    /* NOTHING */
#define INVALID()                             /* NOTHING */
#define DECODE_END_TABLE(...)                 /* NOTHING */
#define DECODE_OPINFO(...)                    /* NOTHING */

#define DECODE_MATCH_INFO_NORMAL(TAG, MASK, MATCH) \
    [TAG] = { \
        .mask = MASK, \
        .match = MATCH, \
    },

#define DECODE_MATCH_INFO_NULL(TAG, MASK, MATCH) \
    [TAG] = { .match = ~0 },

#define DECODE_MATCH_INFO(...) DECODE_MATCH_INFO_NORMAL(__VA_ARGS__)
#define DECODE_LEGACY_MATCH_INFO(...) /* NOTHING */

static const DecodeITableEntry decode_itable[XX_LAST_OPCODE] = {
#include "dectree_generated.h.inc"
};

#undef DECODE_MATCH_INFO
#define DECODE_MATCH_INFO(...) DECODE_MATCH_INFO_NULL(__VA_ARGS__)

#undef DECODE_LEGACY_MATCH_INFO
#define DECODE_LEGACY_MATCH_INFO(...) DECODE_MATCH_INFO_NORMAL(__VA_ARGS__)

static const DecodeITableEntry decode_legacy_itable[XX_LAST_OPCODE] = {
#include "dectree_generated.h.inc"
};

#undef DECODE_OPINFO
#undef DECODE_MATCH_INFO
#undef DECODE_LEGACY_MATCH_INFO
#undef DECODE_END_TABLE
#undef INVALID
#undef TERMINAL
#undef SUBINSNS
#undef EXTSPACE
#undef TABLE_LINK
#undef DECODE_NEW_TABLE
#undef DECODE_SEPARATOR_BITS

void decode_init(void)
{
    decode_ext_init();
}

void decode_send_insn_to(Packet *packet, int start, int newloc)
{
    Insn tmpinsn;
    int direction;
    int i;
    if (start == newloc) {
        return;
    }
    if (start < newloc) {
        /* Move towards end */
        direction = 1;
    } else {
        /* move towards beginning */
        direction = -1;
    }
    for (i = start; i != newloc; i += direction) {
        tmpinsn = packet->insn[i];
        packet->insn[i] = packet->insn[i + direction];
        packet->insn[i + direction] = tmpinsn;
    }
}

/* Fill newvalue registers with the correct regno */
static void
decode_fill_newvalue_regno(Packet *packet)
{
    int i, use_regidx, offset, def_idx, dst_idx;
    uint16_t def_opcode, use_opcode;
    char *dststr;

    for (i = 1; i < packet->num_insns; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_DOTNEWVALUE) &&
            !GET_ATTRIB(packet->insn[i].opcode, A_EXTENSION)) {
            use_opcode = packet->insn[i].opcode;

            /* It's a store, so we're adjusting the Nt field */
            if (GET_ATTRIB(use_opcode, A_STORE)) {
                use_regidx = strchr(opcode_reginfo[use_opcode], 't') -
                    opcode_reginfo[use_opcode];
            } else {    /* It's a Jump, so we're adjusting the Ns field */
                use_regidx = strchr(opcode_reginfo[use_opcode], 's') -
                    opcode_reginfo[use_opcode];
            }

            /*
             * What's encoded at the N-field is the offset to who's producing
             * the value.  Shift off the LSB which indicates odd/even register,
             * then walk backwards and skip over the constant extenders.
             */
            offset = packet->insn[i].regno[use_regidx] >> 1;
            def_idx = i - offset;
            for (int j = 0; j < offset; j++) {
                if (GET_ATTRIB(packet->insn[i - j - 1].opcode, A_IT_EXTENDER)) {
                    def_idx--;
                }
            }

            /*
             * Check for a badly encoded N-field which points to an instruction
             * out-of-range
             */
            g_assert(!((def_idx < 0) || (def_idx > (packet->num_insns - 1))));

            /*
             * packet->insn[def_idx] is the producer
             * Figure out which type of destination it produces
             * and the corresponding index in the reginfo
             */
            def_opcode = packet->insn[def_idx].opcode;
            dststr = strstr(opcode_wregs[def_opcode], "Rd");
            if (dststr) {
                dststr = strchr(opcode_reginfo[def_opcode], 'd');
            } else {
                dststr = strstr(opcode_wregs[def_opcode], "Rx");
                if (dststr) {
                    dststr = strchr(opcode_reginfo[def_opcode], 'x');
                } else {
                    dststr = strstr(opcode_wregs[def_opcode], "Re");
                    if (dststr) {
                        dststr = strchr(opcode_reginfo[def_opcode], 'e');
                    } else {
                        dststr = strstr(opcode_wregs[def_opcode], "Ry");
                        if (dststr) {
                            dststr = strchr(opcode_reginfo[def_opcode], 'y');
                        } else {
                            g_assert_not_reached();
                        }
                    }
                }
            }
            g_assert(dststr != NULL);

            /* Now patch up the consumer with the register number */
            dst_idx = dststr - opcode_reginfo[def_opcode];
            packet->insn[i].regno[use_regidx] =
                packet->insn[def_idx].regno[dst_idx];
            /*
             * We need to remember who produces this value to later
             * check if it was dynamically cancelled
             */
            packet->insn[i].new_value_producer_slot =
                packet->insn[def_idx].slot;
        }
    }
}

/* Split CJ into a compare and a jump */
static void decode_split_cmpjump(Packet *pkt)
{
    int last, i;
    int numinsns = pkt->num_insns;

    /*
     * First, split all compare-jumps.
     * The compare is sent to the end as a new instruction.
     * Do it this way so we don't reorder dual jumps. Those need to stay in
     * original order.
     */
    for (i = 0; i < numinsns; i++) {
        /* It's a cmp-jump */
        if (GET_ATTRIB(pkt->insn[i].opcode, A_NEWCMPJUMP)) {
            last = pkt->num_insns;
            pkt->insn[last] = pkt->insn[i];    /* copy the instruction */
            pkt->insn[last].part1 = 1;    /* last instruction does the CMP */
            pkt->insn[i].part1 = 0;    /* existing instruction does the JUMP */
            pkt->num_insns++;
        }
    }

    /* Now re-shuffle all the compares back to the beginning */
    for (i = 0; i < pkt->num_insns; i++) {
        if (pkt->insn[i].part1) {
            decode_send_insn_to(pkt, i, 0);
        }
    }
}

static inline int decode_opcode_can_jump(int opcode)
{
    if ((GET_ATTRIB(opcode, A_JUMP)) ||
        (GET_ATTRIB(opcode, A_CALL)) ||
        (opcode == J2_trap0) ||
        (opcode == J2_trap1) ||
        (opcode == J2_rte) ||
        (opcode == J2_pause)) {
        /* Exception to A_JUMP attribute */
        if (opcode == J4_hintjumpr) {
            return 0;
        }
        return 1;
    }

    return 0;
}

static inline int decode_opcode_ends_loop(int opcode)
{
    return GET_ATTRIB(opcode, A_HWLOOP0_END) ||
           GET_ATTRIB(opcode, A_HWLOOP1_END);
}

/* Set the is_* fields in each instruction */
static void decode_set_insn_attr_fields(Packet *pkt)
{
    int i;
    int numinsns = pkt->num_insns;
    size2u_t opcode;
    int loads = 0;
    int stores = 0;
    int canjump;
    int total_slots_valid = 0;

    pkt->num_rops = 0;
    pkt->pkt_has_cof = 0;
    pkt->pkt_has_call = 0;
    pkt->pkt_has_jumpr = 0;
    pkt->pkt_has_cjump = 0;
    pkt->pkt_has_cjump_dotnew = 0;
    pkt->pkt_has_cjump_dotold = 0;
    pkt->pkt_has_cjump_newval = 0;
    pkt->pkt_has_endloop = 0;
    pkt->pkt_has_endloop0 = 0;
    pkt->pkt_has_endloop01 = 0;
    pkt->pkt_has_endloop1 = 0;
    pkt->pkt_has_cacheop = 0;
    pkt->memop_or_nvstore = 0;
    pkt->pkt_has_dczeroa = 0;
    pkt->pkt_has_dealloc_return = 0;
#ifndef CONFIG_USER_ONLY
    pkt->pkt_has_sys_visibility = 0;
#endif

    for (i = 0; i < numinsns; i++) {
        opcode = pkt->insn[i].opcode;
        if (pkt->insn[i].part1) {
            continue;    /* Skip compare of cmp-jumps */
        }

        if (GET_ATTRIB(opcode, A_ROPS_3)) {
            pkt->num_rops += 3;
        } else if (GET_ATTRIB(opcode, A_ROPS_2)) {
            pkt->num_rops += 2;
        } else {
            pkt->num_rops++;
        }
        if (pkt->insn[i].extension_valid) {
            pkt->num_rops += 2;
        }

        if (GET_ATTRIB(opcode, A_MEMOP) ||
            GET_ATTRIB(opcode, A_NVSTORE)) {
            pkt->memop_or_nvstore = 1;
        }

        if (GET_ATTRIB(opcode, A_CACHEOP)) {
            pkt->pkt_has_cacheop = 1;
            if (GET_ATTRIB(opcode, A_DCZEROA)) {
                pkt->pkt_has_dczeroa = 1;
            }
            if (GET_ATTRIB(opcode, A_ICTAGOP)) {
                pkt->pkt_has_ictagop = 1;
            }
            if (GET_ATTRIB(opcode, A_ICFLUSHOP)) {
                pkt->pkt_has_icflushop = 1;
            }
            if (GET_ATTRIB(opcode, A_DCTAGOP)) {
                pkt->pkt_has_dctagop = 1;
            }
            if (GET_ATTRIB(opcode, A_DCFLUSHOP)) {
                pkt->pkt_has_dcflushop = 1;
            }
            if (GET_ATTRIB(opcode, A_L2TAGOP)) {
                pkt->pkt_has_l2tagop = 1;
            }
            if (GET_ATTRIB(opcode, A_L2FLUSHOP)) {
                pkt->pkt_has_l2flushop = 1;
            }
        }

        if (GET_ATTRIB(opcode, A_DEALLOCRET)) {
            pkt->pkt_has_dealloc_return = 1;
        }

        if (GET_ATTRIB(opcode, A_STORE) && !GET_ATTRIB(opcode, A_LLSC)) {
            pkt->insn[i].is_store = 1;
            int skip = (GET_ATTRIB(opcode, A_STORE) &&
                       (GET_ATTRIB(opcode, A_MEMSIZE_0B) ||
                        GET_ATTRIB(opcode, A_VMEM) ||
                        GET_ATTRIB(opcode, A_VMEMU)));
            if (!skip) {
                if (pkt->insn[i].slot == 0) {
                    pkt->pkt_has_scalar_store_s0 = 1;
                } else {
                    pkt->pkt_has_scalar_store_s1 = 1;
                }
            }
        }
        if (GET_ATTRIB(opcode, A_DCFETCH)) {
            pkt->insn[i].is_dcfetch = 1;
        }
        if (GET_ATTRIB(opcode, A_LOAD)) {
            pkt->insn[i].is_load = 1;
            if (GET_ATTRIB(opcode, A_VMEM))  {
                pkt->insn[i].is_vmem_ld = 1;
            }

            if (pkt->insn[i].slot == 0) {
                pkt->pkt_has_load_s0 = 1;
            } else {
                pkt->pkt_has_load_s1 = 1;
            }
        }
        if (GET_ATTRIB(opcode, A_CVI_GATHER) ||
            GET_ATTRIB(opcode, A_CVI_SCATTER)) {
            pkt->insn[i].is_scatgath = 1;
        }
        if (GET_ATTRIB(opcode, A_MEMOP)) {
            pkt->insn[i].is_memop = 1;
        }
        if (GET_ATTRIB(opcode, A_DEALLOCRET) ||
            GET_ATTRIB(opcode, A_DEALLOCFRAME)) {
            pkt->insn[i].is_dealloc = 1;
        }
        if (GET_ATTRIB(opcode, A_DCFLUSHOP) ||
            GET_ATTRIB(opcode, A_DCTAGOP)) {
            pkt->insn[i].is_dcop = 1;
        }
#ifndef CONFIG_USER_ONLY
        uint32_t could_halt = 0;
        uint32_t triggers_int = 0;
        uint32_t is_solo = 0;
        uint32_t sys_reg_access = 0;
        if (opcode == Y2_tfrscrr ||
            opcode == Y2_tfrsrcr ||
            opcode == Y4_tfrscpp ||
            opcode == Y4_tfrspcp ||
            opcode == A2_tfrcrr ||
            opcode == A2_tfrrcr ||
            opcode == A4_tfrcpp ||
            opcode == A4_tfrpcp) {
            sys_reg_access = 1;
        }
        if (opcode == Y2_stop ||
            opcode == Y2_wait ||
            opcode == Y2_k0lock ||
            opcode == Y2_tlblock) {
            could_halt = 1;
        }
        if (opcode == J2_trap0 ||
            opcode == J2_trap1 ||
            opcode == Y4_nmi   ||
            GET_ATTRIB(opcode, A_EXCEPTION_TLB) ||
            GET_ATTRIB(opcode, A_EXCEPTION_SWI) ||
            GET_ATTRIB(opcode, A_EXCEPTION_ACCESS)) {
            triggers_int = 1;
        }
        if (GET_ATTRIB(opcode, A_RESTRICT_NOPACKET)) {
            is_solo = 1;
        }
#endif

        pkt->pkt_has_call |= GET_ATTRIB(opcode, A_CALL);
        pkt->pkt_has_jumpr |= GET_ATTRIB(opcode, A_INDIRECT) &&
                              !GET_ATTRIB(opcode, A_HINTJR);
        pkt->pkt_has_cjump |= GET_ATTRIB(opcode, A_CJUMP);
        pkt->pkt_has_cjump_dotnew |= GET_ATTRIB(opcode, A_DOTNEW) &&
                                     GET_ATTRIB(opcode, A_CJUMP);
        pkt->pkt_has_cjump_dotold |= GET_ATTRIB(opcode, A_DOTOLD) &&
                                     GET_ATTRIB(opcode, A_CJUMP);
        pkt->pkt_has_cjump_newval |= GET_ATTRIB(opcode, A_DOTNEWVALUE) &&
                                     GET_ATTRIB(opcode, A_CJUMP);

        canjump = decode_opcode_can_jump(opcode);

        if (pkt->pkt_has_cof) {
            if (canjump) {
                pkt->pkt_has_dual_jump = 1;
                pkt->insn[i].is_2nd_jump = 1;
            }
        } else {
            pkt->pkt_has_cof |= canjump;
        }

        pkt->insn[i].is_endloop = decode_opcode_ends_loop(opcode);

        pkt->pkt_has_endloop |= pkt->insn[i].is_endloop;
        pkt->pkt_has_endloop0 |= GET_ATTRIB(opcode, A_HWLOOP0_END) &&
                                 !GET_ATTRIB(opcode, A_HWLOOP1_END);
        pkt->pkt_has_endloop01 |= GET_ATTRIB(opcode, A_HWLOOP0_END) &&
                                  GET_ATTRIB(opcode, A_HWLOOP1_END);
        pkt->pkt_has_endloop1 |= GET_ATTRIB(opcode, A_HWLOOP1_END) &&
                                 !GET_ATTRIB(opcode, A_HWLOOP0_END);

        pkt->pkt_has_cof |= pkt->pkt_has_endloop;

        /* Now create slot valids */
        if (pkt->insn[i].is_endloop)    /* Don't count endloops */
            continue;

        switch (pkt->insn[i].slot) {
        case 0:
            pkt->slot0_valid = 1;
            break;
        case 1:
            pkt->slot1_valid = 1;
            break;
        case 2:
            pkt->slot2_valid = 1;
            break;
        case 3:
            pkt->slot3_valid = 1;
        break;
        }
        total_slots_valid++;

        /* And track #loads/stores */
        if (pkt->insn[i].is_store) {
            stores++;
        } else if (pkt->insn[i].is_load) {
            loads++;
        }

#ifndef CONFIG_USER_ONLY
        pkt->pkt_has_sys_visibility |=
               (is_solo && !(GET_ATTRIB(opcode, A_STORE) ||
                             GET_ATTRIB(opcode, A_LOAD) ||
                             GET_ATTRIB(opcode, A_CACHEOP)))
             || could_halt
             || triggers_int
             || sys_reg_access
             || GET_ATTRIB(opcode, A_IMPLICIT_WRITES_IPENDAD_IPEND)
             || GET_ATTRIB(opcode, A_IMPLICIT_WRITES_IPENDAD_IAD)
             || GET_ATTRIB(opcode, A_IMPLICIT_WRITES_SYSCFG_K0LOCK)
             || GET_ATTRIB(opcode, A_IMPLICIT_WRITES_SYSCFG_TLBLOCK);
#endif
        if (GET_ATTRIB(opcode, A_DCZEROA)) {
            pkt->pkt_has_dczeroa = 1;
        }


        pkt->pkt_has_cof |= decode_opcode_can_jump(opcode);

        pkt->insn[i].is_endloop = decode_opcode_ends_loop(opcode);

        pkt->pkt_has_endloop |= pkt->insn[i].is_endloop;

        pkt->pkt_has_cof |= pkt->pkt_has_endloop;
    }

    if (stores == 2) {
        pkt->dual_store = 1;
    } else if (loads == 2) {
        pkt->dual_load = 1;
    } else if ((loads == 1) && (stores == 1)) {
        pkt->load_and_store = 1;
    } else if (loads == 1) {
        pkt->single_load = 1;
    } else if (stores == 1) {
        pkt->single_store = 1;
    }

}

/*
 * Shuffle for execution
 * Move stores to end (in same order as encoding)
 * Move compares to beginning (for use by .new insns)
 */
static void decode_shuffle_for_execution(Packet *packet)
{
    int changed = 0;
    int i;
    int flag;    /* flag means we've seen a non-memory instruction */
    int n_mems;
    int last_insn = packet->num_insns - 1;

    /*
     * Skip end loops, somehow an end loop is getting in and messing
     * up the order
     */
    if (decode_opcode_ends_loop(packet->insn[last_insn].opcode)) {
        last_insn--;
    }

    do {
        changed = 0;
        /*
         * Stores go last, must not reorder.
         * Cannot shuffle stores past loads, either.
         * Iterate backwards.  If we see a non-memory instruction,
         * then a store, shuffle the store to the front.  Don't shuffle
         * stores wrt each other or a load.
         */
        for (flag = n_mems = 0, i = last_insn; i >= 0; i--) {
            int opcode = packet->insn[i].opcode;

            if (flag && GET_ATTRIB(opcode, A_STORE)) {
                decode_send_insn_to(packet, i, last_insn - n_mems);
                n_mems++;
                changed = 1;
            } else if (GET_ATTRIB(opcode, A_STORE)) {
                n_mems++;
            } else if (GET_ATTRIB(opcode, A_LOAD)) {
                /*
                 * Don't set flag, since we don't want to shuffle a
                 * store past a load
                 */
                n_mems++;
            } else if (GET_ATTRIB(opcode, A_DOTNEWVALUE)) {
                /*
                 * Don't set flag, since we don't want to shuffle past
                 * a .new value
                 */
            } else {
                flag = 1;
            }
        }

        if (changed) {
            continue;
        }
        /* Compares go first, may be reordered wrt each other */
        for (flag = 0, i = 0; i < last_insn + 1; i++) {
            int opcode = packet->insn[i].opcode;

            if ((strstr(opcode_wregs[opcode], "Pd4") ||
                 strstr(opcode_wregs[opcode], "Pe4")) &&
                GET_ATTRIB(opcode, A_STORE) == 0) {
                /* This should be a compare (not a store conditional) */
                if (flag) {
                    decode_send_insn_to(packet, i, 0);
                    changed = 1;
                    continue;
                }
            } else if (GET_ATTRIB(opcode, A_IMPLICIT_WRITES_P3) &&
                       !decode_opcode_ends_loop(packet->insn[i].opcode)) {
                /*
                 * spNloop instruction
                 * Don't reorder endloops; they are not valid for .new uses,
                 * and we want to match HW
                 */
                if (flag) {
                    decode_send_insn_to(packet, i, 0);
                    changed = 1;
                    continue;
                }
            } else if (GET_ATTRIB(opcode, A_IMPLICIT_WRITES_P0) &&
                       !GET_ATTRIB(opcode, A_NEWCMPJUMP)) {
                /* CABAC instruction */
                if (flag) {
                    decode_send_insn_to(packet, i, 0);
                    changed = 1;
                    continue;
                }
            } else {
                flag = 1;
            }
        }
        if (changed) {
            continue;
        }
    } while (changed);

    /*
     * If we have a .new register compare/branch, move that to the very
     * very end, past stores
     */
    for (i = 0; i < last_insn; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_DOTNEWVALUE)) {
            decode_send_insn_to(packet, i, last_insn);
            break;
        }
    }

    /*
     * And at the very very very end, move any RTE's, since they update
     * user/supervisor mode.
     */
#if !defined(CONFIG_USER_ONLY)
    for (i = 0; i < last_insn; i++) {
        if (packet->insn[i].opcode == J2_rte) {
            decode_send_insn_to(packet, i, last_insn);
            break;
        }
    }
#endif
}

#if 0
static void decode_assembler_count_fpops(Packet *pkt)
{
    int i;
    for (i = 0; i < pkt->num_insns; i++) {
        if (GET_ATTRIB(pkt->insn[i].opcode, A_FPOP)) {
            pkt->pkt_has_fp_op = 1;
        }
        if (GET_ATTRIB(pkt->insn[i].opcode, A_FPDOUBLE)) {
            pkt->pkt_has_fpdp_op = 1;
        } else if (GET_ATTRIB(pkt->insn[i].opcode, A_FPSINGLE)) {
            pkt->pkt_has_fpsp_op = 1;
        }
    }
}
#endif

static void
apply_extender(Packet *pkt, int i, uint32_t extender)
{
    int immed_num;
    uint32_t base_immed;

    immed_num = opcode_which_immediate_is_extended(pkt->insn[i].opcode);
    base_immed = pkt->insn[i].immed[immed_num];

    pkt->insn[i].immed[immed_num] = extender | fZXTN(6, 32, base_immed);
}

static void decode_apply_extenders(Packet *packet)
{
    int i;
    for (i = 0; i < packet->num_insns; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_IT_EXTENDER)) {
            packet->insn[i + 1].extension_valid = true;
            apply_extender(packet, i + 1, packet->insn[i].immed[0]);
        }
    }
}

static void decode_remove_extenders(Packet *packet)
{
    int i, j;
    for (i = 0; i < packet->num_insns; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_IT_EXTENDER)) {
            /* Remove this one by moving the remaining instructions down */
            for (j = i;
                (j < packet->num_insns - 1) && (j < INSTRUCTIONS_MAX - 1);
                j++) {
                packet->insn[j] = packet->insn[j + 1];
            }
            packet->num_insns--;
        }
    }
}

static SlotMask get_valid_slots(const Packet *pkt, unsigned int slot)
{
    if (GET_ATTRIB(pkt->insn[slot].opcode, A_EXTENSION)) {
        return mmvec_ext_decode_find_iclass_slots(pkt->insn[slot].opcode);
    } else {
        return find_iclass_slots(pkt->insn[slot].opcode,
                                 pkt->insn[slot].iclass);
    }
}


#if 0
static const char *
get_valid_slot_str(const Packet *pkt, unsigned int slot)

{
    const char *str;

    switch (get_valid_slots(pkt, slot)) {
        case SLOTS_0:
            str = "0";
            break;

        case SLOTS_1:
            str = "1";
            break;

        case SLOTS_01:
            str = "01";
            break;

        case SLOTS_2:
            str = "2";
            break;

        case SLOTS_3:
            str = "3";
            break;

        case SLOTS_23:
            str = "23";
            break;

        case SLOTS_0123:
            str = "0123";
            break;
    }
    return str;
}
#endif

#define DECODE_NEW_TABLE(TAG, SIZE, WHATNOT)     /* NOTHING */
#define TABLE_LINK(TABLE)                        /* NOTHING */
#define TERMINAL(TAG, ENC)                       /* NOTHING */
#define SUBINSNS(TAG, CLASSA, CLASSB, ENC)       /* NOTHING */
#define EXTSPACE(TAG, ENC)                       /* NOTHING */
#define INVALID()                                /* NOTHING */
#define DECODE_END_TABLE(...)                    /* NOTHING */
#define DECODE_MATCH_INFO(...)                   /* NOTHING */
#define DECODE_LEGACY_MATCH_INFO(...)            /* NOTHING */

#define DECODE_REG(REGNO, WIDTH, STARTBIT) \
    insn->regno[REGNO] = ((encoding >> STARTBIT) & ((1 << WIDTH) - 1));

#define DECODE_IMPL_REG(REGNO, VAL) \
    insn->regno[REGNO] = VAL;

#define DECODE_IMM(IMMNO, WIDTH, STARTBIT, VALSTART) \
    insn->immed[IMMNO] |= (((encoding >> STARTBIT) & ((1 << WIDTH) - 1))) << \
                          (VALSTART);

#define DECODE_IMM_SXT(IMMNO, WIDTH) \
    insn->immed[IMMNO] = ((((int32_t)insn->immed[IMMNO]) << (32 - WIDTH)) >> \
                          (32 - WIDTH));

#define DECODE_IMM_NEG(IMMNO, WIDTH) \
    insn->immed[IMMNO] = -insn->immed[IMMNO];

#define DECODE_IMM_SHIFT(IMMNO, SHAMT)                                 \
    if ((!insn->extension_valid) || \
        (insn->which_extended != IMMNO)) { \
        insn->immed[IMMNO] <<= SHAMT; \
    }

#define DECODE_OPINFO(TAG, BEH) \
    case TAG: \
        { BEH  } \
        break; \

/*
 * Fill in the operands of the instruction
 * dectree_generated.h.inc has a DECODE_OPINFO entry for each opcode
 * For example,
 *     DECODE_OPINFO(A2_addi,
 *          DECODE_REG(0,5,0)
 *          DECODE_REG(1,5,16)
 *          DECODE_IMM(0,7,21,9)
 *          DECODE_IMM(0,9,5,0)
 *          DECODE_IMM_SXT(0,16)
 * with the macros defined above, we'll fill in a switch statement
 * where each case is an opcode tag.
 */
static void
decode_op(Insn *insn, Opcode tag, uint32_t encoding)
{
    insn->immed[0] = 0;
    insn->immed[1] = 0;
    insn->opcode = tag;
    if (insn->extension_valid) {
        insn->which_extended = opcode_which_immediate_is_extended(tag);
    }

    switch (tag) {
#include "dectree_generated.h.inc"
    default:
        break;
    }

    insn->generate = opcode_genptr[tag];

    insn->iclass = iclass_bits(encoding);
}

#undef DECODE_REG
#undef DECODE_IMPL_REG
#undef DECODE_IMM
#undef DECODE_IMM_SHIFT
#undef DECODE_OPINFO
#undef DECODE_MATCH_INFO
#undef DECODE_LEGACY_MATCH_INFO
#undef DECODE_END_TABLE
#undef INVALID
#undef TERMINAL
#undef SUBINSNS
#undef EXTSPACE
#undef TABLE_LINK
#undef DECODE_NEW_TABLE
#undef DECODE_SEPARATOR_BITS

static unsigned int
decode_subinsn_tablewalk(Insn *insn, const DectreeTable *table,
                         uint32_t encoding)
{
    unsigned int i;
    Opcode opc;
    if (table->lookup_function) {
        i = table->lookup_function(table->startbit, table->width, encoding);
    } else {
        i = extract32(encoding, table->startbit, table->width);
    }
    if (table->table[i].type == DECTREE_TABLE_LINK) {
        return decode_subinsn_tablewalk(insn, table->table[i].table_link,
                                        encoding);
    } else if (table->table[i].type == DECTREE_TERMINAL) {
        opc = table->table[i].opcode;
        if ((encoding & decode_itable[opc].mask) != decode_itable[opc].match) {
            return 0;
        }
        decode_op(insn, opc, encoding);
        return 1;
    } else {
        return 0;
    }
}

static unsigned int get_insn_a(uint32_t encoding)
{
    return extract32(encoding, 0, 13);
}

static unsigned int get_insn_b(uint32_t encoding)
{
    return extract32(encoding, 16, 13);
}

static unsigned int
decode_insns_tablewalk(Insn *insn, const DectreeTable *table,
                       uint32_t encoding)
{
    unsigned int i;
    unsigned int a, b;
    Opcode opc;
    if (table->lookup_function) {
        i = table->lookup_function(table->startbit, table->width, encoding);
    } else {
        i = extract32(encoding, table->startbit, table->width);
    }
    if (table->table[i].type == DECTREE_TABLE_LINK) {
        return decode_insns_tablewalk(insn, table->table[i].table_link,
                                      encoding);
    } else if (table->table[i].type == DECTREE_SUBINSNS) {
        a = get_insn_a(encoding);
        b = get_insn_b(encoding);
        b = decode_subinsn_tablewalk(insn, table->table[i].table_link_b, b);
        a = decode_subinsn_tablewalk(insn + 1, table->table[i].table_link, a);
        if ((a == 0) || (b == 0)) {
            return 0;
        }
        return 2;
    } else if (table->table[i].type == DECTREE_TERMINAL) {
        opc = table->table[i].opcode;
        if ((encoding & decode_itable[opc].mask) != decode_itable[opc].match) {
            if ((encoding & decode_legacy_itable[opc].mask) !=
                decode_legacy_itable[opc].match) {
                return 0;
            }
        }
        decode_op(insn, opc, encoding);
        return 1;
    } else if (table->table[i].type == DECTREE_EXTSPACE) {
        size4u_t active_ext;
        /*
         * For now, HVX will be the only coproc
         */
        active_ext = EXT_IDX_mmvec;
        return decode_insns_tablewalk(insn, ext_trees[active_ext], encoding);
    } else {
        return 0;
    }
}

static unsigned int
decode_insns(Insn *insn, uint32_t encoding)
{
    const DectreeTable *table;
    if (parse_bits(encoding) != 0) {
        /* Start with PP table - 32 bit instructions */
        table = &dectree_table_DECODE_ROOT_32;
    } else {
        /* start with EE table - duplex instructions */
        table = &dectree_table_DECODE_ROOT_EE;
    }
    return decode_insns_tablewalk(insn, table, encoding);
}

static void decode_add_endloop_insn(Insn *insn, int loopnum)
{
    if (loopnum == 10) {
        insn->opcode = J2_endloop01;
        insn->generate = opcode_genptr[J2_endloop01];
    } else if (loopnum == 1) {
        insn->opcode = J2_endloop1;
        insn->generate = opcode_genptr[J2_endloop1];
    } else if (loopnum == 0) {
        insn->opcode = J2_endloop0;
        insn->generate = opcode_genptr[J2_endloop0];
    } else {
        g_assert_not_reached();
    }
}

static inline int decode_parsebits_is_loopend(uint32_t encoding32)
{
    uint32_t bits = parse_bits(encoding32);
    return bits == 0x2;
}

static void
decode_set_slot_number(Packet *pkt)
{
    int slot;
    int i;
    int hit_mem_insn = 0;
    int hit_duplex = 0;

    /*
     * The slots are encoded in reverse order
     * For each instruction, count down until you find a suitable slot
     */
    for (i = 0, slot = 3; i < pkt->num_insns; i++) {
        SlotMask valid_slots = get_valid_slots(pkt, i);

        while (!(valid_slots & (1 << slot))) {
            slot--;
        }
        pkt->insn[i].slot = slot;
        if (slot) {
            /* I've assigned the slot, now decrement it for the next insn */
            slot--;
        }
    }

    /* Fix the exceptions - mem insns to slot 0,1 */
    for (i = pkt->num_insns - 1; i >= 0; i--) {
        /* First memory instruction always goes to slot 0 */
        if ((GET_ATTRIB(pkt->insn[i].opcode, A_MEMLIKE) ||
             GET_ATTRIB(pkt->insn[i].opcode, A_MEMLIKE_PACKET_RULES)) &&
            !hit_mem_insn) {
            hit_mem_insn = 1;
            pkt->insn[i].slot = 0;
            continue;
        }

        /* Next memory instruction always goes to slot 1 */
        if ((GET_ATTRIB(pkt->insn[i].opcode, A_MEMLIKE) ||
             GET_ATTRIB(pkt->insn[i].opcode, A_MEMLIKE_PACKET_RULES)) &&
            hit_mem_insn) {
            pkt->insn[i].slot = 1;
        }
    }

    /* Fix the exceptions - duplex always slot 0,1 */
    for (i = pkt->num_insns - 1; i >= 0; i--) {
        /* First subinsn always goes to slot 0 */
        if (GET_ATTRIB(pkt->insn[i].opcode, A_SUBINSN) && !hit_duplex) {
            hit_duplex = 1;
            pkt->insn[i].slot = 0;
            continue;
        }

        /* Next subinsn always goes to slot 1 */
        if (GET_ATTRIB(pkt->insn[i].opcode, A_SUBINSN) && hit_duplex) {
            pkt->insn[i].slot = 1;
        }
    }

    /* Fix the exceptions - slot 1 is never empty, always aligns to slot 0 */
    int slot0_found = 0;
    int slot1_found = 0;
    int slot1_iidx = 0;
    for (i = pkt->num_insns - 1; i >= 0; i--) {
        /* Is slot0 used? */
        if (pkt->insn[i].slot == 0) {
            int is_endloop = (pkt->insn[i].opcode == J2_endloop01);
            is_endloop |= (pkt->insn[i].opcode == J2_endloop0);
            is_endloop |= (pkt->insn[i].opcode == J2_endloop1);

            /*
             * Make sure it's not endloop since, we're overloading
             * slot0 for endloop
             */
            if (!is_endloop) {
                slot0_found = 1;
            }
        }
        /* Is slot1 used? */
        if (pkt->insn[i].slot == 1) {
            slot1_found = 1;
            slot1_iidx = i;
        }
    }
    /* Is slot0 empty and slot1 used? */
    if ((slot0_found == 0) && (slot1_found == 1)) {
        /* Then push it to slot0 */
        pkt->insn[slot1_iidx].slot = 0;
    }
}

/*
 * decode_packet
 * Decodes packet with given words
 * Returns 0 on insufficient words,
 * or number of words used on success
 */

int decode_packet(int max_words, const uint32_t *words, Packet *pkt,
                  bool disas_only)
{
    int num_insns = 0;
    int words_read = 0;
    int end_of_packet = 0;
    int new_insns = 0;
    uint32_t encoding32;

    /* Initialize */
    memset(pkt, 0, sizeof(*pkt));
    /* Try to build packet */
    while (!end_of_packet && (words_read < max_words)) {
        encoding32 = words[words_read];
        end_of_packet = is_packet_end(encoding32);
        new_insns = decode_insns(&pkt->insn[num_insns], encoding32);
        g_assert(new_insns > 0);
        /*
         * If we saw an extender, mark next word extended so immediate
         * decode works
         */
        if (pkt->insn[num_insns].opcode == A4_ext) {
            pkt->insn[num_insns + 1].extension_valid = 1;
        }
        num_insns += new_insns;
        words_read++;
    }

    pkt->num_insns = num_insns;
    if (!end_of_packet) {
        /* Ran out of words! */
        return 0;
    }
    pkt->encod_pkt_size_in_bytes = words_read * 4;

    /*
     * Check for :endloop in the parse bits
     * Section 10.6 of the Programmer's Reference describes the encoding
     *     The end of hardware loop 0 can be encoded with 2 words
     *     The end of hardware loop 1 needs 3 words
     */
    if ((words_read == 2) && (decode_parsebits_is_loopend(words[0]))) {
        decode_add_endloop_insn(&pkt->insn[pkt->num_insns++], 0);
    }
    if (words_read >= 3) {
        uint32_t has_loop0, has_loop1;
        has_loop0 = decode_parsebits_is_loopend(words[0]);
        has_loop1 = decode_parsebits_is_loopend(words[1]);
        if (has_loop0 && has_loop1) {
            decode_add_endloop_insn(&pkt->insn[pkt->num_insns++], 10);
        } else if (has_loop1) {
            decode_add_endloop_insn(&pkt->insn[pkt->num_insns++], 1);
        } else if (has_loop0) {
            decode_add_endloop_insn(&pkt->insn[pkt->num_insns++], 0);
        }
    }

    decode_apply_extenders(pkt);
    if (!disas_only) {
        decode_remove_extenders(pkt);
    }
    decode_set_slot_number(pkt);
    decode_fill_newvalue_regno(pkt);

    if (!disas_only) {
        decode_shuffle_for_execution(pkt);
        decode_split_cmpjump(pkt);
        decode_set_insn_attr_fields(pkt);
    }

    return words_read;
}

/* Used for "-d in_asm" logging */
int disassemble_hexagon(uint32_t *words, int nwords, bfd_vma pc,
                        GString *buf)
{
    Packet pkt;

    if (decode_packet(nwords, words, &pkt, true) > 0) {
        snprint_a_pkt_disas(buf, &pkt, words, pc);
        return pkt.encod_pkt_size_in_bytes;
    } else {
        g_string_assign(buf, "<invalid>");
        return 0;
    }
}
