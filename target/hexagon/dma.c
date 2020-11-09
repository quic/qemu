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

/*
 * User-DMA Engine.
 *
 * - This version implements "Hexagon V68 Architecture System-Level
 *   Specification - User DMA" which was released on July 28, 2018.
 *
 * - The specification is available from;
 *   https://sharepoint.qualcomm.com/qct/Modem-Tech/QDSP6/Shared%20Documents/
 *   QDSP6/QDSP6v68/Architecture/DMA/UserDMA.pdf
 *
 * TODO:
 * Should this code also use dma_decomposition.[ch]? to share code with the timing model.
 *
 *
 */

#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#include "dma.h"
#include "dma_adapter.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#if 0
#define PRINTF(DMA, ...) \
{ \
	FILE * fp = dma_adapter_debug_log(DMA);\
	if (fp) {\
		fprintf(fp, __VA_ARGS__);\
        fflush(fp);\
	}\
}
#else
#define PRINTF(DMA, ...)
#endif

#ifndef MIN
#define MIN(A,B) (((A) < (B)) ? (A) : (B))
#endif

// If you want to dump a descriptor contents, use this function.
static void dump_dma_desc(dma_t *dma, void* d, uint32_t type, const char* spot)
{
#if 0
#if 1
	char s[512];
	HEXAGON_DmaDescriptor_toStr(s, (HEXAGON_DmaDescriptor_t *)d);
	PRINTF(dma, "DMA %d: DMA desc dump @%s: %s\n", dma->num, spot, s);

#else
	PRINTF(dma, "DMA %d: DMA desc dump @%s:\n", dma->num, spot);

    if (type == 0)
    {
        PRINTF(dma, "   next             =0x%x\n", (unsigned int)get_dma_desc_next(d));
        PRINTF(dma, "   length           =0x%x\n", (unsigned int)get_dma_desc_length(d));
        PRINTF(dma, "   desctype         =0x%x\n", (unsigned int)get_dma_desc_desctype(d));
        PRINTF(dma, "   comp_src         =0x%x\n", (unsigned int)get_dma_desc_srccomp(d));
        PRINTF(dma, "   comp_dst         =0x%x\n", (unsigned int)get_dma_desc_dstcomp(d));
        PRINTF(dma, "   bypass_src       =0x%x\n", (unsigned int)get_dma_desc_bypasssrc(d));
        PRINTF(dma, "   bypass_dst       =0x%x\n", (unsigned int)get_dma_desc_bypassdst(d));
        PRINTF(dma, "   order            =0x%x\n", (unsigned int)get_dma_desc_order(d));
        PRINTF(dma, "   dstate           =0x%x\n", (unsigned int)get_dma_desc_dstate(d));
        PRINTF(dma, "   src              =0x%x\n", (unsigned int)get_dma_desc_src(d));
        PRINTF(dma, "   dst              =0x%x\n", (unsigned int)get_dma_desc_dst(d));
    }
    else if (type == 1)
    {
        PRINTF(dma, "   next             =0x%x\n", (unsigned int)get_dma_desc_next(d));
        PRINTF(dma, "   length           =0x%x\n", (unsigned int)get_dma_desc_length(d));
        PRINTF(dma, "   desctype         =0x%x\n", (unsigned int)get_dma_desc_desctype(d));
        PRINTF(dma, "   comp_src         =0x%x\n", (unsigned int)get_dma_desc_srccomp(d));
        PRINTF(dma, "   comp_dst         =0x%x\n", (unsigned int)get_dma_desc_dstcomp(d));
        PRINTF(dma, "   bypass_src       =0x%x\n", (unsigned int)get_dma_desc_bypasssrc(d));
        PRINTF(dma, "   bypass_dst       =0x%x\n", (unsigned int)get_dma_desc_bypassdst(d));
        PRINTF(dma, "   order            =0x%x\n", (unsigned int)get_dma_desc_order(d));
        PRINTF(dma, "   dstate           =0x%x\n", (unsigned int)get_dma_desc_dstate(d));
        PRINTF(dma, "   src              =0x%x\n", (unsigned int)get_dma_desc_src(d));
        PRINTF(dma, "   dst              =0x%x\n", (unsigned int)get_dma_desc_dst(d));
        PRINTF(dma, "   roi_width        =0x%x\n", (unsigned int)get_dma_desc_roiwidth(d));
        PRINTF(dma, "   roi_height       =0x%x\n", (unsigned int)get_dma_desc_roiheight(d));
        PRINTF(dma, "   src_stride       =0x%x\n", (unsigned int)get_dma_desc_srcstride(d));
        PRINTF(dma, "   dst_stride       =0x%x\n", (unsigned int)get_dma_desc_dststride(d));
        PRINTF(dma, "   src_width_offset =0x%x\n", (unsigned int)get_dma_desc_srcwidthoffset(d));
        PRINTF(dma, "   dst_width_offset =0x%x\n", (unsigned int)get_dma_desc_dstwidthoffset(d));
        PRINTF(dma, "   padding          =0x%x\n", (unsigned int)get_dma_desc_padding(d));
    }
#endif
#endif
}

static uint32_t get_transfer_size_to_aligned_va(dma_t *dma, uint32_t va, uint32_t max_transfer_size) {
    uint32_t tx_size = max_transfer_size;
    uint32_t mask = max_transfer_size - 1;
    while(tx_size) {
        if ((va & mask) == 0) {
            break;
        } else {
            tx_size >>= 1;
            mask >>= 1;
        }
    }
    return tx_size;
}

static uint32_t dma_data_read(dma_t *dma, uint64_t pa, uint32_t len, uint8_t *buffer)
{
    uint32_t bytes = len;
    uint32_t count;
    uint8_t *p = buffer;
    uint64_t pa_cur = pa;
    while (bytes)
    {
        count = 1;
        dma_adapter_memread(dma, 0, pa_cur, p++, count);
        pa_cur++;
        bytes -= count;
    }

    return 1;
}

static uint32_t dma_data_write(dma_t *dma, uint64_t pa, uint32_t len, uint8_t *buffer)
{
    uint32_t bytes = len;
    uint32_t count;
    uint8_t *p = buffer;
    uint64_t pa_cur = pa;

    while (bytes)
    {
        count = 1;
        dma_adapter_memwrite(dma, 0, pa_cur, p++, count);
        pa_cur++;
        bytes -= count;
    }

    return 1;
}





static int set_dma_error(dma_t *dma, uint32_t addr, uint32_t reason, const char * reason_str) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_ERROR);
    udma_ctx->ext_status = DM0_STATUS_ERROR;
    udma_ctx->error_state_reason_captured   = 1;
    udma_ctx->error_state_address           = addr;
    udma_ctx->error_state_reason            = reason;
    dma->error_state_reason = reason;       // Back to dma_adapter for debug help
    dma_adapter_set_dm5(dma, addr);

    if (udma_ctx->dm2.error_exception_enable)
    {
        udma_ctx->exception_status = 1;
        dma_adapter_register_error_exception(dma, addr);
    }
    PRINTF(dma, "DMA %d Tick %d: Entering Error State : %s va=%x\n", dma->num, udma_ctx->dma_tick_count, reason_str, addr);
    return 0;
}

static uint32_t check_dma_address(dma_t *dma, uint32_t addr, dma_memtype_t  * memtype, uint32_t result) {
    if (memtype->invalid_dma) {
        set_dma_error(dma, addr, DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_ADDRESS, "UNSUPPORTED_ADDRESS");
        result = 0;
    }
    return result;
}

void dma_force_error(dma_t *dma, uint32_t addr, uint32_t reason) {
    set_dma_error(dma, addr, reason, "forced");
}
void dma_target_desc(dma_t *dma, uint32_t addr, uint32_t width, uint32_t height) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    udma_ctx->target.va = addr;
    if (udma_ctx->target.va != 0) {
        udma_ctx->target.va = addr;
        udma_ctx->target.bytes_to_write  = width;
        udma_ctx->target.num_lines_write = height;
    }
}



