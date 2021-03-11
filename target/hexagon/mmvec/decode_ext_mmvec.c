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
#include "decode.h"
#include "opcodes.h"
#include "insn.h"
#include "iclass.h"
#include "printinsn.h"
#include "mmvec/mmvec.h"
#include "mmvec/decode_ext_mmvec.h"

typedef enum {
    HVX_RESOURCE_LOAD    = 0,
    HVX_RESOURCE_STORE   = 1,
    HVX_RESOURCE_PERM    = 2,
    HVX_RESOURCE_SHIFT   = 3,
    HVX_RESOURCE_MPY0    = 4,
    HVX_RESOURCE_MPY1    = 5,
    HVX_RESOURCE_ZR      = 6,
    HVX_RESOURCE_ZW      = 7
} HVXResource;

#define FREE    1
#define USED    0

static void
check_dv_instruction(HVXResource *resources, int *ilist,
                     int num_insn, Packet *packet, unsigned int attribute,
                     HVXResource resource0, HVXResource resource1)
{
    int current_insn = 0;
    /* Loop on vector instruction count */
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        /* valid instruction */
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            if (GET_ATTRIB(opcode, attribute)) {
                /* Needs two available resources */
                if ((resources[resource0] + resources[resource1]) == 2 * FREE) {
                    resources[resource0] = USED;
                    resources[resource1] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource0);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource1);
                } else {
                    g_assert_not_reached();
                }
                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }
    }
}

/* Double Vector instructions that can use any one of specific or both pairs */
static void
check_dv_instruction2(HVXResource *resources, int *ilist,
                      int num_insn, Packet *packet, unsigned int attribute,
                      HVXResource resource0, HVXResource resource1,
                      HVXResource resource2, HVXResource resource3)
{
    int current_insn = 0;
    /* Loop on vector instruction count */
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        /* valid instruction */
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            if (GET_ATTRIB(opcode, attribute)) {
                /* Needs two available resources */
                if ((resources[resource0] + resources[resource1]) == 2 * FREE) {
                    resources[resource0] = USED;
                    resources[resource1] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                       (1 << resource0);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                       (1 << resource1);
                } else if ((resources[resource2] + resources[resource3]) ==
                           2 * FREE) {
                    resources[resource2] = USED;
                    resources[resource3] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource0);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource1);
                } else {
                    g_assert_not_reached();
                }
                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }
    }
}

static void
check_umem_instruction(HVXResource *resources, int *ilist,
                       int num_insn, Packet *packet)
{
    int current_insn = 0;
    /* Loop on vector instruction count */
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        /* valid instruction */
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            /* check attribute */
            if (GET_ATTRIB(opcode, A_CVI_VP) && GET_ATTRIB(opcode, A_CVI_VM)) {
                /* Needs three available resources, both mem and permute */
                if ((resources[HVX_RESOURCE_LOAD] +
                     resources[HVX_RESOURCE_STORE] +
                     resources[HVX_RESOURCE_PERM]) == 3 * FREE) {
                    resources[HVX_RESOURCE_LOAD] = USED;
                    resources[HVX_RESOURCE_STORE] = USED;
                    resources[HVX_RESOURCE_PERM] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << HVX_RESOURCE_LOAD);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << HVX_RESOURCE_STORE);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << HVX_RESOURCE_PERM);
                } else {
                    g_assert_not_reached();
                }

                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }
    }
}


