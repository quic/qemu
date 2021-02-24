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
 */

#ifndef _DMA_H_
#define _DMA_H_

#include <stdint.h>
#include "dma_descriptor.h"
/* Function pointer type to let us know if an instruction finishes its job. */
typedef struct dma_state dma_t;	// forward declaration
typedef uint32_t (*dma_insn_checker_ptr)(struct dma_state *dma);
#if 0
#include "uarch_dma.h"
#endif

#define UDMA_STANDALONE_MODEL 1
#define DMA_STATUS_INT_IDLE       0x0000
#define DMA_STATUS_INT_RUNNING    0x0001
#define DMA_STATUS_INT_PAUSED     0x0002
#define DMA_STATUS_INT_ERROR      0x0004

#define DMA_STATUS_INT(DMA, STATE) (((DMA)->int_status) == STATE)
#define DMA_STATUS_INT_SET(DMA, STATE) (((DMA)->int_status = STATE))

#define DMA_MAX_TRANSFER_SIZE       256
#ifdef VERIFICATION
#define DMA_BUF_SIZE                2
#else
#define DMA_BUF_SIZE                128*256
#endif

#define DMA_MAX_DESC_SIZE           32

#define DMA_CFG0_SYNDRONE_CODE___DMSTART_DMRESUME_IN_RUNSTATE   0
#define DMA_CFG0_SYNDRONE_CODE___DESCRIPTOR_INVALID_ALIGNMENT   1
#define DMA_CFG0_SYNDRONE_CODE___DESCRIPTOR_INVALID_TYPE        2
#define DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_ADDRESS            3
#define DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_BYPASS_MODE        4
#define DMA_CFG0_SYNDRONE_CODE___UNSUPPORTED_COMP_MODE          5
#define DMA_CFG0_SYNDRONE_CODE___DESCRIPTOR_ROI_ERROR           6
#define DMA_CFG0_SYNDRONE_CODE___INVALID_ACCESS_RIGHTS          102
#define DMA_CFG0_SYNDRONE_CODE___DATA_TIMEOUT                   103
#define DMA_CFG0_SYNDRONE_CODE___DATA_ABORT                     104


#define INSN_TIMER_IDLE             0
#define INSN_TIMER_ACTIVE           1
#define INSN_TIMER_EXPIRED          2
#define INSN_TIMER_NOTUSED          3

#define INSN_TIMER_LATENCY_DMA_CMD_POLL     20
#define INSN_TIMER_LATENCY_DMA_CMD_WAIT     24
#define INSN_TIMER_LATENCY_DMA_CMD_START    20
#define INSN_TIMER_LATENCY_DMA_CMD_LINK     20
#define INSN_TIMER_LATENCY_DMA_CMD_PAUSE    20
#define INSN_TIMER_LATENCY_DMA_CMD_RESUME   20

#define DMA_MEM_TYPE_L2             0x00000000
#define DMA_MEM_TYPE_VTCM           0x00000001

#define DMA_MEM_PERMISSION_USER     0x00000001
#define DMA_MEM_PERMISSION_WRITE    0x00000002
#define DMA_MEM_PERMISSION_READ     0x00000004
#define DMA_MEM_PERMISSION_EXECUTE  0x00000008

#define DMA_XLATE_TYPE_LOAD         0x00000000
#define DMA_XLATE_TYPE_STORE        0x00000001

typedef enum {
  DMA_DESC_STATE_START=0,
  DMA_DESC_STATE_DONE=1,
  DMA_DESC_STATE_PAUSE=2,
  DMA_DESC_STATE_EXCEPT=3,
  DMA_DESC_STATE_NUM=4
} dma_callback_state_t;

typedef struct dma_state {
    int num;                          // DMA instance identification index (num).
    int error_state_reason;
    uint32_t pc;                      /* current PC for debug */
    void *udma_ctx;                   // User DMA Context Structure
    void * owner;
	int verbosity;					// 0-9;
} dma_t;

struct dma_mem_xact;
struct dma_mem_xact_entry;
/* Function pointer type for DMA timing call-backs from memory subsystems. */
typedef dma_t* (*dma_timing_cb_t)(struct dma_mem_xact*);

/* One piece of the user-DMA memory transaction */
typedef int descFifoEntry_t;
typedef struct dma_mem_xact
{
  dma_t* dma;                /* Requester DMA instance */
  descFifoEntry_t *entryP;	///< descriptor fifo entry ptr. Only non-null for src read/dst write xactions.
  uint64_t len;          /* Length (#bytes) of a memory transaction */
  uint32_t vaddr;          /* VA of a memory transaction */
  uint64_t paddr;          /* PA of a memory transaction */
  uint32_t is_read:1;      /* Flag to indicate read/write */
  uint32_t is_bypass:1;    /* Flag to indicate L2 bypass path */
  uint32_t is_desc:1;       /* Flag to indicate descriptor/memory transfer transaction*/
  uint32_t pc;              /* PC of DMSTART/DMLINK */
  /* General-purpose data to communicate between a memory subsystem and
     the engine if there is. */
  void* ptr;

  /* Call-back from the memory subsystem to the engine. When a memory
     transaction is completed, this function will be called by the memory
     subsystem. */
  dma_timing_cb_t callback;
} dma_mem_xact;

typedef struct dma_mem_xact_entry
{
    void* prev;
    dma_mem_xact entry;
    void* next;
} dma_mem_xact_entry;

