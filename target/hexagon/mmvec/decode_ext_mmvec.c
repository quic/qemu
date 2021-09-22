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
#include "mmvec/mmvec.h"
#include "mmvec/decode_ext_mmvec.h"

typedef enum hvx_resource {
    HVX_RESOURCE_LOAD    = 0,
    HVX_RESOURCE_STORE   = 1,
    HVX_RESOURCE_PERM    = 2,
    HVX_RESOURCE_SHIFT   = 3,
    HVX_RESOURCE_MPY0    = 4,
    HVX_RESOURCE_MPY1    = 5,
    HVX_RESOURCE_ZR      = 6,
    HVX_RESOURCE_ZW      = 7
} hvx_resource_t;


#include "mmvec.h"
#define THREAD2STRUCT ((hmx_state_t*)thread->processor_ptr->shared_extptr)

void decode_check_vmemu_and_scalar_memory_ops(thread_t *thread, Packet * packet,exception_info *einfo);

#if 0
int opcode_vreg_rd_count[] = {
#define VREGCOUNT(TAG,RD_COUNT,WR_COUNT) RD_COUNT,
#define IMMINFO(TAG,SIGN,SIZE,SHAMT,SIGN2,SIZE2,SHAMT2)	/* nothing */
#define REGINFO(TAG,REGINFO,RREGS,WREGS)
#include "op_regs.odef"
	0
#undef REGINFO
#undef IMMINFO
#undef VREGCOUNT
};

#if 0
int opcode_vreg_wr_count[] = {
#define VREGCOUNT(TAG,RD_COUNT,WR_COUNT) WR_COUNT,
#define IMMINFO(TAG,SIGN,SIZE,SHAMT,SIGN2,SIZE2,SHAMT2)	/* nothing */
#define REGINFO(TAG,REGINFO,RREGS,WREGS)
#include "op_regs.odef"
	0
#undef REGINFO
#undef IMMINFO
#undef VREGCOUNT
};
#endif
#endif

static int
handle_bad_packet(
		thread_t *thread,
		exception_info * einfo,
		const char *reason
) {
	//thread->exception_msg = "Bad HVX Decode";
	//decode_error(thread, einfo, PRECISE_CAUSE_INVALID_PACKET);
	//warn("Bad Packet:HVX:%s:PC=0x%x\n",reason,einfo->elr);

	return 1;
}




#define FREE	1
#define USED	0



static int
check_scatter_gather_packet(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, exception_info *einfo)
{

	int current_insn = 0;
    int scatter_gather_count = 0;
	// Loop on vector instruction count
	for(current_insn = 0; current_insn < num_insn; current_insn++) {
		// valid instruction
		if(ilist[current_insn]>-1) {
			scatter_gather_count += GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_CVI_SCATTER) ;
			scatter_gather_count += GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_CVI_GATHER) ;
		}
	}

    if (scatter_gather_count>1) {
        return handle_bad_packet(thread, einfo, "Only one scatter/gather opcode per packet");
    }
    return 0;

}

static int
check_dv_instruction(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, unsigned int attribute, hvx_resource_t resource0, hvx_resource_t resource1, exception_info *einfo)
{

	int current_insn = 0;
	// Loop on vector instruction count
	for(current_insn = 0; current_insn < num_insn; current_insn++) {
		// valid instruction
		if(ilist[current_insn]>-1) {
			if(  GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, attribute) ) {
				// Needs two available resources
				if ((resources[resource0]+resources[resource1]) == 2*FREE) {
					resources[resource0] = USED;
					resources[resource1] = USED;
					packet->insn[ilist[current_insn]].hvx_resource |= (1<<resource0);
					packet->insn[ilist[current_insn]].hvx_resource |= (1<<resource1);
				} else {
					return handle_bad_packet(thread, einfo, "Double vector instruction needs two resources");
				}
				ilist[current_insn] = -1; 	// Remove Instruction
			}
		}
	}
	return 0;
}