/* Memory instructions */
static void
check_mem_instruction(HVXResource *resources, int *ilist,
                      int num_insn, Packet *packet)
{
    int current_insn = 0;
    /* Loop on vector instruction count */
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        /* valid instruction */
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            if (GET_ATTRIB(opcode, A_CVI_VM)) {
                if (GET_ATTRIB(opcode, A_LOAD)) {
                    if (resources[HVX_RESOURCE_LOAD] == FREE) {
                        resources[HVX_RESOURCE_LOAD] = USED;
                        packet->insn[ilist[current_insn]].hvx_resource |=
                            (1 << HVX_RESOURCE_LOAD);
                    } else {
                        g_assert_not_reached();
                    }

                } else if (GET_ATTRIB(opcode, A_STORE)) {
                    if (resources[HVX_RESOURCE_STORE] == FREE) {
                        resources[HVX_RESOURCE_STORE] = USED;
                        packet->insn[ilist[current_insn]].hvx_resource |=
                            (1 << HVX_RESOURCE_STORE);
                    } else {
                        g_assert_not_reached();
                    }
                } else {
                    g_assert_not_reached();
                }

                /* Not a load temp and not a store new */
                if (!(GET_ATTRIB(opcode, A_CVI_TMP) &&
                      GET_ATTRIB(opcode, A_LOAD)) &&
                    !(GET_ATTRIB(opcode, A_CVI_NEW) &&
                      GET_ATTRIB(opcode, A_STORE))) {
                    /* Grab any one of the other resources */
                    if (resources[HVX_RESOURCE_PERM] == FREE) {
                        resources[HVX_RESOURCE_PERM] = USED;
                        packet->insn[ilist[current_insn]].hvx_resource |=
                            (1 << HVX_RESOURCE_PERM);
                    } else if (resources[HVX_RESOURCE_SHIFT] == FREE) {
                        resources[HVX_RESOURCE_SHIFT] = USED;
                        packet->insn[ilist[current_insn]].hvx_resource |=
                            (1 << HVX_RESOURCE_SHIFT);
                    } else if (resources[HVX_RESOURCE_MPY0] == FREE) {
                        resources[HVX_RESOURCE_MPY0] = USED;
                        packet->insn[ilist[current_insn]].hvx_resource |=
                            (1 << HVX_RESOURCE_MPY0);
                    } else if (resources[HVX_RESOURCE_MPY1] == FREE) {
                        resources[HVX_RESOURCE_MPY1] = USED;
                        packet->insn[ilist[current_insn]].hvx_resource |=
                            (1 << HVX_RESOURCE_MPY1);
                    } else {
                        g_assert_not_reached();
                    }
                }
                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }
    }
}

/*
 * Single Vector instructions that can use one, two, or four resources
 * Insert instruction into one possible resource
 */
static void
check_instruction1(HVXResource *resources, int *ilist,
                   int num_insn, Packet *packet, unsigned int attribute,
                   HVXResource resource0)
{
    int current_insn = 0;
    /* Loop on vector instruction count */
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        /* valid instruction */
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            if (GET_ATTRIB(opcode, attribute)) {
                /* Needs two available resources */
                if (resources[resource0] == FREE) {
                    resources[resource0] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource0);
                } else {
                    g_assert_not_reached();
                }

                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }
    }
}

/* Insert instruction into one of two possible resource2 */
static void
check_instruction2(HVXResource *resources, int *ilist,
                   int num_insn, Packet *packet, unsigned int attribute,
                   HVXResource resource0, HVXResource resource1)
{
    int current_insn = 0;
    /* Loop on vector instruction count */
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        /* valid instruction */
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            if (GET_ATTRIB(opcode, attribute)) {
                /* Needs one of two possible available resources */
                if (resources[resource0] == FREE) {
                    resources[resource0] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource0);
                } else if (resources[resource1] == FREE)  {
                    resources[resource1] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource1);
                } else {
                    g_assert_not_reached();
                }

                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }
    }
}

/* Insert instruction into one of 4 four possible resource */
static void
check_instruction4(HVXResource *resources, int *ilist,
                   int num_insn, Packet *packet, unsigned int attribute,
                   HVXResource resource0, HVXResource resource1,
                   HVXResource resource2, HVXResource resource3)
{
    int current_insn = 0;
    /* Loop on vector instruction count */
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        /* valid instruction */
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            if (GET_ATTRIB(opcode, attribute)) {
                /* Needs one of four available resources */
                if (resources[resource0] == FREE) {
                    resources[resource0] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource0);
                } else if (resources[resource1] == FREE) {
                    resources[resource1] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource1);
                } else if (resources[resource2] == FREE) {
                    resources[resource2] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource2);
                } else if (resources[resource3] == FREE) {
                    resources[resource3] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << resource3);
                } else {
                    g_assert_not_reached();
                }

                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }
    }
}