int dma_target_desc_check(dma_t *dma, dma_decoded_descriptor_t * current, dma_decoded_descriptor_t * final);
int dma_target_desc_check(dma_t *dma, dma_decoded_descriptor_t * current, dma_decoded_descriptor_t * final) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    if ( udma_ctx->target.va  != 0) {
        uint32_t current_va = current->va;
        uint32_t final_va = final->va;
        if (final_va == current_va) {
            PRINTF(dma, "DMA %d: Tick %d: Target Decriptor reached=0x%08x active=0x%08x", dma->num, udma_ctx->dma_tick_count, final_va, current_va );
            if (current->desc.common.descSize) {
                if (( ((current->num_lines_write == final->num_lines_write) && (current->bytes_to_write == final->bytes_to_write)) || ((final->bytes_to_write == 0) ))  && (current->bytes_to_write!=0)) {
                    udma_ctx->target.va = 0;
                    PRINTF(dma, " !!!HIT!!! bytes to write=%d %d lines to write=%d %d\n", current->bytes_to_write, final->bytes_to_write, final->num_lines_write, current->num_lines_write);
                    return 1;
                }
            } else {
                if (((current->bytes_to_write == final->bytes_to_write) || (final->bytes_to_write==0))) {
                    udma_ctx->target.va = 0;
                    PRINTF(dma, " !!!HIT!!! bytes to write=%d %d\n", current->bytes_to_write, final->bytes_to_write);
                    return 1;
                }
            }
            PRINTF(dma, "\n");
        } else {
            PRINTF(dma, "DMA %d: Tick %d: Target Decriptor not reached=0x%08x active=0x%08x\n", dma->num, udma_ctx->dma_tick_count, final_va, current_va );
        }
    }
    return 0;
}



// DMA Engine status to string -------------------------------------------------
static void dump_dma_status(dma_t *dma, const char * buf, uint32_t val)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    PRINTF(dma, "DMA %d: Tick %d: %s = 0x%08x State Internal: ", dma->num, udma_ctx->dma_tick_count, buf, val);
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_IDLE))
    {
        PRINTF(dma, "IDLE ");
    }
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING))
    {
        PRINTF(dma, "RUNNING ");
    }
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED))
    {
        PRINTF(dma, "PAUSED ");
    }
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR))
    {
        PRINTF(dma, "ERROR ");
    }
    if (udma_ctx->exception_status)
    {
        PRINTF(dma, "EXCEPTION ");
    }
    PRINTF(dma, "\tExternal: ");
    if (udma_ctx->ext_status == DM0_STATUS_RUN)
    {
        PRINTF(dma, "RUNNING\n");
    }
    if (udma_ctx->ext_status == DM0_STATUS_IDLE)
    {
        PRINTF(dma, "IDLE\n");
    }
    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        PRINTF(dma, "ERROR\n");
    }

}

static void apply_transform(dma_t *dma, uint8_t *src, uint8_t *dst, uint32_t len)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    switch (udma_ctx->active.desc.type1.transform)
    {
        case DMA_XFORM_EXPAND_UPPER:
            for (uint32_t src_idx = 0, dst_idx = 0; src_idx < len; src_idx++, dst_idx+=2)
            {
                dst[dst_idx+0] = 0;
                dst[dst_idx+1] = src[src_idx];
            }
            break;

        case DMA_XFORM_EXPAND_LOWER:
            for (uint32_t src_idx = 0, dst_idx = 0; src_idx < len; src_idx++, dst_idx+=2)
            {
                dst[dst_idx+0] = src[src_idx];
                dst[dst_idx+1] = 0;
            }
            break;

        case DMA_XFORM_COMPRESS_UPPER:
            for (uint32_t src_idx = 1, dst_idx = 0; src_idx < len; src_idx++, dst_idx++)
            {
                 dst[dst_idx] = src[src_idx];
            }
            break;

        case DMA_XFORM_COMPRESS_LOWER:
            for (uint32_t src_idx = 0, dst_idx = 0; src_idx < len; src_idx++, dst_idx++)
            {
                dst[dst_idx] = src[src_idx];
            }
            break;

        case DMA_XFORM_SWAP:
            for (uint32_t src_idx = 0, dst_idx = 0; src_idx < len; src_idx+=2, dst_idx+=2)
            {
                dst[dst_idx+0] = src[src_idx+1];
                dst[dst_idx+1] = src[src_idx+0];
            }
            break;
        default: //DMA_XFORM_NONE
            for (uint32_t src_idx = 0, dst_idx = 0; src_idx < len; src_idx++, dst_idx++)
            {
                dst[dst_idx] = src[src_idx];
            }
            break;
    }
}


static void dma_transfer(dma_t *dma, uint32_t src_va,  uint64_t src_pa, uint32_t src_len, uint32_t dst_va,  uint64_t dst_pa, uint32_t dst_len)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    uint8_t buffer[DMA_MAX_TRANSFER_SIZE] = {0};
    uint8_t buffer_transform[DMA_MAX_TRANSFER_SIZE] = {0};
    uint8_t *p = buffer;

    dma_adapter_pmu_increment(dma, DMA_PMU_UNALIGNED_RD, (src_pa & (uint64_t)(((uint8_t)src_len  - 1))));
    dma_adapter_pmu_increment(dma, DMA_PMU_UNALIGNED_WR, (dst_pa & (uint64_t)(((uint8_t)dst_len - 1))));

    dma_data_read(dma, src_pa, src_len, buffer);

	if (dma->verbosity >= 5) {
    PRINTF(dma, "DMA %d: Tick %d: dma_load  for desc=%08x src va=%x pa=%llx len=%u data= ", dma->num, udma_ctx->dma_tick_count, udma_ctx->active.va, src_va, (long long int)src_pa, src_len);
    for(int32_t i = src_len; i > 0; i--) {
       PRINTF(dma, "%02x", (unsigned int)buffer[i-1]);
    }
    PRINTF(dma, "\n");
	}

    if ( (udma_ctx->active.desc.common.descSize == DESC_DESCTYPE_TYPE1) && udma_ctx->active.desc.type1.transform)
    {
        apply_transform(dma, &buffer[0], &buffer_transform[0], src_len);
        p = &buffer_transform[0];
    }

    if (dma->verbosity >= 5) {
    PRINTF(dma, "DMA %d: Tick %d: dma_store for desc=%08x va=%x pa=%llx len=%u data= ", dma->num,  udma_ctx->dma_tick_count, udma_ctx->active.va, dst_va,  (long long int)dst_pa, dst_len);
    for(int32_t i = dst_len; i > 0; i--) {
       PRINTF(dma, "%02x", (unsigned int)p[i-1]);
    }
    PRINTF(dma, "\n");
	}

    dma_data_write(dma, dst_pa, dst_len, p);

}





void dma_stop(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    PRINTF(dma, "DMA %d: Tick %d: dma_stop\n", dma->num, udma_ctx->dma_tick_count);

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_IDLE);
    udma_ctx->ext_status = DM0_STATUS_IDLE;
    udma_ctx->active.va = 0;
    udma_ctx->active.exception = DMA_DESC_NOT_DONE;
    udma_ctx->active.state = DMA_DESC_NOT_DONE;
    udma_ctx->target.va = 0;
}

static void update_descriptor(dma_t *dma, dma_decoded_descriptor_t * entry)
{
    udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;
    HEXAGON_DmaDescriptor_t *desc = (HEXAGON_DmaDescriptor_t*) &entry->desc;
    uint64_t pa = entry->pa;
    PRINTF(dma, "DMA %d: Tick %d: descriptor update for desc va=%x pa=%llx is_pause/exception=%d\n", dma->num, udma_ctx->dma_tick_count, entry->va, (long long int)pa, entry->pause || entry->exception);


    // Do not update the Next* ptr as that is updated through the rest of archsim

#if 0
    dma_data_write(dma, pa + 4, 4, (uint8_t *)&desc->word[1]);  //ptr->dstate_order_bypass_comp_desctype_length

    // Need to fix this in the testcase, hardware only updates src and dst on pause/exception not on done. weird.
#ifdef VERIFICATION
    if (entry->pause || entry->exception)
#endif
    {
        dma_data_write(dma, pa + 8,  4, (uint8_t *)&desc->common.srcAddress); //ptr->src
        dma_data_write(dma, pa + 12, 4, (uint8_t *)&desc->common.dstAddress); //ptr->dst
    }

    if (desc->common.descSize == DESC_DESCTYPE_TYPE1)
    {
        dma_data_write(dma, pa + 16, 4, (uint8_t *)&desc->word[4]);    //ptr->allocation_padding
        dma_data_write(dma, pa + 20, 4, (uint8_t *)&desc->word[5]);    //ptr->roiheight_roiwidth
        dma_data_write(dma, pa + 28, 4, (uint8_t *)&desc->word[7]);    //ptr->dstwidthoffset_srcwidthoffset
    }
#else
#define FIELD_SIZEOF(t, f) (sizeof(((t*)0)->f))
#define DMA_WRITE_FIELD(daddr, srcaddr, structure, field)  \
    dma_data_write(dma, \
      daddr + offsetof(structure,field), \
      FIELD_SIZEOF(structure, field), \
      (void *)&(((structure *)srcaddr)->field));

    DMA_WRITE_FIELD(pa, desc, dma_descriptor_type1_t, dstate_order_bypass_comp_desctype_length);
    // Need to fix this in the testcase, hardware only updates src and dst on pause/exception not on done. weird.
#ifdef VERIFICATION
    if (entry->pause || entry->exception)
#endif
    {
        DMA_WRITE_FIELD(pa, desc, dma_descriptor_type1_t, src);
        DMA_WRITE_FIELD(pa, desc, dma_descriptor_type1_t, dst);
    }

    if (desc->common.descSize == DESC_DESCTYPE_TYPE1)
    {
        DMA_WRITE_FIELD(pa, desc, dma_descriptor_type1_t, allocation_padding);
        DMA_WRITE_FIELD(pa, desc, dma_descriptor_type1_t, roiheight_roiwidth);
        DMA_WRITE_FIELD(pa, desc, dma_descriptor_type1_t, dstwidthoffset_srcwidthoffset);
    }
#endif
    dma_adapter_descriptor_end(dma, entry->id, entry->va, (uint32_t*)desc, entry->pause, entry->exception);
}