/* Double Vector instructions that can use anyone of specific or both pairs */
static int
check_dv_instruction2(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, unsigned int attribute, hvx_resource_t resource3, hvx_resource_t resource2, hvx_resource_t resource1, hvx_resource_t resource0, exception_info *einfo)
{

	int current_insn = 0;
	// Loop on vector instruction count
	for(current_insn = 0; current_insn < num_insn; current_insn++)
	{
		// valid instruction
		int insn_idx = ilist[current_insn];
		if(insn_idx >-1) {
			if(  GET_ATTRIB(packet->insn[insn_idx].opcode, attribute) ) {
				// Needs two available resources

				// Try by even/odd slot assignment
				int slot = packet->insn[insn_idx].slot & 0x1;

				int first_try = (slot) ? resource2 : resource0;
				int second_try = (slot) ? resource0 : resource2;
				if ((resources[first_try]+resources[first_try+1]) == 2*FREE) {
					resources[first_try] = USED;
					resources[first_try+1] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<(first_try));
					packet->insn[insn_idx].hvx_resource |= (1<<(first_try+1));
				} else if ((resources[second_try]+resources[second_try+1]) == 2*FREE) {
					resources[second_try] = USED;
					resources[second_try+1] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<(second_try));
					packet->insn[insn_idx].hvx_resource |= (1<<(second_try+1));
				} else {
					return handle_bad_packet(thread, einfo, "Double vector instruction needs two resources from one of two options.");
				}

				ilist[current_insn] = -1; 	// Remove Instruction
			}
		}
	}
	return 0;
}

static int
check_umem_instruction(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, exception_info *einfo)
{

	int current_insn = 0;
	// Loop on vector instruction count
	for(current_insn = 0; current_insn < num_insn; current_insn++)
	{
		// valid instruction
		if(ilist[current_insn]>-1)
		{
			// check attribute
			if( GET_ATTRIB(packet->insn[ilist[current_insn]].opcode,  A_CVI_VP) && GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_CVI_VM))
			{
				// Needs three available resources, both mem and permute
				if ((resources[HVX_RESOURCE_LOAD]+resources[HVX_RESOURCE_STORE]+resources[HVX_RESOURCE_PERM]) == 3*FREE) {
					resources[HVX_RESOURCE_LOAD] = USED;
					resources[HVX_RESOURCE_STORE] = USED;
					resources[HVX_RESOURCE_PERM] = USED;
					packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_LOAD);
					packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_STORE);
					packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_PERM);
				}
				else
				{
					return handle_bad_packet(thread, einfo, "one or more of load, store, or permute resource unavailable.");
				}

				ilist[current_insn] = -1; 	// Remove Instruction
			}
		}
	}
	return 0;
}


/* Memory instructions */
static int
check_mem_instruction(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, exception_info *einfo)
{

	int current_insn = 0;
	// Loop on vector instruction count
	for (current_insn = 0; current_insn < num_insn; current_insn++) {
		// valid instruction
		if (ilist[current_insn]>-1) {
			if (GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_CVI_VM)) {
				if (GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_LOAD)) {
					if (resources[HVX_RESOURCE_LOAD] == FREE) {
						resources[HVX_RESOURCE_LOAD] = USED;
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_LOAD);
					} else {
						return handle_bad_packet(thread, einfo, "load resource unavailable");
					}

				} else if( GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_STORE)) {
					if (resources[HVX_RESOURCE_STORE] == FREE) {
						resources[HVX_RESOURCE_STORE] = USED;
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_STORE);
					} else {
						return handle_bad_packet(thread, einfo, "store resource unavailable");
					}
				} else {
					return handle_bad_packet(thread, einfo, "unknown vector memory instruction");
				}

				// Not a load temp, not a store new, and not a vfetch
				if ( !( GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_CVI_TMP) &&
						GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_LOAD   )    ) &&
					 !( GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_CVI_NEW) &&
						GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_STORE  )  ) &&
					 ! GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_VFETCH ) )
				{
					/* Grab any one of the other resources */
					if (resources[HVX_RESOURCE_PERM] == FREE) {
						resources[HVX_RESOURCE_PERM] = USED;
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_PERM);
					} else if (resources[HVX_RESOURCE_SHIFT] == FREE) {
						resources[HVX_RESOURCE_SHIFT] = USED;
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_SHIFT);
					} else if (resources[HVX_RESOURCE_MPY0] == FREE) {
						resources[HVX_RESOURCE_MPY0] = USED;
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_MPY0);
					} else if (resources[HVX_RESOURCE_MPY1] == FREE) {
						resources[HVX_RESOURCE_MPY1] = USED;
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_MPY1);
					} else {
						return handle_bad_packet(thread, einfo, "no vector execution resources available");
					}
				}
				ilist[current_insn] = -1; 	// Remove Instruction
			}
		}
	}
	return 0;
}