static void
check_4res_instruction(HVXResource *resources, int *ilist,
                       int num_insn, Packet *packet)
{
    int current_insn = 0;
    for (current_insn = 0; current_insn < num_insn; current_insn++) {
        if (ilist[current_insn] > -1) {
            int opcode = packet->insn[ilist[current_insn]].opcode;
            if (GET_ATTRIB(opcode, A_CVI_4SLOT)) {
                int available_resource =
                        resources[HVX_RESOURCE_PERM]
                        + resources[HVX_RESOURCE_SHIFT]
                        + resources[HVX_RESOURCE_MPY0]
                        + resources[HVX_RESOURCE_MPY1];

                if (available_resource == 4 * FREE) {
                    resources[HVX_RESOURCE_PERM] = USED;
                    resources[HVX_RESOURCE_SHIFT] = USED;
                    resources[HVX_RESOURCE_MPY0] = USED;
                    resources[HVX_RESOURCE_MPY1] = USED;
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << HVX_RESOURCE_PERM);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << HVX_RESOURCE_SHIFT);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << HVX_RESOURCE_MPY0);
                    packet->insn[ilist[current_insn]].hvx_resource |=
                        (1 << HVX_RESOURCE_MPY1);
                } else {
                    g_assert_not_reached();
                }

                ilist[current_insn] = -1;     /* Remove Instruction */
            }
        }

    }
}


static void
decode_populate_cvi_resources(Packet *packet)
{
    int i, num_insn = 0;
    int vlist[4] = {-1, -1, -1, -1};
    HVXResource hvx_resources[8] = {
        FREE,
        FREE,
        FREE,
        FREE,
        FREE,
        FREE,
        FREE,
        FREE
    };    /* All Available */


    /* Count Vector instructions and check for deprecated ones */
    for (num_insn = 0, i = 0; i < packet->num_insns; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_CVI)) {
            vlist[num_insn++] = i;
        }
    }

    /* Instructions that consume all vector resources */
    check_4res_instruction(hvx_resources, vlist, num_insn, packet);
    /* Insert Unaligned Memory Access */
    check_umem_instruction(hvx_resources, vlist, num_insn, packet);

    /* double vector permute Consumes both permute and shift resources */
    check_dv_instruction(hvx_resources, vlist, num_insn,
                         packet, A_CVI_VP_VS,
                         HVX_RESOURCE_SHIFT, HVX_RESOURCE_PERM);
    /* Single vector permute can only go to permute resource */
    check_instruction1(hvx_resources, vlist, num_insn,
                       packet, A_CVI_VP, HVX_RESOURCE_PERM);
    /* Single vector permute can only go to permute resource */
    check_instruction1(hvx_resources, vlist, num_insn,
                       packet, A_CVI_VS, HVX_RESOURCE_SHIFT);

    /* Try to insert double vector multiply */
    check_dv_instruction(hvx_resources, vlist, num_insn,
                         packet, A_CVI_VX_DV,
                         HVX_RESOURCE_MPY0, HVX_RESOURCE_MPY1);
    /* Try to insert double capacity mult */
    check_dv_instruction2(hvx_resources, vlist, num_insn,
                          packet, A_CVI_VS_VX,
                          HVX_RESOURCE_SHIFT, HVX_RESOURCE_MPY0,
                          HVX_RESOURCE_PERM, HVX_RESOURCE_MPY1);
    /* Single vector permute can only go to permute resource */
    check_instruction2(hvx_resources, vlist, num_insn,
                       packet, A_CVI_VX,
                       HVX_RESOURCE_MPY0, HVX_RESOURCE_MPY1);
    /* Try to insert double vector alu */
    check_dv_instruction2(hvx_resources, vlist, num_insn,
                          packet, A_CVI_VA_DV,
                          HVX_RESOURCE_SHIFT, HVX_RESOURCE_PERM,
                          HVX_RESOURCE_MPY0, HVX_RESOURCE_MPY1);

    check_mem_instruction(hvx_resources, vlist, num_insn, packet);
    /* single vector alu can go on any of the 4 pipes */
    check_instruction4(hvx_resources, vlist, num_insn,
                       packet, A_CVI_VA,
                       HVX_RESOURCE_SHIFT, HVX_RESOURCE_PERM,
                       HVX_RESOURCE_MPY0, HVX_RESOURCE_MPY1);

}