static uint64_t retrieve_descriptor(dma_t *dma, uint32_t desc_va, uint8_t *pdesc, uint32_t peek)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    uint32_t transfer_size = sizeof(dma_descriptor_type0_t);
    uint64_t pa = 0;
    uint32_t result = 1;
    //uint8_t *pdesc = (uint8_t *)&udma_ctx->active.desc;
    uint32_t mask = ((uint32_t)-1) & (~0xF);
    uint32_t xlate_va = desc_va & mask; // Force alignment to 16B for translation
    dma_memaccess_info_t dma_mem_access;
    HEXAGON_DmaDescriptor_t * desc = (HEXAGON_DmaDescriptor_t *) pdesc;
    result = dma_adapter_xlate_desc_va(dma, xlate_va, &pa, &dma_mem_access);
    PRINTF(dma, "DMA %d: Tick %d: descriptor retrieve va=%x access_rights U:%x R:%x W:%x X:%x pa=%llx result=%d \n", dma->num, udma_ctx->dma_tick_count, desc_va,  dma_mem_access.perm.u , dma_mem_access.perm.r , dma_mem_access.perm.w, dma_mem_access.perm.x, (long long int)pa, result );
    if (result == 0)
    {
        udma_ctx->exception_status = 1;
        if (check_dma_address(dma, xlate_va, &dma_mem_access.memtype, 1) == 0) {
            PRINTF(dma, "DMA %d: Tick %d: descriptor retrieve error memtype\n", dma->num, udma_ctx->dma_tick_count);
            return 0;
        }
        PRINTF(dma, "DMA %d: Tick %d: descriptor retrieve error\n", dma->num, udma_ctx->dma_tick_count);
        return 0;
    }
    else
    {
        PRINTF(dma, "DMA %d: Tick %d: data read pa=%llx size=%d\n", dma->num, udma_ctx->dma_tick_count, (long long int)pa, transfer_size);

        dma_data_read(dma, pa, transfer_size, pdesc);
        uint32_t dlbc_enabled = udma_ctx->dm2.dlbc_enable;
        uint32_t type = desc->common.descSize;
        uint32_t src_dlbc = desc->type0.srcDlbc;
        uint32_t dst_dlbc = desc->type0.dstDlbc;

        if (type == DESC_DESCTYPE_TYPE1) {

            if ((pa & 0x1F)) {
                set_dma_error(dma, xlate_va, DMA_CFG0_SYNDRONE_CODE___DESCRIPTOR_INVALID_ALIGNMENT, "DESCRIPTOR_INVALID_ALIGNMENT");
                return 0;
            }
            if (dlbc_enabled && (src_dlbc || dst_dlbc)) {
                set_dma_error(dma, xlate_va, DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_COMP_MODE, "UNSUPPORTED_COMP_MODE");
                return 0;
            }
            uint32_t previous_size = transfer_size;
            transfer_size = sizeof(dma_descriptor_type1_t);
            uint32_t remainder_size = transfer_size-previous_size;
            dma_data_read(dma, pa+previous_size, remainder_size, pdesc+previous_size);

            uint32_t roi_error = 0;
            uint32_t transform_mode = desc->type1.transform;
            uint32_t num = 1, den = 1;
            if ((transform_mode == 1) || (transform_mode == 2)) {
                roi_error = desc->type1.dstStride & 0x1;
            } else if ((transform_mode == 3) || (transform_mode == 4)) {
                roi_error = desc->type1.srcStride & 0x1;
            } else if (transform_mode == 5) {
                roi_error = (desc->type1.srcAddress & 0x1) || (desc->type1.dstAddress & 0x1);
            }
            roi_error |= desc->type1.width > desc->type1.srcStride;
            roi_error |= ((desc->type1.width * num) / den) > desc->type1.dstStride;
            if (roi_error) {
                PRINTF(dma, "DMA %d: Tick %d: ROI Error: transfrom: %d srcStride=%d dstStride=%d srcAddr=%x dstAddress=%x width=%d\n", dma->num, udma_ctx->dma_tick_count, transform_mode, desc->type1.srcStride, desc->type1.dstStride, desc->type1.srcAddress, desc->type1.dstAddress, desc->type1.width);
                dump_dma_desc(dma, (void*) pdesc, type , "retrieve but rot error");
                set_dma_error(dma, xlate_va, DMA_CFG0_SYNDRONE_CODE___DESCRIPTOR_ROI_ERROR, "DESCRIPTOR_ROI_ERROR");
                return 0;
            }

            PRINTF(dma, "DMA %d: Tick %d: reading 32-byte descriptor \n", dma->num, udma_ctx->dma_tick_count);

        } else if (type == DESC_DESCTYPE_TYPE0) {
            if ((pa & 0xF)) {
                set_dma_error(dma, xlate_va, DMA_CFG0_SYNDRONE_CODE___DESCRIPTOR_INVALID_ALIGNMENT, "DESCRIPTOR_INVALID_ALIGNMENT");
                return 0;
            }
            if (dlbc_enabled) {
                uint32_t dlbc_error = 0;
                uint32_t src_bypass = desc->type0.srcBypass;
                uint32_t dst_bypass = desc->type0.dstBypass;
                uint32_t src_aligned = desc->type0.srcAddress & 255;
                uint32_t dst_aligned = desc->type0.dstAddress & 255;
                uint32_t src_aligned_valid = src_aligned == 0;
                uint32_t dst_aligned_valid = dst_aligned == 0;
                uint32_t dlbc_length_valid = (desc->type0.length & 255) == 0;

                dlbc_error |=  src_dlbc && (!src_bypass || !dlbc_length_valid || !src_aligned_valid || !dst_aligned_valid); // DLBC Read needs source and dest to be aligned to 256 bytes and length == 256
                dlbc_error |=  dst_dlbc && (!dst_bypass || (src_aligned != dst_aligned)); // DLBC write needs alignment between source and dest and transfers of size 256

                if (dlbc_error) {
                    PRINTF(dma," dlbc error dlbc src=%d dst=%d bypass src=%d dst=%d src_aligned=%d dst_aligned=%d src_aligned_valid=%d dst_aligned_valid=%d length=%d valid=%d\n",src_dlbc,dst_dlbc, src_bypass, dst_bypass, src_aligned, dst_aligned, src_aligned_valid, dst_aligned_valid, get_dma_desc_length((void*)pdesc), dlbc_length_valid);
                    dump_dma_desc(dma, (void*) pdesc, type, "retrieve but invalid dlbc error");
                    set_dma_error(dma, xlate_va, DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_COMP_MODE, "UNSUPPORTED_COMP_MODE");
                    return 0;
                }
            }
        } else {
            dump_dma_desc(dma, (void*) pdesc, type , "retrieve but invalid");
            set_dma_error(dma, xlate_va, DMA_CFG0_SYNDRONE_CODE___DESCRIPTOR_INVALID_TYPE, "DESCRIPTOR_INVALID_TYPE");
            return 0;
        }
        if (peek) {
            dump_dma_desc(dma, (void*) pdesc, type, "retrieve by peek");
        } else {
            dump_dma_desc(dma, (void*) pdesc, type, "retrieve");
            #ifdef VERIFICATION
            dma_adapter_descriptor_start(dma, 0, desc_va, (uint32_t*)pdesc);
            #endif
        }
    }
    return pa;
}