/* Single Vector instructions that can use anyone of one, two, or four resources */
/* Insert instruction into one possible resource */
static int
check_instruction1(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, unsigned int attribute, hvx_resource_t resource0, exception_info *einfo)
{

	int current_insn = 0;
	// Loop on vector instruction count
	for(current_insn = 0; current_insn < num_insn; current_insn++) {
		// valid instruction
		if (ilist[current_insn]>-1) {
			if (GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, attribute) ) {
				// Needs two available resources
				if (resources[resource0] == FREE) {
					resources[resource0] = USED;
					packet->insn[ilist[current_insn]].hvx_resource |= (1<<resource0);
				} else {
					return handle_bad_packet(thread, einfo, "unavailable vector resource");
				}

				ilist[current_insn] = -1; 	// Remove Instruction
			}
		}
	}
	return 0;
}

/* Insert instruction into one of two possible resource2 */
static int
check_instruction2(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, unsigned int attribute, hvx_resource_t resource1, hvx_resource_t resource0, exception_info *einfo)
{

	int current_insn = 0;
	// Loop on vector instruction count
	for(current_insn = 0; current_insn < num_insn; current_insn++) {
		// valid instruction
		int insn_idx = ilist[current_insn];
		if(insn_idx>-1) {
			if(  GET_ATTRIB(packet->insn[insn_idx].opcode, attribute) ) {

				// Try by slot assignment
				int slot = packet->insn[insn_idx].slot;

				int first_try  = (slot == resource1) ? resource1 : resource0;
				int second_try = (slot == resource1) ? resource0 : resource1;

				// Needs one of two possible available resources
				if (resources[first_try] == FREE) {
					resources[first_try] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<first_try);
				} else if (resources[second_try] == FREE)  {
					resources[second_try] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<second_try);
				} else {
					return handle_bad_packet(thread, einfo, "unavailable vector resource");
				}

				ilist[current_insn] = -1; 	// Remove Instruction
			}
		}
	}
	return 0;
}


#define SWAP_SORT(A, B, C, D) \
	if (A < B) { \
		int tmp = B;\
		B = A;\
		A = tmp;\
		tmp = D;\
		D = C;\
		C = tmp;\
	}\




