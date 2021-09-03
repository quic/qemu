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
	// .New Value for a MMVector Store
	int i,j;
	const char *reginfo;
	const char *destletters;
	const char *dststr = NULL;
	size2u_t def_opcode;
	char letter;
	int def_regnum;
	for(i = 1; i < packet->num_insns; i++){
		size2u_t use_opcode = packet->insn[i].opcode;
		if ( GET_ATTRIB(use_opcode, A_DOTNEWVALUE) && GET_ATTRIB(use_opcode, A_CVI)&& GET_ATTRIB(use_opcode, A_STORE)) {
			int use_regidx = strchr(opcode_reginfo[use_opcode], 's') - opcode_reginfo[use_opcode];
			/* What's encoded at the N-field is the offset to who's producing the value.
			   Shift off the LSB which indicates odd/even register.
			 */
			int def_off = ((packet->insn[i].regno[use_regidx]) >> 1);
			int def_oreg = packet->insn[i].regno[use_regidx] & 1;
			int def_idx = -1;
			/*warn("Trying to decode .new: def_off=%d\n",def_off);*/
			/*if (def_oreg) warn("EJP: odd oreg: %d pc=%08x",def_oreg,thread->Regs[REG_PC]);*/
			for (j = i-1; (j >= 0) && (def_off >= 0); j--) {
				if (!GET_ATTRIB(packet->insn[j].opcode,A_CVI)) continue;
				def_off--;
				if (def_off == 0) { def_idx = j; break; }
			}
			/* Check for a badly encoded N-field which points to an instruction
			   out-of-range */
			if ((def_off != 0) || (def_idx < 0) || (def_idx > (packet->num_insns - 1))) {
				return handle_bad_packet(thread, einfo, "A new-value consumer has no valid producer!");
			}
			/* previous insn is the producer */
			def_opcode = packet->insn[def_idx].opcode;
			reginfo = opcode_reginfo[def_opcode];
			destletters = "dexy";
			for (j = 0; (letter = destletters[j]) != 0; j++) {
				dststr = strchr(reginfo,letter);
				if (dststr != NULL) break;
			}
			if ((dststr != NULL) && GET_ATTRIB(def_opcode,A_CVI_TMP)) {
            	packet->invalid_new_target = 1;
            }

            if ((dststr == NULL)  && GET_ATTRIB(def_opcode,A_CVI_GATHER)) {
                def_regnum = 0;
                packet->insn[i].regno[use_regidx] = def_oreg;
                packet->insn[i].new_value_producer_slot = packet->insn[def_idx].slot;
            } else {
                if (dststr == NULL) {   /* still not there, we have a bad packet */
                    return handle_bad_packet(thread, einfo, "A new-value consumer has no valid producer! (can't find written reg)");
                }
                if (packet->invalid_new_target == 1) {   /* still not there, we have a bad packet */
                    return handle_bad_packet(thread, einfo, "A new-value consumer has no valid producer! (can't find written reg)");
                }
                def_regnum = packet->insn[def_idx].regno[dststr - reginfo];
                /* Now patch up the consumer with the register number */
                packet->insn[i].regno[use_regidx] = def_regnum ^ def_oreg;
                /* special case for (Vx,Vy) */
                if (def_oreg && strchr(reginfo,'x') && (dststr = strchr(reginfo,'y'))) {
                    def_regnum = packet->insn[def_idx].regno[dststr - reginfo];
                    packet->insn[i].regno[use_regidx] = def_regnum;
                }
                /* We need to remember who produces this value to later check if it was dynamically cancelled */
                packet->insn[i].new_value_producer_slot = packet->insn[def_idx].slot;
            }
		}
	}
	return 0;
}


#if 0
// Shouln't be needed due to SLOT1 restriction of VMEMU
void decode_check_vmemu_and_scalar_memory_ops(thread_t *thread, Packet * packet,exception_info *einfo){

    int packet_has_scalar_mem   = 0;
    int packet_has_vmemu        = 0;
    int i;

    for(i = 0; i < packet->num_insns; i++){

        packet_has_scalar_mem   |= ((GET_ATTRIB(packet->insn[i].opcode, A_LOAD)
                                    || GET_ATTRIB(packet->insn[i].opcode, A_STORE))
                                    && (!GET_ATTRIB(packet->insn[i].opcode, A_MMVECX)));

        packet_has_vmemu        |= GET_ATTRIB(packet->insn[i].opcode,  A_MMVEC_VMU);
    }

    if ((packet_has_scalar_mem>0) && (packet_has_vmemu>0)){
        printf("A packet cannot have an unaligned vector memory access and a scalar memeory access\n");
        char pkt_buf[1024];
        snprint_a_pkt(pkt_buf, 1024, packet, NULL);
        printf(" %s\n", pkt_buf);
        decode_error(thread,einfo,PRECISE_CAUSE_INVALID_PACKET);
    }

}
#endif