static uint32_t start_dma(dma_t *dma, uint32_t new)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    /* Deal with (fetching) a new descriptor that given by "new".
       Two paths: 1) from the user side via dmstart, dmlink and dmresume, or
       2) from DMA chaining via \transfer_done(). */
    dump_dma_status(dma, "->start_dma new", new);
    udma_ctx->active.pa = retrieve_descriptor(dma, new, (uint8_t*)&udma_ctx->active.desc, 0);
    udma_ctx->exception_va = 0;

    if (!udma_ctx->active.pa)
    {
        PRINTF(dma, "DMA %d: Tick %d: <-start_dma exception @0x%08x\n", dma->num, udma_ctx->dma_tick_count, new);
        udma_ctx->exception_status = 1;
        udma_ctx->active.va = new;
        udma_ctx->active.exception = 1;
        // Record is we took a recoverable or unrecoverable exception
        udma_ctx->active.state = (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR)) ? DMA_DESC_EXCEPT_ERROR : DMA_DESC_EXCEPT_RUNNING ;
        udma_ctx->exception_va = new;
        return 0;
    }

    // The translation is ok; get the descriptor PA.
    HEXAGON_DmaDescriptor_t * desc = (HEXAGON_DmaDescriptor_t *) &udma_ctx->active.desc;
    udma_ctx->active.va = new;
    // Initialize condition
#ifdef VERIFICATION
    uint32_t padding_mode5 = 1;
#endif
    // Dump the descriptor we are about to process
    PRINTF(dma, "DMA %d: Tick %d: Descriptor addr = 0x%x\n", dma->num, udma_ctx->dma_tick_count, (unsigned int)udma_ctx->active.va);
    dump_dma_desc(dma, (void*)desc, udma_ctx->active.desc.common.descSize, __FUNCTION__);

    udma_ctx->active.va = new;
    udma_ctx->active.state = DMA_DESC_NOT_DONE;
    udma_ctx->active.pause = 0;
    udma_ctx->active.exception = 0;

    udma_ctx->active.padding_scale_factor_num = 1;
    udma_ctx->active.padding_scale_factor_den = 1;

    if (desc->common.descSize == DESC_DESCTYPE_TYPE0) {

        udma_ctx->active.bytes_to_read = desc->type0.length;
        udma_ctx->active.bytes_to_write = udma_ctx->active.bytes_to_read;

    } else if (udma_ctx->active.desc.common.descSize == DESC_DESCTYPE_TYPE1) {

        // THis needs to move the retrieve descriptor function
        uint32_t padding_mode = desc->type1.transform;
        dma_adapter_pmu_increment(dma, DMA_PMU_PADDING_DESCRIPTOR, (padding_mode != 0));
        if ((padding_mode == 1) || (padding_mode == 2))
        {
            udma_ctx->active.padding_scale_factor_num = 2;
        }
        else if ((padding_mode == 3) || (padding_mode == 4))
        {
            udma_ctx->active.padding_scale_factor_den = 2;
        }
        else if (padding_mode == 5)
        {
#ifdef VERIFICATION
            padding_mode5 = 2;
#endif
        }

        udma_ctx->active.src_roi_width         = desc->type1.width;
        udma_ctx->active.dst_roi_width         = (desc->type1.width * udma_ctx->active.padding_scale_factor_num) / udma_ctx->active.padding_scale_factor_den;
        udma_ctx->active.bytes_to_read         = udma_ctx->active.src_roi_width - desc->type1.srcWidthOffset;
        udma_ctx->active.bytes_to_write        = udma_ctx->active.dst_roi_width - desc->type1.dstWidthOffset;

        udma_ctx->active.num_lines_read  = desc->type1.height;
        udma_ctx->active.num_lines_write = desc->type1.height;
    }

    udma_ctx->active.max_transfer_size_read  = desc->common.srcBypass ? 256 : 128;
    udma_ctx->active.max_transfer_size_write = desc->common.dstBypass ? 256 : 128;


#ifdef VERIFICATION
    udma_ctx->active.max_transfer_size_write = 1 * udma_ctx->active.padding_scale_factor_num * padding_mode5;
    udma_ctx->active.max_transfer_size_read =  1 * udma_ctx->active.padding_scale_factor_den * padding_mode5;
#endif

    udma_ctx->active.desc.common.done = DESC_DSTATE_INCOMPLETE;
    if (udma_ctx->timing_on)
        udma_ctx->init_desc = udma_ctx->active;  // Copy for Timing model

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
    udma_ctx->ext_status = DM0_STATUS_RUN;

    dump_dma_status(dma, "<-start_dma", 0);

    return 1;
}
static void descriptor_done_callback(void * desc_void)
{
    desc_tracker_entry_t * entry = (desc_tracker_entry_t *) desc_void;

    // When we do the callback ....
    // Check if the descriptor was sent with a recoverable exception or not
    // If not, we assume it's done and we'll pop it from the work queue
    // On a recoverable exception, don't pop it from the queue in timing mode
    // Assume that the descriptor will restart on TLBW
    dma_t *dma = (dma_t *) entry->dma;
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    if (DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING) && (entry->desc.state == DMA_DESC_EXCEPT_RUNNING)) {
        PRINTF(dma, "DMA %d: Tick %d: callback descriptor=%x ended with exception\n", dma->num, udma_ctx->dma_tick_count, entry->desc.va);
    } else {
        entry->desc.state = DMA_DESC_DONE;
        PRINTF(dma, "DMA %d: Tick %d: callback descriptor=%x done pause=%d\n", dma->num, udma_ctx->dma_tick_count, entry->desc.va, entry->desc.pause);
    }
}




void dma_update_descriptor_done(dma_t *dma, dma_decoded_descriptor_t * entry)
{
	udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;

    HEXAGON_DmaDescriptor_t *desc = (HEXAGON_DmaDescriptor_t*) &entry->desc;
    if ((entry->state == DMA_DESC_DONE) && !entry->exception && !entry->pause) {
        desc->common.done = DESC_DSTATE_COMPLETE;
        dma_adapter_pmu_increment(dma, DMA_PMU_DESC_DONE, 1);
    }

    PRINTF(dma, "\nDMA %d: Tick %d: Descriptor addr = 0x%x Done=%d Pause=%d Exception=%d and Update\n", dma->num, udma_ctx->dma_tick_count, (unsigned int)entry->va, (int)entry->state, (int)entry->pause, (int)entry->exception);
    dump_dma_desc(dma, (void*)desc, desc->common.descSize, __FUNCTION__);
    update_descriptor(dma, entry);
}

static uint32_t dma_xfer_xlate(dma_t *dma, uint32_t src_va, uint32_t rd_size,  uint64_t * src_pa,  uint32_t dst_va,  uint32_t wr_size, uint64_t * dst_pa )
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    HEXAGON_DmaDescriptor_t *desc = (HEXAGON_DmaDescriptor_t*) &udma_ctx->active.desc;
    dma_memaccess_info_t dma_mem_access;

    if ((rd_size == 0) || (wr_size==0)) {
        PRINTF(dma, "DMA %d: Tick %d: one of rd_size=%d or wr_size=%d. probably dropping descriptor.\n", dma->num, udma_ctx->dma_tick_count,  rd_size, wr_size);
        return 1;
    }

    uint32_t result = dma_adapter_xlate_va(dma, src_va, src_pa, &dma_mem_access, rd_size, DMA_XLATE_TYPE_LOAD);

    if(!result) {
        udma_ctx->exception_status = 1;
        result = check_dma_address(dma, src_va, &dma_mem_access.memtype, result);
    } else {
        if ((dma_mem_access.memtype.vtcm || dma_mem_access.memtype.l2tcm) && desc->common.srcBypass) {
            result = set_dma_error(dma, src_va, DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_BYPASS_MODE, "UNSUPPORTED_BYPASS_MODE");
        }  else {
            result = dma_adapter_xlate_va(dma, dst_va, dst_pa, &dma_mem_access, wr_size, DMA_XLATE_TYPE_STORE);
            if (!result) {
                udma_ctx->exception_status = 1;
                result = check_dma_address(dma, dst_va, &dma_mem_access.memtype, result);
            } else {
                if ((dma_mem_access.memtype.vtcm || dma_mem_access.memtype.l2tcm) && desc->common.dstBypass) {
                    result = set_dma_error(dma, dst_va, DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_BYPASS_MODE, "UNSUPPORTED_BYPASS_MODE");
                }
            }
        }
    }
    return result;
}