/* Insert instruction into one of 4 four possible resource */
static int
check_instruction4(thread_t * thread,  hvx_resource_t * resources, int * ilist, int num_insn, Packet *packet, unsigned int attribute, exception_info *einfo)
{
	for (int current_insn = 0; current_insn < num_insn; current_insn++) {
		// valid instruction
		int insn_idx = ilist[current_insn];
		if (insn_idx>-1) {
			if (GET_ATTRIB(packet->insn[insn_idx].opcode, attribute)) {
				if (resources[HVX_RESOURCE_PERM] == FREE) {
					resources[HVX_RESOURCE_PERM] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<HVX_RESOURCE_PERM);
					ilist[current_insn] = -1; 	// Remove Instruction
				} else if (resources[HVX_RESOURCE_SHIFT] == FREE) {
					resources[HVX_RESOURCE_SHIFT] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<HVX_RESOURCE_SHIFT);
					ilist[current_insn] = -1; 	// Remove Instruction
				} else if (resources[HVX_RESOURCE_MPY0] == FREE) {
					resources[HVX_RESOURCE_MPY0] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<HVX_RESOURCE_MPY0);
					ilist[current_insn] = -1; 	// Remove Instruction
				} else if (resources[HVX_RESOURCE_MPY1] == FREE) {
					resources[HVX_RESOURCE_MPY1] = USED;
					packet->insn[insn_idx].hvx_resource |= (1<<HVX_RESOURCE_MPY1);
					ilist[current_insn] = -1; 	// Remove Instruction
				}
			}
		}
	}
	// Check to make sure everything has mapped
	for (int current_insn = 0; current_insn < num_insn; current_insn++) {
		int insn_idx = ilist[current_insn];
		if (insn_idx>-1) {
			if (GET_ATTRIB(packet->insn[insn_idx].opcode, attribute)) {
				return handle_bad_packet(thread, einfo, "unavailable vector resource");
			}
		}
	}

	return 0;
}

static int
check_4res_instruction(thread_t * thread, hvx_resource_t * resources, int * ilist, int num_insn, Packet * packet, exception_info *einfo)
{

	int current_insn = 0;
	for(current_insn = 0; current_insn < num_insn; current_insn++) {
		if(ilist[current_insn]>-1) {
			if(  GET_ATTRIB(packet->insn[ilist[current_insn]].opcode, A_CVI_4SLOT) ) {
				int available_resource =
						resources[HVX_RESOURCE_PERM]
						+ resources[HVX_RESOURCE_SHIFT]
						+ resources[HVX_RESOURCE_MPY0]
						+ resources[HVX_RESOURCE_MPY1];

				if (available_resource == 4*FREE) {
						resources[HVX_RESOURCE_PERM] = USED;
						resources[HVX_RESOURCE_SHIFT] = USED;
						resources[HVX_RESOURCE_MPY0] = USED;
						resources[HVX_RESOURCE_MPY1] = USED;
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_PERM);
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_SHIFT);
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_MPY0);
						packet->insn[ilist[current_insn]].hvx_resource |= (1<<HVX_RESOURCE_MPY1);

				} else {
					return handle_bad_packet(thread, einfo, "all vector resources needed, not all available");
				}

				ilist[current_insn] = -1; 	// Remove Instruction
			}
		}

	}
	return 0;
}