/*
 * EJP: hw-issue 3285
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
		if (GET_ATTRIB(packet->insn[i].opcode,A_CVI)) {
            int last_inst = packet->num_insns-1;

            // If the last instruction is an endloop, move to the one before it
            // Keep endloop as the last thing always
            if ( (packet->insn[last_inst].opcode == J2_endloop0)
					|| (packet->insn[last_inst].opcode == J2_endloop1)
					|| (packet->insn[last_inst].opcode == J2_endloop01))
                    last_inst--;
            if (!GET_ATTRIB(packet->insn[last_inst].opcode,A_CVI) &&
            	(GET_ATTRIB(packet->insn[last_inst].opcode,A_LOAD) || GET_ATTRIB(packet->insn[last_inst].opcode,A_STORE)))
                    last_inst--;
            if (!GET_ATTRIB(packet->insn[last_inst].opcode,A_CVI) &&
            	(GET_ATTRIB(packet->insn[last_inst].opcode,A_LOAD) || GET_ATTRIB(packet->insn[last_inst].opcode,A_STORE)))
                    last_inst--;

			decode_send_insn_to(packet,i,last_inst);
			max--;
			i--;	/* Retry this index now that packet has rotated */
		}
	}
}

static int
decode_shuffle_for_execution_vops(Packet * packet, exception_info *einfo)
{
	/* Sort for V.new = VMEM()
	 * Right now we need to make sure that the vload occurs before the permute instruction or VPVX ops
	 */
	int i;
	for (i = 0; i < packet->num_insns; i++) {
		if ((GET_ATTRIB(packet->insn[i].opcode,A_LOAD) &&
			(GET_ATTRIB(packet->insn[i].opcode,A_CVI_NEW))) ||
			 GET_ATTRIB(packet->insn[i].opcode,A_CVI_TMP)) {
			decode_mmvec_move_cvi_to_end(packet,i);
			break;
		}
	}
	for (i = 0; i < packet->num_insns-1; i++) {
		if (GET_ATTRIB(packet->insn[i].opcode,A_STORE)
			&& GET_ATTRIB(packet->insn[i].opcode,A_CVI_NEW) && !GET_ATTRIB(packet->insn[i].opcode,A_CVI_SCATTER_RELEASE)) {
			/* decode_send_insn_to the end */
            int last_inst = packet->num_insns-1;

            // If the last instruction is an endloop, move to the one before it
            // Keep endloop as the last thing always
            if ( (packet->insn[last_inst].opcode == J2_endloop0)
					|| (packet->insn[last_inst].opcode == J2_endloop1)
					|| (packet->insn[last_inst].opcode == J2_endloop01))
                    last_inst--;

			decode_send_insn_to(packet,i,last_inst);
			break;
		}
	}
	return 0;
}