static uint32_t dma_step_descriptor_chain(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    HEXAGON_DmaDescriptor_t *desc = (HEXAGON_DmaDescriptor_t*) &udma_ctx->active.desc;
    uint32_t result = 1;

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
    udma_ctx->ext_status = DM0_STATUS_RUN;
    udma_ctx->pause_va=0;
    udma_ctx->active.pause = 0;

    while (udma_ctx->desc_new != 0)
    {
        PRINTF(dma, "DMA %d: Tick %d: Trying to load Descriptor=%x restart=%d\n",   dma->num, udma_ctx->dma_tick_count, udma_ctx->desc_new, udma_ctx->desc_restart);
        if (start_dma(dma, udma_ctx->desc_new) == 0)
        {
            udma_ctx->pause_va = udma_ctx->desc_new;
            return 0;
        }
        PRINTF(dma, "DMA %d: Tick %d: New Descriptor=%x Load Successful\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->desc_new);
        udma_ctx->desc_new = 0;
        uint32_t desc_transfer_done = 0;
        while(!desc_transfer_done)
        {


            uint32_t desc_type = desc->common.descSize;
            uint64_t src_pa = 0;
            uint32_t src_va = (uint32_t)desc->common.srcAddress;
            uint64_t dst_pa = 0;
            uint32_t dst_va = (uint32_t)desc->common.dstAddress;

            uint32_t transfer_size_write = MIN(udma_ctx->active.bytes_to_write, get_transfer_size_to_aligned_va(dma, dst_va, udma_ctx->active.max_transfer_size_write));
            uint32_t transfer_size_read  = MIN(udma_ctx->active.bytes_to_read,  get_transfer_size_to_aligned_va(dma, src_va,udma_ctx->active.max_transfer_size_read));
            transfer_size_read = MIN(transfer_size_read, transfer_size_write);
            transfer_size_write = transfer_size_read;

            if (udma_ctx->exception_status)
                return 0;


            // TODO: Set max common transfer size


            if(desc_type == DESC_DESCTYPE_TYPE1) {

                uint32_t transform_mode = desc->type1.transform;

                if ((transform_mode == 1) || (transform_mode == 2))
                {
                    transfer_size_write =  2;
                }
                else if ((transform_mode == 3) || (transform_mode == 4))
                {
                    transfer_size_read =  2;
                }

                src_va += desc->type1.srcWidthOffset;
                dst_va += desc->type1.dstWidthOffset;

                if( desc->type1.height == 0) {
                    transfer_size_read = 0;
                    transfer_size_write = 0;
                }
                if (dma->verbosity >= 5) {
                PRINTF(dma, "DMA %d: Tick %d: 2D Desc Step: transform=%d bytes to read=%d write=%d rd_size=%d wr_size=%d\n", dma->num, udma_ctx->dma_tick_count,
                (int)transform_mode,(int) udma_ctx->active.bytes_to_read, (int)udma_ctx->active.bytes_to_write, (int)transfer_size_read, (int)transfer_size_write);
            }
            }

#ifdef VERIFICATION
            if (dma_target_desc_check(dma, &udma_ctx->active, &udma_ctx->target) && !dma_test_gen_mode(dma)) {
                PRINTF(dma, "Not doing linear step since we have reached target descriptor\n");
                udma_ctx->pause_va = udma_ctx->active.va;
                udma_ctx->active.pause = 1;
                desc_transfer_done = 1;
            }
#endif
            if (!udma_ctx->pause_va)
            {

                result = dma_xfer_xlate(dma, src_va, transfer_size_read,  &src_pa, dst_va, transfer_size_write, &dst_pa );
                if (result && transfer_size_read && transfer_size_write)
                {

                    dma_transfer(dma, src_va, src_pa, transfer_size_read, dst_va, dst_pa, transfer_size_write );

                    udma_ctx->active.bytes_to_write -= transfer_size_write;
                    udma_ctx->active.bytes_to_read  -= transfer_size_read;

                    // Update internal state of descriptor fields
                    if(desc_type == DESC_DESCTYPE_TYPE0)
                    {
                        desc->type0.length -= transfer_size_write;
                        desc->type0.srcAddress += transfer_size_read;
                        desc->type0.dstAddress += transfer_size_write;

                        // Update a descriptor accordingly.
						if (dma->verbosity >= 5) {
                        PRINTF(dma, "DMA %d: Tick %d: bytes to read=%d to write=%d\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->active.bytes_to_read, udma_ctx->active.bytes_to_write);
						}
                        desc_transfer_done = ((udma_ctx->active.bytes_to_read == 0) && (udma_ctx->active.bytes_to_write == 0));
                    }
                    else
                    {
                        desc->type1.srcWidthOffset += transfer_size_read;
                        desc->type1.dstWidthOffset += transfer_size_write;

                        if (desc->type1.srcWidthOffset == udma_ctx->active.src_roi_width)
                        {
                            desc->type1.srcWidthOffset = 0;

                            desc->type1.srcAddress += desc->type1.srcStride;
                            udma_ctx->active.bytes_to_read = udma_ctx->active.src_roi_width;
                            udma_ctx->active.num_lines_read--;
                        }


                        if (desc->type1.dstWidthOffset == udma_ctx->active.dst_roi_width)
                        {
                            desc->type1.dstWidthOffset = 0;
                            desc->type1.height -= 1;
                            desc->type1.dstAddress += desc->type1.dstStride;
                            udma_ctx->active.bytes_to_write = udma_ctx->active.dst_roi_width;
                            udma_ctx->active.num_lines_write--;
                        }

                        // Wait for dest writes to be completed before moving onto next src line
                        if (desc->type1.height == 0)
                        {
                            // done
                            desc->type1.srcWidthOffset = 0;
                            desc->type1.dstWidthOffset = 0;
                            desc->type1.width = 0;
                            desc->type1.height = 0;
                            desc_transfer_done = 1;
                        }

                    }

                }
                else if (result && (transfer_size_read==0) && (transfer_size_write==0)) {
                    desc_transfer_done = 1;
                }
            }
            if (udma_ctx->exception_status) {
                desc_transfer_done = 1;
                udma_ctx->active.exception = 1;
                udma_ctx->active.state = (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR)) ? DMA_DESC_EXCEPT_ERROR : DMA_DESC_EXCEPT_RUNNING;
                udma_ctx->exception_va = udma_ctx->active.va;
                PRINTF(dma, "DMA %d: Tick %d: Logging exception Desc VA=0x%x\n", dma->num, udma_ctx->dma_tick_count, (unsigned int)udma_ctx->exception_va );
                update_descriptor(dma, &udma_ctx->active);

            } else if (desc_transfer_done) {
                udma_ctx->active.exception = 0;
                udma_ctx->active.state = DMA_DESC_NOT_DONE;
                PRINTF(dma, "DMA %d: Tick %d: Logging done VA=0x%x pause=%d\n", dma->num, udma_ctx->dma_tick_count, (unsigned int)udma_ctx->active.va, (unsigned int)udma_ctx->active.pause );
            }
        }



        // This descriptor is done, put  it in the queue and log it to timing
        desc_tracker_entry_t * entry;
        if (udma_ctx->desc_restart) {
            udma_ctx->desc_restart = 0; // Restarted DMA already, don't need a new entry, just log it a
            entry = dma_adapter_peek_desc_queue_head(dma);
            entry->desc.exception = udma_ctx->active.exception;
            entry->desc.state = udma_ctx->active.state; // Update state, it may have finished or excepted again
            entry->desc.id =  entry->id;
            entry->desc.pause = udma_ctx->active.pause;
            PRINTF(dma, "DMA %d: Tick %d: Setting callback for restart VA=0x%x with pause=%d\n", dma->num, udma_ctx->dma_tick_count, (unsigned int)udma_ctx->active.va, entry->desc.pause );
            udma_ctx->active.id =  entry->id;
        } else {
            entry = dma_adapter_insert_desc_queue(dma, &udma_ctx->active );
            entry->desc.id =  entry->id;
            entry->desc.pause = udma_ctx->active.pause;
            udma_ctx->active.id =  entry->id;
            entry->callback = descriptor_done_callback;
            PRINTF(dma, "DMA %d: Tick %d: Setting callback for done VA=0x%x with pause=%d\n", dma->num, udma_ctx->dma_tick_count, (unsigned int)udma_ctx->active.va, entry->desc.pause );
        }
#ifndef VERIFICATION
        dma_adapter_descriptor_start(dma, entry->desc.id, udma_ctx->active.va, (uint32_t*)desc);
#endif
        if (udma_ctx->timing_on) {
            entry->init_desc = udma_ctx->init_desc;
            PRINTF(dma, "DMA %d: Tick %d: Logging Descriptor VA=0x%x ID=%lli to timing model state=%d\n", dma->num, udma_ctx->dma_tick_count, (unsigned int)udma_ctx->active.va, entry->id, entry->desc.state );
#ifdef FAKE_TIMING
            // Log to timing
            dma_adapter_insert_to_timing(dma, entry );
#endif
        } else {
            entry->callback((void*) entry); // Callback to indicate that the descriptor is done
        }

        if (!udma_ctx->exception_status && !udma_ctx->pause_va) {  // Not in exception mode
            // Chaining of a next descriptor.
            uint32_t next_descriptor = desc->common.nextDescPointer;
            if (next_descriptor != 0)
            {
                udma_ctx->active.va = next_descriptor;
                udma_ctx->desc_new = next_descriptor;
            } else {
               PRINTF(dma, "DMA %d: Tick %d: End of Descriptor chain\n", dma->num, udma_ctx->dma_tick_count );
            }
        }
    }
    return 1;
}

void dma_arch_tlbw(dma_t *dma, uint32_t jtlb_idx) {
#if 0
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dma_tlbw(udma_ctx->dpuP, jtlb_idx);
#endif
}
void dma_arch_tlbinvasid(dma_t *dma) {
#if 0
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dma_tlbinvasid(udma_ctx->dpuP);
#endif
}


uint32_t dma_get_tick_count(dma_t *dma) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    return udma_ctx->dma_tick_count;
}