static int
decode_populate_cvi_resources(thread_t * thread, Packet *packet, exception_info *einfo)
{

	int i,num_insn=0;
	int vlist[4] = {-1,-1,-1,-1};
	hvx_resource_t hvx_resources[8] = {FREE,FREE,FREE,FREE,FREE,FREE,FREE,FREE};	// All Available
	int errors = 0;

	/* Count Vector instructions and check for deprecated ones */
	for (num_insn=0, i = 0; i < packet->num_insns; i++) {
		if (GET_ATTRIB(packet->insn[i].opcode, A_CVI)) {
			vlist[num_insn++] = i;
		}
	}
	/* Instructions that consume all vector resources */
	errors += check_4res_instruction(thread, hvx_resources, vlist, num_insn, packet, einfo);

	/* Insert Unaligned Memory Access */
	errors += check_umem_instruction(thread, hvx_resources, vlist, num_insn, packet, einfo);

	/* double vector permute Consumes both permute and shift resources */
	errors += check_dv_instruction(thread, hvx_resources, vlist, num_insn, packet, A_CVI_VP_VS, HVX_RESOURCE_SHIFT, HVX_RESOURCE_PERM, einfo);

	/* Try to insert double vector multiply */
	errors += check_dv_instruction(thread, hvx_resources, vlist, num_insn, packet, A_CVI_VX_DV, HVX_RESOURCE_MPY1, HVX_RESOURCE_MPY0, einfo);

	/* Single vector permute can only go to permute resource */
	errors += check_instruction1(thread, hvx_resources, vlist, num_insn, packet, A_CVI_VP, HVX_RESOURCE_PERM, einfo);

	/* Single vector permute can only go to shift resource */
	errors += check_instruction1(thread, hvx_resources, vlist, num_insn, packet, A_CVI_VS, HVX_RESOURCE_SHIFT, einfo);

	/* Single vector multiply instruction can  go to one of two VX resources */
	errors += check_instruction2(thread, hvx_resources, vlist, num_insn, packet, A_CVI_VX, HVX_RESOURCE_MPY1, HVX_RESOURCE_MPY0, einfo);

	/* Try to insert double vector alu */
	errors += check_dv_instruction2(thread, hvx_resources, vlist, num_insn, packet, A_CVI_VA_DV, HVX_RESOURCE_MPY1, HVX_RESOURCE_MPY0, HVX_RESOURCE_SHIFT, HVX_RESOURCE_PERM, einfo);

	/* Memory Instruction assignment */
	errors += check_mem_instruction(thread, hvx_resources, vlist, num_insn, packet, einfo);

	/* single vector alu can go on any of the 4 pipes */
	errors += check_instruction4(thread, hvx_resources, vlist, num_insn, packet, A_CVI_VA, einfo);

	errors += check_scatter_gather_packet(thread, hvx_resources, vlist, num_insn, packet, einfo);

	//THREAD2STRUCT->pkt_total_mx = (hvx_resources[HVX_RESOURCE_MPY0] == USED) + (hvx_resources[HVX_RESOURCE_MPY1] == USED);

	if (errors) {
		printf("HVX has packet errors (12/12/19 - compiler/assembler may not be working correctly yet ):\n");
		//gdb_print_pkt(packet, thread);
	}


    return errors;
}