static void
check_new_value(Packet *packet)
{
    /* .New Value for a MMVector Store */
    int i, j;
    const char *reginfo;
    const char *destletters;
    const char *dststr = NULL;
    uint16_t def_opcode;
    char letter;
    int def_regnum;

    for (i = 1; i < packet->num_insns; i++) {
        uint16_t use_opcode = packet->insn[i].opcode;
        if (GET_ATTRIB(use_opcode, A_DOTNEWVALUE) &&
            GET_ATTRIB(use_opcode, A_CVI) &&
            GET_ATTRIB(use_opcode, A_STORE)) {
            int use_regidx = strchr(opcode_reginfo[use_opcode], 's') -
                opcode_reginfo[use_opcode];
            /*
             * What's encoded at the N-field is the offset to who's producing
             * the value.
             * Shift off the LSB which indicates odd/even register.
             */
            int def_off = ((packet->insn[i].regno[use_regidx]) >> 1);
            int def_oreg = packet->insn[i].regno[use_regidx] & 1;
            int def_idx = -1;
            for (j = i - 1; (j >= 0) && (def_off >= 0); j--) {
                if (!GET_ATTRIB(packet->insn[j].opcode, A_CVI)) {
                    continue;
                }
                def_off--;
                if (def_off == 0) {
                    def_idx = j;
                    break;
                }
            }
            /*
             * Check for a badly encoded N-field which points to an instruction
             * out-of-range
             */
            if ((def_off != 0) || (def_idx < 0) ||
                (def_idx > (packet->num_insns - 1))) {
                g_assert_not_reached();
            }
            /* previous insn is the producer */
            def_opcode = packet->insn[def_idx].opcode;
            reginfo = opcode_reginfo[def_opcode];
            destletters = "dexy";
            for (j = 0; (letter = destletters[j]) != 0; j++) {
                dststr = strchr(reginfo, letter);
                if (dststr != NULL) {
                    break;
                }
            }
            if ((dststr == NULL)  && GET_ATTRIB(def_opcode, A_CVI_GATHER)) {
                def_regnum = 0;
                packet->insn[i].regno[use_regidx] = def_oreg;
                packet->insn[i].new_value_producer_slot =
                    packet->insn[def_idx].slot;
            } else {
                if (dststr == NULL) {
                    /* still not there, we have a bad packet */
                    g_assert_not_reached();
                }
                def_regnum = packet->insn[def_idx].regno[dststr - reginfo];
                /* Now patch up the consumer with the register number */
                packet->insn[i].regno[use_regidx] = def_regnum ^ def_oreg;
                /* special case for (Vx,Vy) */
                dststr = strchr(reginfo, 'y');
                if (def_oreg && strchr(reginfo, 'x') && dststr) {
                    def_regnum = packet->insn[def_idx].regno[dststr - reginfo];
                    packet->insn[i].regno[use_regidx] = def_regnum;
                }
                /*
                 * We need to remember who produces this value to later
                 * check if it was dynamically cancelled
                 */
                packet->insn[i].new_value_producer_slot =
                    packet->insn[def_idx].slot;
            }
        }
    }
}