uint32_t dma_retry_after_exception(dma_t *dma)
{

    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    dump_dma_status(dma, "->dma_retry_after_exception", udma_ctx->exception_status);

    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_IDLE))
        return 0;

    // Check if we are running and have already reported an exception
    if ((DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING)) && (udma_ctx->exception_status == 0)) {
        // We should be in running mode and without exceptions
        desc_tracker_entry_t * entry = dma_adapter_peek_desc_queue_head(dma);

        // Only restart a DMA that was in exception,
        // If it was a good descriptor, it should have been flushed and done
        if (entry && entry->desc.exception && (entry->desc.state != DMA_DESC_NOT_DONE)) {
            udma_ctx->desc_new = entry->desc.va;
        udma_ctx->desc_restart = 1;

#ifndef VERIFICATION
            PRINTF(dma, "DMA %d: Tick %d: Trying restart after TLBW/Exception DESC=%x\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->desc_new );
        dma_tick(dma, 1);
 #else
            PRINTF(dma, "DMA %d: Tick %d: Setting restart after TLBW/Exception DESC=%x but not actually ticking\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->desc_new );
#endif
        }

    }


    return 1;
}

uint32_t dma_tick(dma_t *dma, uint32_t do_step)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    uint32_t stall_check_guest   = !udma_ctx->dm2.no_stall_guest && dma_adapter_in_guest_mode(dma);
    uint32_t stall_check_monitor = !udma_ctx->dm2.no_stall_monitor && dma_adapter_in_monitor_mode(dma);
    uint32_t stall_check_debug   = !udma_ctx->dm2.no_cont_debug && dma_adapter_in_debug_mode(dma);
    udma_ctx->dma_tick_count  = dma_adapter_get_pcycle(dma);


    stall_check_guest = 0;
    stall_check_monitor = 0;
    stall_check_debug = 0;

	if ((DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING)) && (udma_ctx->exception_status == 0) && do_step)
        {

		if (stall_check_guest || stall_check_monitor || stall_check_debug) {
			PRINTF(dma, "Requesting Stall: DMA DM2=%08x ", udma_ctx->dm2.val);
			PRINTF(dma, "stall due to guest = %d monitor = %d debug = %d\n", stall_check_guest, stall_check_monitor, stall_check_debug);
		}
		else
		{

            dma_step_descriptor_chain(dma);
        }

    }


    dma_adapter_pop_desc_queue_done(dma);

#if 0
    if (udma_ctx->timing_on) {  // This goes away with real timing mode
#ifdef FAKE_TIMING
        dma_adapter_pop_from_timing(dma );
#else
		dma_dpu_inst_t *dpuP = udma_ctx->dpuP;
		dma_dpu_tick(dpuP);
#endif
    }
#endif

    return 1;
}

static uint32_t dma_cmd_insn_timer_checker(dma_t *dma)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    if (--udma_ctx->insn_timer == 0)  {
        udma_ctx->insn_timer_active = INSN_TIMER_EXPIRED;
        dma_adapter_pmu_increment(dma, udma_ctx->insn_timer_pmu, 1);
        PRINTF(dma, "DMA %d: Tick %d: insn latency expired\n", dma->num, udma_ctx->dma_tick_count);
        return 1;
    }
    PRINTF(dma, "DMA %d: Tick %d: insn latency timer=%d\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->insn_timer);
    return 0;
}

static inline uint32_t
dma_instruction_latency(dma_t *dma, dma_cmd_report_t *report, uint32_t latency, uint32_t pmu_num) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    if (udma_ctx->timing_on == 1) {
        if (udma_ctx->insn_timer_active == INSN_TIMER_IDLE) {
            report->insn_checker = &dma_cmd_insn_timer_checker;
            udma_ctx->insn_timer_active = INSN_TIMER_ACTIVE;
            udma_ctx->insn_timer = dma_adapter_get_insn_latency(dma, latency); // get latency
            udma_ctx->insn_timer_pmu = pmu_num;
            PRINTF(dma, "DMA %d: Tick %d: setting insn latency timer=%d\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->insn_timer);
            return 1;
        } else if (udma_ctx->insn_timer_active == INSN_TIMER_EXPIRED) {
            return 0;
        } else {
            return 1;
        }
    }
    return 0;
}



void dma_cmd_start(dma_t *dma, uint32_t new, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    dump_dma_status(dma, "->dma_cmd_start new", new);

    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        return;
    }

    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING))
    {
        set_dma_error(dma, new, DMA_CFG0_SYNDRONE_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "DMSTART_DMRESUME_IN_RUNSTATE");
        return;
    }

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
    udma_ctx->ext_status = DM0_STATUS_RUN;
    udma_ctx->active.va = new;
    udma_ctx->active.pc = dma->pc;
    udma_ctx->desc_new = new;
    udma_ctx->desc_restart = 0;
    dma_adapter_pmu_increment(dma, DMA_PMU_CMD_START, 1);

#ifndef VERIFICATION
    dma_tick(dma, 1);
#endif

   dump_dma_status(dma, "<-dma_cmd_start", 0);
}

void dma_cmd_link(dma_t *dma, uint32_t cur, uint32_t new, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    /* Roles: chain a new descriptor, and start DMA if it can. */
    dump_dma_status(dma, "->dma_cmd_link cur=", cur);
    dump_dma_status(dma, "->dma_cmd_link new=", new);

    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        return;
    }

    /* Try to chain or start. */
    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING) || DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED))
    {
        if (cur == 0)  /* stop */
        {
            PRINTF(dma, "DMA %d: Tick %d: %s cur == 0: \n", dma->num, udma_ctx->dma_tick_count, __FUNCTION__);
            /* If a current tail to chain a new descriptor is given to NULL,
                that means we will start a totally new descriptor chain
                from the beginning. */
            dma_stop(dma);
        }
        else  /* continue running or resume*/
        {
            // Check tail translation
            uint64_t pa;
            dma_memaccess_info_t dma_mem_access;
            if(dma_adapter_xlate_desc_va(dma, cur, &pa, &dma_mem_access)==0) {
                udma_ctx->exception_status = 1;
                PRINTF(dma, "DMA %d: Tick %d: %s Desc Retrieve Exception Encountered at dmlink : VA=0x%x PA=%llx\n", dma->num, udma_ctx->dma_tick_count, __FUNCTION__, (unsigned int)cur, (long long int)pa);
                check_dma_address(dma, 0, &dma_mem_access.memtype, 1);
                return;
            }

            if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED)) {
                // If paused set to run
                DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
                udma_ctx->ext_status = DM0_STATUS_RUN;
                PRINTF(dma, "DMA %d: Tick %d: dma_cmd_link resuming dma\n", dma->num, udma_ctx->dma_tick_count);
                if (new != 0)
                {

                    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
                    udma_ctx->ext_status = DM0_STATUS_RUN;
                    udma_ctx->active.va = new;
                    udma_ctx->desc_new = new;
                    udma_ctx->desc_restart = 0;
                    PRINTF(dma, "DMA %d: Tick %d: %s new != 0: active desc va = %x desc_nex = %x\n", dma->num, udma_ctx->dma_tick_count, __FUNCTION__, (unsigned int)udma_ctx->active.va, (unsigned int)udma_ctx->desc_new );
#ifndef VERIFICATION
                    dma_tick(dma, 1);
#endif
                }
            }
#ifndef VERIFICATION
            else if ( (new != 0) && DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING)) {
                udma_ctx->active.va = new;
                udma_ctx->active.pc = dma->pc;
                udma_ctx->desc_new = new;
                PRINTF(dma, "DMA %d: Tick %d: %s new != 0: active desc va = %x desc_nex = %x\n", dma->num, udma_ctx->dma_tick_count, __FUNCTION__, (unsigned int)udma_ctx->active.va, (unsigned int)udma_ctx->desc_new );
                dma_tick(dma, 1);
            }