typedef struct dm2_reg_t {
  union {
    struct {
      uint32_t no_stall_guest:1;
      uint32_t no_stall_monitor:1;
      uint32_t reserved_0:1;
      uint32_t no_cont_except:1;
      uint32_t no_cont_debug:1;
      uint32_t priority:2;
      uint32_t dlbc_enable:1;
      uint32_t ooo_disable:1;
      uint32_t error_exception_enable:1;
      uint32_t reserved_1:6;
      uint32_t max_rd_tx:8;
      uint32_t max_wr_tx:8;
    };
    uint32_t val;
  };

} dm2_reg_t;


typedef int dma_dpu_inst_t;
typedef struct udma_ctx_t {
    int32_t num;                        // DMA instance identification index (num).
    uint32_t int_status;                // Internal State Machine
    uint32_t ext_status;                // External (M0) State machine status.
    uint32_t dma_tick_count;

    uint32_t desc_new;                  // New descriptor to be processed
    // Timing mode support
    uint32_t timing_on;                 // for timing, 0 for non-timing
    int16_t insn_timer;                 // Instruction timer : 0xFFFF means there is no
                                        // timing model working. 0 means the timer expires.
    uint32_t insn_timer_active;
    uint32_t insn_timer_pmu;
    // Error information
    uint32_t error_state_reason_captured;
    uint32_t error_state_reason;
    uint32_t error_state_address;
    uint32_t exception_status;
    uint32_t exception_va;
    uint32_t desc_restart;
    uint32_t pause_va;

    // DM2 Cfg
    dm2_reg_t dm2;

    dma_decoded_descriptor_t init_desc;
    dma_decoded_descriptor_t active;
    dma_decoded_descriptor_t target;  // For verif - precise pause


    uint32_t pc; // archsim debug/tracing

	// timing model
	dma_dpu_inst_t *dpuP;				// Timing model instance (Data Prefetch Unit)

} udma_ctx_t;


void dma_arch_tlbw(dma_t *dma, uint32_t jtlb_idx);
void dma_arch_tlbinvasid(dma_t *dma);


#define VERIF_DMASTEP_DESC_DONE 0
#define VERIF_DMASTEP_DESC_SET 1
#define VERIF_DMASTEP_DESC_IDLE 2
#define VERIF_DMASTEP_DESC_FINISHED 3


/*! Function to report a memory transaction is completed; call-back from
   the memory subsystem to report a transaction is completed in some ticks. */
/*! @param param Memory transaction parameters. */
/*! @return Owner DMA instance. */
dma_t* dma_report_mem_xact(dma_mem_xact* param);

/* DMA ticking to make a progress. */
/*! @param dma DMA instance. */
/*! @param do step, step or not of descriptor (advance state only). */
/*! @return Always 0. */

uint32_t dma_tick(dma_t *dma, uint32_t do_step);

uint32_t dma_set_timing(dma_t *dma, uint32_t timing_on);

/* Information which will be returned from a DMA instruction implementation.
   The engine should return these values to make our simulation scheme.
   The adapter will assume these values as a kind of contract. */
typedef struct {
  uint32_t excpt;                    /* 1 for exception report, otherwise 0 */
  dma_insn_checker_ptr insn_checker; /* Instruction completeness checker */
} dma_cmd_report_t;

/* User-mode commands */
void dma_cmd_start(dma_t *dma, uint32_t new, dma_cmd_report_t *report);

void dma_cmd_link(dma_t *dma, uint32_t cur, uint32_t new, dma_cmd_report_t *report);

void dma_cmd_poll(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report);
void dma_cmd_wait(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report);
void dma_cmd_waitdescriptor(dma_t *dma, uint32_t desc, uint32_t *ret, dma_cmd_report_t *report);

/* Monitor-mode commands */
void dma_cmd_pause(dma_t *dma, uint32_t *ret, dma_cmd_report_t *report);
void dma_cmd_resume(dma_t *dma, uint32_t ptr, dma_cmd_report_t *report);

/*! @param dma DMA instance. */
/*! @param timing_on 1 for timing-mode simulation, and 0 for non-timing mode. */
/*! @return 1 for success, and 0 for error. */
uint32_t dma_init(dma_t *dma, uint32_t timing_on);

/*! @param dma DMA instance. */
/*! @return 1 for success, and 0 for error. */
uint32_t dma_free(dma_t *dma);


void dma_stop(dma_t *dma);
uint32_t dma_in_error(dma_t *dma);

void dma_cmd_cfgrd(dma_t *dma, uint32_t index, uint32_t *val, dma_cmd_report_t *report);
void dma_cmd_cfgwr(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report);


dma_mem_xact_entry *mem_trans_remove(dma_t *dma);
void mem_trans_commit(dma_t *dma);
uint32_t mem_trans_num_entries_stored(dma_t *dma);
void dma_cmd_syncht(dma_t *dma, uint32_t index, uint32_t  val, dma_cmd_report_t *report);
void dma_cmd_tlbsynch(dma_t *dma, uint32_t index, uint32_t val, dma_cmd_report_t *report);
void dma_tlb_entry_invalidated(dma_t *dma, uint32_t asid);

uint32_t dma_get_tick_count(dma_t *dma);
void dma_force_error(dma_t *dma, uint32_t addr, uint32_t reason);
void dma_target_desc(dma_t *dma, uint32_t addr, uint32_t width, uint32_t height);
void dma_update_descriptor_done(dma_t *dma, dma_decoded_descriptor_t * entry);
uint32_t dma_retry_after_exception(dma_t *dma);
#endif