static int
check_new_value(thread_t * thread, Packet * packet, exception_info *einfo)
{
    /* .new value for a MMVector store */
    int i, j;
    const char *reginfo;
    const char *destletters;
    const char *dststr = NULL;
    uint16_t def_opcode;
    char letter;
    int def_regnum;

    for (i = 1; i < pkt->num_insns; i++) {
        uint16_t use_opcode = pkt->insn[i].opcode;
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
            int def_off = ((pkt->insn[i].regno[use_regidx]) >> 1);
            int def_oreg = pkt->insn[i].regno[use_regidx] & 1;
            int def_idx = -1;
            for (j = i - 1; (j >= 0) && (def_off >= 0); j--) {
                if (!GET_ATTRIB(pkt->insn[j].opcode, A_CVI)) {
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
            g_assert(!((def_off != 0) || (def_idx < 0) ||
                       (def_idx > (pkt->num_insns - 1))));

            /* def_idx is the index of the producer */
            def_opcode = pkt->insn[def_idx].opcode;
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
                pkt->insn[i].regno[use_regidx] = def_oreg;
                pkt->insn[i].new_value_producer_slot = pkt->insn[def_idx].slot;
            } else {
                if (dststr == NULL) {
                    /* still not there, we have a bad packet */
                    g_assert_not_reached();
                }
                def_regnum = pkt->insn[def_idx].regno[dststr - reginfo];
                /* Now patch up the consumer with the register number */
                pkt->insn[i].regno[use_regidx] = def_regnum ^ def_oreg;
                /* special case for (Vx,Vy) */
                dststr = strchr(reginfo, 'y');
                if (def_oreg && strchr(reginfo, 'x') && dststr) {
                    def_regnum = pkt->insn[def_idx].regno[dststr - reginfo];
                    pkt->insn[i].regno[use_regidx] = def_regnum;
                }
                /*
                 * We need to remember who produces this value to later
                 * check if it was dynamically cancelled
                 */
                pkt->insn[i].new_value_producer_slot = pkt->insn[def_idx].slot;
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
decode_mmvec_move_cvi_to_end(Packet *pkt, int max)
{
    int i;
    for (i = 0; i < max; i++) {
        if (GET_ATTRIB(pkt->insn[i].opcode, A_CVI)) {
            int last_inst = pkt->num_insns - 1;
            uint16_t last_opcode = pkt->insn[last_inst].opcode;

            /*
             * If the last instruction is an endloop, move to the one before it
             * Keep endloop as the last thing always
             */
            if ((last_opcode == J2_endloop0) ||
                (last_opcode == J2_endloop1) ||
                (last_opcode == J2_endloop01)) {
                last_inst--;
            }

            decode_send_insn_to(pkt, i, last_inst);
            max--;
            i--;    /* Retry this index now that packet has rotated */
        }
    }
}

static void
decode_shuffle_for_execution_vops(Packet *pkt)
{
    /*
     * Sort for .new
     */
    int i;
    for (i = 0; i < pkt->num_insns; i++) {
        uint16_t opcode = pkt->insn[i].opcode;
        if (GET_ATTRIB(opcode, A_LOAD) &&
            (GET_ATTRIB(opcode, A_CVI_NEW) ||
             GET_ATTRIB(opcode, A_CVI_TMP))) {
            /*
             * Find prior consuming vector instructions
             * Move to end of packet
             */
            decode_mmvec_move_cvi_to_end(pkt, i);
            break;
        }
    }

    /* Move HVX new value stores to the end of the packet */
    for (i = 0; i < pkt->num_insns - 1; i++) {
        uint16_t opcode = pkt->insn[i].opcode;
        if (GET_ATTRIB(opcode, A_STORE) &&
            GET_ATTRIB(opcode, A_CVI_NEW) &&
            !GET_ATTRIB(opcode, A_CVI_SCATTER_RELEASE)) {
            int last_inst = pkt->num_insns - 1;
            uint16_t last_opcode = pkt->insn[last_inst].opcode;

            /*
             * If the last instruction is an endloop, move to the one before it
             * Keep endloop as the last thing always
             */
            if ((last_opcode == J2_endloop0) ||
                (last_opcode == J2_endloop1) ||
                (last_opcode == J2_endloop01)) {
                last_inst--;
            }

            decode_send_insn_to(pkt, i, last_inst);
            break;
        }
    }
}

static void
check_for_vhist(Packet *pkt)
{
    pkt->pkt_has_vhist = false;
    for (int i = 0; i < pkt->num_insns; i++) {
        Insn *insn = &pkt->insn[i];
        int opcode = insn->opcode;
        if (GET_ATTRIB(opcode, A_CVI) && GET_ATTRIB(opcode, A_CVI_4SLOT)) {
                pkt->pkt_has_vhist = true;
                return;
        }
    }
}

/*
 * Public Functions
 */

SlotMask mmvec_ext_decode_find_iclass_slots(int opcode)
{
    if (GET_ATTRIB(opcode, A_CVI_VM)) {
        /* HVX memory instruction */
        if (GET_ATTRIB(opcode, A_RESTRICT_SLOT0ONLY)) {
            return SLOTS_0;
        } else if (GET_ATTRIB(opcode, A_RESTRICT_SLOT1ONLY)) {
            return SLOTS_1;
        }
        return SLOTS_01;
    } else if (GET_ATTRIB(opcode, A_RESTRICT_SLOT2ONLY)) {
        return SLOTS_2;
    } else if (GET_ATTRIB(opcode, A_CVI_VX)) {
        /* HVX multiply instruction */
        return SLOTS_23;
    } else if (GET_ATTRIB(opcode, A_CVI_VS_VX)) {
        /* HVX permute/shift instruction */
        return SLOTS_23;
    } else {
        return SLOTS_0123;
    }
}

void mmvec_ext_decode_checks(Packet *pkt, bool disas_only)
{
    check_new_value(pkt);
    if (!disas_only) {
        decode_shuffle_for_execution_vops(pkt);
    }
    check_for_vhist(pkt);
}