#endif




            dump_dma_status(dma, "<-dma_cmd_link", 0);
            dma_adapter_pmu_increment(dma, DMA_PMU_CMD_LINK, 1);
            return;
        }
    }
    /* If we reach here, this is our very first descriptor to process. */
    if (new != 0)
    {
        if (cur != 0) {
            uint64_t pa;
            dma_memaccess_info_t dma_mem_access;
            if(dma_adapter_xlate_desc_va(dma, cur, &pa, &dma_mem_access)==0) {
                udma_ctx->exception_status = 1;
                PRINTF(dma, "DMA %d: Tick %d: %s Desc Retrieve Exception Encountered at dmlink : VA=0x%x PA=%llx\n", dma->num, udma_ctx->dma_tick_count, __FUNCTION__, (unsigned int)cur, (long long int)pa);
                check_dma_address(dma, 0, &dma_mem_access.memtype, 1);
                return;
            }
        }

        DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
        udma_ctx->ext_status = DM0_STATUS_RUN;
        udma_ctx->active.va = new;
        udma_ctx->active.pc = dma->pc;
        udma_ctx->desc_new = new;
        PRINTF(dma, "DMA %d: Tick %d: %s new != 0: active.va = %x desc_nex = %x\n", dma->num, udma_ctx->dma_tick_count, __FUNCTION__, (unsigned int)udma_ctx->active.va, (unsigned int)udma_ctx->desc_new );

    }
    dma_adapter_pmu_increment(dma, DMA_PMU_CMD_LINK, 1);
    dump_dma_status(dma, "<-dma_cmd_link", 0);
#ifndef VERIFICATION
    dma_tick(dma, 1);
#endif
}




void dma_cmd_poll(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dump_dma_status(dma, "->dma_cmd_poll exception_status", udma_ctx->exception_status);
    /* If an exception happened already, we have to make an owner thread  take it. */

    if (dma_instruction_latency(dma, report, DMA_INSN_LATENCY_DMPOLL, DMA_PMU_DMPOLL_CYCLES))
        return;

    uint32_t dm0 = dma_adapter_peek_desc_queue_head_va(dma);
    if (udma_ctx->exception_status)
    {
        /* Clear the DMA exception for a next ticking. */
        udma_ctx->exception_status = 0;
        dm0 = udma_ctx->exception_va;
        (*report).excpt = 1;
        dump_dma_status(dma, "->dma_cmd_poll report exception", udma_ctx->exception_status);
    }
    /* Return value of dmpoll instruction. */

    (*ret) = dm0 | udma_ctx->ext_status;    // Need head of Queue
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    dump_dma_status(dma, "<-dma_cmd_poll dm0", (*ret));
}

static uint32_t dma_cmd_wait_checker(dma_t *dma)
{
    /* This function will be used as a stall-breaker for dmwait instruction.
       This function checks if DMA tasks of a current thread is done or not.
     While the DMA engine working, if an exception happened, we should stop
     stalling immediately so that DMWAIT or DMPOLL can be processed
     by the simulator's thread cycle. While this staller is ON,
     the thread cycle will be skipped, although the DMA cycle will keep
     working on. */
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;


    PRINTF(dma, "DMA %d: Tick %d: ->dma_cmd_wait_checker: current_status=%d exception_status=%d\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->ext_status, udma_ctx->exception_status);
    dma_adapter_pmu_increment(dma, DMA_PMU_DMWAIT_CYCLES, 1);
    return ((udma_ctx->ext_status == DM0_STATUS_IDLE) || (udma_ctx->exception_status));
}

void dma_cmd_wait(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dump_dma_status(dma, "->dma_cmd_wait", 0);

    uint32_t dm0 = 0;
#ifdef VERIFICATION
    dma_tick(dma, 1);
#endif

    if (dma_instruction_latency(dma, report, DMA_INSN_LATENCY_DMWAIT, DMA_PMU_DMWAIT_CYCLES))
        return;

    /* Take a pending exception. Because of this report, later we will
        revisit "dmwait" instruction again by replying. */
    if (udma_ctx->exception_status)
    {
        /* Clear the DMA exception to move forward. */

        PRINTF(dma, "DMA %d: Tick %d: clear exception and set report and update descriptor on dmwait exception va=%x\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->exception_va);
        udma_ctx->exception_status = 0;
        (*report).excpt = 1;
        dm0 = udma_ctx->exception_va;
    }
    else
    {
        /* When we reach here after an exception is resolved,
            we expect "RUNNING". */
        if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING))
        {
            /* We will wait for the entire task completed. This checker will see
                if the task is done, or an exception occurs. This will make
                the adapter automatically set up a staller on behalf of the engine.
            */
            (*report).insn_checker = &dma_cmd_wait_checker;

            PRINTF(dma, "DMA %d: Tick %d: dmwait set up a staller\n", dma->num, udma_ctx->dma_tick_count);
        } else if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_ERROR)) {
            if (udma_ctx->dm2.error_exception_enable)
            {
                //udma_ctx->exception_status = 1;
                (*report).excpt = 1;
            }
        }
        else
        {
            PRINTF(dma, "DMA %d: Tick %d: %s\n", dma->num, udma_ctx->dma_tick_count, __FUNCTION__);
        }
    }


    if(udma_ctx->target.va != 0) {
        PRINTF(dma, "DMA %d: Tick %d: clearing target descriptor=%x on wait\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->target.va);
        udma_ctx->target.va = 0;
    }

    /* Return value of dmwait instruction. */
    (*ret) = dm0 | udma_ctx->ext_status;
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    dump_dma_status(dma, "<-dma_cmd_wait dm0", (*ret));
}

static uint32_t dma_cmd_waitdescriptor_checker(dma_t *dma)
{
    /* This function will be used as a stall-breaker for dmwait instruction.
       This function checks if DMA tasks of a current thread is done or not.
     While the DMA engine working, if an exception happened, we should stop
     stalling immediately so that DMWAIT or DMPOLL can be processed
     by the simulator's thread cycle. While this staller is ON,
     the thread cycle will be skipped, although the DMA cycle will keep
     working on. */
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    return (udma_ctx->active.va == udma_ctx->target.va);
}

void dma_cmd_waitdescriptor(dma_t *dma, uint32_t desc, uint32_t *ret, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    dump_dma_status(dma, "->dma_cmd_waitdescriptor", 0);

    /* Take a pending exception. Because of this report, later we will
       revisit "dmwait" instruction again by replying. */
    if (udma_ctx->exception_status)
    {
        /* Clear the DMA exception to move forward. */
        udma_ctx->exception_status = 0;

        (*report).excpt = 1;
    }
    else
    {
        /* When we reach here after an exception is resolved,
           we expect "RUNNING". */
        if (udma_ctx->active.va != udma_ctx->target.va)
        {
            /* We will wait for the entire task completed. This checker will see
               if the task is done, or an exception occurs. This will make
               the adapter automatically set up a staller on behalf of the engine.
            */
            (*report).insn_checker = &dma_cmd_waitdescriptor_checker;
            PRINTF(dma, "DMA %d: Tick %d: dmwaitdescriptor set up a staller\n", dma->num, udma_ctx->dma_tick_count);
        }
    }

    /* Return value of dmwaitdescriptor instruction. */
    (*ret) =  udma_ctx->active.desc.word[1];
    dump_dma_status(dma, "<-dma_cmd_waitdescriptor ret", *ret);
}

uint32_t dma_in_error (dma_t *dma) {
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    return  (udma_ctx->ext_status == DM0_STATUS_ERROR) || (udma_ctx->exception_status);
}
static uint32_t dma_cmd_pause_checker(dma_t *dma)
{
    udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;
    PRINTF(dma, "DMA %d: Tick %d: ->dma_cmd_pause_checker\n", dma->num, udma_ctx->dma_tick_count);
    dma_adapter_pmu_increment(dma, DMA_PMU_PAUSE_CYCLES, 1);
    return dma_adapter_desc_queue_empty(dma);
}