// Collect stats on HVX packet
static void
decode_hvx_packet_contents(thread_t * thread, Packet * pkt) {
	pkt->pkt_hvx_va = 0;
	pkt->pkt_hvx_vx = 0;
	pkt->pkt_hvx_vp = 0;
	pkt->pkt_hvx_vs = 0;
	pkt->pkt_hvx_all = 0;
	pkt->pkt_hvx_none = 0;
	pkt->pkt_hvx_vs_3src = 0;
	pkt->pkt_hvx_va_2src = 0;
	pkt->pkt_hvx_insn = 0;
  pkt->pkt_has_hvx = 0;

	for (int i = 0; i < pkt->num_insns; i++) {
		int opcode = pkt->insn[i].opcode;
		pkt->pkt_hvx_va += GET_ATTRIB(opcode,A_CVI_VA);
		pkt->pkt_hvx_vx += GET_ATTRIB(opcode,A_CVI_VX);
		pkt->pkt_hvx_vp += GET_ATTRIB(opcode,A_CVI_VP);
		pkt->pkt_hvx_vs += GET_ATTRIB(opcode,A_CVI_VS);
		pkt->pkt_hvx_none += GET_ATTRIB(opcode,A_CVI_TMP);
		pkt->pkt_hvx_all += GET_ATTRIB(opcode,A_CVI_4SLOT);
		pkt->pkt_hvx_va += 2*GET_ATTRIB(opcode,A_CVI_VA_DV);
		pkt->pkt_hvx_vx += 2*GET_ATTRIB(opcode,A_CVI_VX_DV);
		pkt->pkt_hvx_vp += GET_ATTRIB(opcode,A_CVI_VP_VS);
		pkt->pkt_hvx_vs += GET_ATTRIB(opcode,A_CVI_VP_VS);
		pkt->pkt_hvx_va_2src |= ( GET_ATTRIB(opcode,A_CVI) && GET_ATTRIB(opcode,A_CVI_VA_2SRC) );
		pkt->pkt_hvx_vs_3src |= ( GET_ATTRIB(opcode,A_CVI) && GET_ATTRIB(opcode,A_CVI_VS_3SRC) );
		pkt->pkt_hvx_insn += GET_ATTRIB(opcode,A_CVI);
    pkt->pkt_has_hvx |= pkt->insn[i].hvx_resource ? 1 : 0;
	}
}

static int
decode_mx_ops(thread_t * thread, Packet * pkt, exception_info * einfo) {
#if 0
	for (int i = 0; i < pkt->num_insns; i++) {
		if ( GET_ATTRIB(pkt->insn[i].opcode, A_HMX) && !thread->processor_ptr->arch_proc_options->QDSP6_MX_PRESENT)
			return handle_bad_packet(thread, einfo, "Architecture doesn't support Matrix Op. Only v68 and up");
	}
#endif
	return 0;
}


#define MAX_HVX_VECTOR_RD_PER_PACKET 8
#define MAX_HVX_VECTOR_WR_PER_PACKET 4

static int
count_vector_access(thread_t * thread, Packet * pkt, exception_info * einfo) {
	int error = 0;
	if (pkt->pkt_hvx_vs_3src && pkt->pkt_hvx_va_2src) {
					//gdb_print_pkt(pkt, thread);
		handle_bad_packet(thread, einfo, "Packet with non unary alu and 3 vector source narrowing shift");
					return 1;
				}
	if (pkt->pkt_hvx_vs_3src && pkt->pkt_hvx_vp) {
				//gdb_print_pkt(pkt, thread);
		handle_bad_packet(thread, einfo, "Packet with permute and 3 vector source narrowing shift");
					return 1;
			}
	if (error) {
		printf("Error due to VP slot not having a free read port\n");

	}

	return error;
}

static void
check_for_vhist(Packet *pkt)
{
    pkt->pkt_has_vhist = false;
    for (int i = 0; i < pkt->num_insns; i++) {
        int opcode = pkt->insn[i].opcode;
        if (GET_ATTRIB(opcode, A_CVI) && GET_ATTRIB(opcode, A_CVI_4SLOT)) {
                pkt->pkt_has_vhist = true;
                return;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////
/// Public Functions
////////////////////////////////////////////////////////////////////////////////

SlotMask mmvec_ext_decode_find_iclass_slots(int opcode)
{
	if (GET_ATTRIB(opcode, A_CVI_VM)) {
		// || GET_ATTRIB(opcode, A_MMVEC_VP) || GET_ATTRIB(opcode, A_MMVEC_VS))
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

int mmvec_ext_decode_checks(thread_t * thread, Packet *packet, exception_info *einfo){
	int errors = 0;
    packet->pkt_has_vmemu_access = 0; // Cleared, set by instruction if vmemu is truly unaligned


	errors += check_new_value(thread, packet, einfo);
	errors += decode_populate_cvi_resources(thread, packet, einfo);
	errors += decode_shuffle_for_execution_vops(packet, einfo);
	errors += decode_mx_ops(thread, packet, einfo);
	errors += count_vector_access(thread, packet, einfo);
  if (errors == 0) {
  	decode_hvx_packet_contents(thread, packet);
  }

    check_for_vhist(packet);

	return errors;
}