/*
 * We don't want to reorder slot1/slot0 with respect to each other.
 * So in our shuffling, we don't want to move the .cur / .tmp vmem earlier
 * Instead, we should move the producing instruction later
 * But the producing instruction might feed a .new store!
 * So we may need to move that even later.
 */

static void
decode_mmvec_move_cvi_to_end(Packet *packet, int max)
{
    int i;
    for (i = 0; i < max; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_CVI)) {
            int last_inst = packet->num_insns - 1;

            /*
             * If the last instruction is an endloop, move to the one before it
             * Keep endloop as the last thing always
             */
            if ((packet->insn[last_inst].opcode == J2_endloop0) ||
                (packet->insn[last_inst].opcode == J2_endloop1) ||
                (packet->insn[last_inst].opcode == J2_endloop01)) {
                last_inst--;
            }

            decode_send_insn_to(packet, i, last_inst);
            max--;
            i--;    /* Retry this index now that packet has rotated */
        }
    }
}

static void
decode_shuffle_for_execution_vops(Packet *packet)
{
    /*
     * Sort for V.new = VMEM()
     * Right now we need to make sure that the vload occurs before the
     * permute instruction or VPVX ops
     */
    int i;
    for (i = 0; i < packet->num_insns; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_LOAD) &&
            (GET_ATTRIB(packet->insn[i].opcode, A_CVI_NEW) ||
             GET_ATTRIB(packet->insn[i].opcode, A_CVI_TMP))) {
            /*
             * Find prior consuming vector instructions
             * Move to end of packet
             */
            decode_mmvec_move_cvi_to_end(packet, i);
            break;
        }
    }
    for (i = 0; i < packet->num_insns - 1; i++) {
        if (GET_ATTRIB(packet->insn[i].opcode, A_STORE) &&
            GET_ATTRIB(packet->insn[i].opcode, A_CVI_NEW) &&
            !GET_ATTRIB(packet->insn[i].opcode, A_CVI_SCATTER_RELEASE)) {
            /* decode_send_insn_to the end */
            int last_inst = packet->num_insns - 1;

            /*
             * If the last instruction is an endloop, move to the one before it
             * Keep endloop as the last thing always
             */
            if ((packet->insn[last_inst].opcode == J2_endloop0) ||
                (packet->insn[last_inst].opcode == J2_endloop1) ||
                (packet->insn[last_inst].opcode == J2_endloop01)) {
                last_inst--;
            }

            decode_send_insn_to(packet, i, last_inst);
            break;
        }
    }
}

/* Collect stats on HVX packet */
static void
decode_hvx_packet_contents(Packet *pkt)
{
    for (int i = 0; i < pkt->num_insns; i++) {
        pkt->pkt_has_hvx |= pkt->insn[i].hvx_resource ? 1 : 0;
    }
}

/*
 * Public Functions
 */

SlotMask mmvec_ext_decode_find_iclass_slots(int opcode)
{
    if (GET_ATTRIB(opcode, A_CVI_VM)) {
        if (GET_ATTRIB(opcode, A_RESTRICT_SLOT0ONLY)) {
            return SLOTS_0;
        } else if (GET_ATTRIB(opcode, A_RESTRICT_SLOT1ONLY)) {
            return SLOTS_1;
        }
        return SLOTS_01;
    } else if (GET_ATTRIB(opcode, A_RESTRICT_SLOT2ONLY)) {
        return SLOTS_2;
    } else if (GET_ATTRIB(opcode, A_CVI_VX)) {
        return SLOTS_23;
    } else if (GET_ATTRIB(opcode, A_CVI_VS_VX)) {
        return SLOTS_23;
    } else {
        return SLOTS_0123;
    }
}

void mmvec_ext_decode_checks(Packet *packet)
{
    check_new_value(packet);
    decode_populate_cvi_resources(packet);
    decode_shuffle_for_execution_vops(packet);
    decode_hvx_packet_contents(packet);
}