void dma_cmd_pause(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dump_dma_status(dma, "->dma_cmd_pause", 0);
    uint32_t active_desc_va = udma_ctx->active.va;
    uint32_t status = udma_ctx->ext_status;

    if (dma_instruction_latency(dma, report, DMA_INSN_LATENCY_DMPAUSE, DMA_PMU_PAUSE_CYCLES))
        return;

    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        dma_stop(dma);   // Clear error state;
        udma_ctx->exception_status = 0; // Clear exception status
    }

    /* If the engine is RUNNING, make it PAUSED. */
    if ( DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING) )
    {

#ifdef VERIFICATION
        dma_tick(dma, 1);   // Tick until done or reached target descriptor
        active_desc_va = (udma_ctx->exception_status) ? udma_ctx->exception_va : udma_ctx->pause_va;   // Current active descriptor
#endif

        PRINTF(dma, "DMA %d: Tick %d: udma_ctx->active.va=%x active_desc_va=%x (pause_va=%x exception_va=%x exception_status=%d)\n", dma->num, udma_ctx->dma_tick_count, udma_ctx->active.va, active_desc_va, udma_ctx->pause_va, udma_ctx->exception_va, udma_ctx->exception_status );
        DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_PAUSED);
        udma_ctx->ext_status = DM0_STATUS_IDLE;
        udma_ctx->exception_status = 0; // Clear exception status
        // If not 0, it means that the new descriptor has not been loaded yet so no updating required

        // Flush buffers, short cutting dmpause. We're going to force everything immediately to completion in the timing model
        if (udma_ctx->timing_on) {
#ifdef FAKE_TIMING
            dma_adapter_flush_timing(dma);
#endif
            (*report).insn_checker = &dma_cmd_pause_checker;
            status = DM0_STATUS_IDLE;
        }
        if ((udma_ctx->pause_va==0) && (udma_ctx->active.va==0))
            status = 0;

        udma_ctx->pause_va = 0;
        udma_ctx->active.va  = 0; // Transition to idle, no active descritor
        udma_ctx->desc_new = 0;
        udma_ctx->desc_restart = 0;
    }

    (*ret) =  active_desc_va  | status;
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    dump_dma_status(dma, "<-dma_cmd_pause ret", active_desc_va | status);
}

void dma_cmd_resume(dma_t *dma, uint32_t ptr, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    dump_dma_status(dma, "->dma_cmd_resume ptr", ptr);

    if (dma_instruction_latency(dma, report, DMA_INSN_LATENCY_DMRESUME, DMA_PMU_NUM))
        return;

    if (udma_ctx->ext_status == DM0_STATUS_ERROR)
    {
        return;
    }

    if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_RUNNING))
    {
        set_dma_error(dma, ptr, DMA_CFG0_SYNDRONE_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "DMSTART_DMRESUME_IN_RUNSTATE");
        return;
    }
    else
    {
        /* We do not expect any exceptions from the monitor mode commands - pause
            and resume. */

        /* If the engine is PAUSED, clear it, and resume (restart). But don't
            take any exception. */
        if (DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_PAUSED) || DMA_STATUS_INT(udma_ctx, DMA_STATUS_INT_IDLE))
        {
            if (ptr == 0)
            {
                dma_stop(dma);
            }
            else
            {
                udma_ctx->desc_restart = 0;
                if((ptr & 0x3) == 0x2)
                {   // Resuming error state
                    udma_ctx->active.va = ptr & ~0xF;
                    udma_ctx->desc_new = ptr & ~0xF;
                    set_dma_error(dma, 0, DMA_CFG0_SYNDRONE_CODE___DMSTART_DMRESUME_IN_RUNSTATE, "DMSTART_DMRESUME_IN_RUNSTATE");
                    PRINTF(dma, "DMA %d: Tick %d: dma_cmd_resume: resuming error state descriptor=%x\n", dma->num, udma_ctx->dma_tick_count, ptr & ~0xF);
                }
                else
                {
                    PRINTF(dma, "DMA %d: Tick %d: dma_cmd_resume: setting descriptor=%x\n", dma->num, udma_ctx->dma_tick_count, ptr & ~0xF);
                    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_RUNNING);
                    udma_ctx->ext_status = DM0_STATUS_RUN;
                    udma_ctx->active.va = ptr & ~0xF;
                    udma_ctx->desc_new = ptr & ~0xF;
#ifndef VERIFICATION
                    dma_tick(dma, 1);
#endif
                }
            }
        }
        dma_adapter_pmu_increment(dma, DMA_PMU_CMD_RESUME, 1);
    }
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;
    dump_dma_status(dma, "<-dma_cmd_resume",0);
}

uint32_t dma_set_timing(dma_t *dma, uint32_t timing_on)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    udma_ctx->timing_on = timing_on;
    return 1;
}

uint32_t dma_init(dma_t *dma, uint32_t timing_on)
{
    udma_ctx_t *udma_ctx;
    if ((udma_ctx = calloc(1, sizeof(udma_ctx_t))) == NULL)
    {
        return -1;
    }
    dma->udma_ctx = (void *)udma_ctx;
    udma_ctx->timing_on = timing_on;

    DMA_STATUS_INT_SET(udma_ctx, DMA_STATUS_INT_IDLE);
    udma_ctx->ext_status = DM0_STATUS_IDLE;
    udma_ctx->exception_status = 0;
    udma_ctx->insn_timer_active = INSN_TIMER_IDLE;

    udma_ctx->target.va = 0;

    //Initialize DM2
    udma_ctx->dm2.val = 0;

    udma_ctx->dm2.no_stall_guest = 0;
    udma_ctx->dm2.no_stall_monitor = 0;
    udma_ctx->dm2.no_cont_debug = 0;
	udma_ctx->dm2.priority = 1;					// Inherits the thread?s priority
    udma_ctx->dm2.dlbc_enable = 1;
    udma_ctx->dm2.error_exception_enable = 1;

#if 0
	// TODO: this should be a parm or ideally design parameter (config table) w/ parm override
	const unsigned int dma_dpu_prefetch_depth = 16;

	dma->verbosity = dma_adapter_debug_verbosity(dma);

	// WARNING: The timing model needs to be initialized regardless of timing_on
	// If running with --fastforward, timing_on may turn on later on during the run
	udma_ctx->dpuP =  dma_dpu_init(dma, dma_dpu_prefetch_depth);
	if (udma_ctx->dpuP == NULL)
	{
		return -1;
	}
#endif
    return 1;
}

uint32_t dma_free(dma_t *dma)
{
    udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;
#if 0
	dma_dpu_inst_t *dpuP = udma_ctx->dpuP;
	if (dpuP) {	// timing mode only
		dma_dpu_free(dpuP);
	}
#endif

    if (dma->udma_ctx != NULL)
    {
        free(dma->udma_ctx);
        dma->udma_ctx = NULL;
    }

    return 1;
}

void dma_cmd_cfgrd(dma_t *dma, uint32_t index, uint32_t *val, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;

    PRINTF(dma, "DMA %d: Tick %d: ->dma_cmd_cfgrd: Index=%d\n", dma->num, udma_ctx->dma_tick_count, index);

    (*val) = 0;
    switch (index)
    {
        case 0:
            (*val) = udma_ctx->active.va | udma_ctx->ext_status;
            break;

        case 2:
            (*val) = udma_ctx->dm2.val;
            break;
        case 4:
            (*val) = udma_ctx->error_state_reason_captured |
                     dma->num << 4 |
                     udma_ctx->error_state_reason;
            break;

        case 5:
            (*val) = udma_ctx->error_state_address;
            break;

        case 1: // Reserved
        case 3: // Reserved
        default: // Out of range
            break;
    }
}

void dma_cmd_cfgwr(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report)
{
    udma_ctx_t *udma_ctx = (udma_ctx_t *)dma->udma_ctx;
    PRINTF(dma, "DMA %d: Tick %d: ->dma_cmd_cfgwr: Index=%d Val=%0x\n", dma->num, udma_ctx->dma_tick_count, index, val);

    switch (index)
    {
        case 4:
            if ((val & 0x1) == 0)
            {
                udma_ctx->error_state_reason_captured   = 0;
                udma_ctx->error_state_reason            = 0;
                udma_ctx->error_state_address           = 0;
            }
            break;

        case 2:
            udma_ctx->dm2.val = val;
            PRINTF(dma, "DMA %d: Tick %d: ->dm2 updated = %0x\n", dma->num, udma_ctx->dma_tick_count,  udma_ctx->dm2.val);
            break;

        case 0: // Not applicable
        case 1: // Reserved
        case 3: // Reserved
            break;
        case 5:
             udma_ctx->error_state_address = val;
             PRINTF(dma, "DMA %d: Tick %d: ->error_state_address updated = %0x\n", dma->num, udma_ctx->dma_tick_count,  udma_ctx->error_state_address);
        default: // Out of range
            break;
    }

}

void dma_cmd_syncht(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report)
{
// Nothing to do
    udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;
    PRINTF(dma, "DMA %d: Tick %d: ->dma_cmd_syncht: Index=%d Val=%0x\n", dma->num, udma_ctx->dma_tick_count, index, val);
}
void dma_cmd_tlbsynch(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report)
{
// Nothing to do
    udma_ctx_t *udma_ctx __attribute__((unused)) = (udma_ctx_t *)dma->udma_ctx;
    PRINTF(dma, "DMA %d: Tick %d: ->dma_cmd_tlbsynch: Index=%d Val=%0x \n", dma->num, udma_ctx->dma_tick_count, index, val);

}

