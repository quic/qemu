/*
 * DMA engine adapter between Hexagon ArchSim and User DMA engine.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#include "system.h"
#include "external_api.h"
//#include "hvx.h"
//#include "hmx.h"
//#include "arch/exec.h"
//#include "arch/memwrap.h"
//#include "arch/stall.h"
//#include "arch/system.h"
//#include "arch/uarch/udma_if.h"
//#include "arch/pmu.h"
#include "dma_descriptor.h"
#include "dma_adapter.h"
//#include "iss_ver_registers.h"

#if 0
#define PRINTF(DMA, ...) \
{ \
  FILE * fp = dma_adapter_debug_log(DMA);\
  if (fp) {\
    fprintf(fp, __VA_ARGS__);\
    fprintf(fp, "\n");\
		fflush(fp);\
  }\
}
#ifdef VERIFICATION
#define DMA_STALL_SET(STALLTYPE,INSNCHECKER) {                             \
  thread->status |= EXEC_STATUS_REPLAY;                                    \
  thread->staller = staller_dmainsn;                                       \
  thread->stall_type = STALLTYPE;                                          \
  thread->processor_ptr->dma_insn_checker[thread->threadId] = INSNCHECKER; \
  }
#endif
#else
#define PRINTF(...)
#if 0
#define PRINTF(DMA, ...) do {\
    fprintf(stdout, __VA_ARGS__); \
    fprintf(stdout, "\n"); \
    fflush(stdout); \
  } while(0);
#endif
#define warn(...)
#endif

#if 0
#define DMA_STALL_SET(STALLTYPE,INSNCHECKER) {                             \
  STALLDEBUG("Set stall: tnum=%d cyc=%lld %s",                           \
    thread->threadId,                                                      \
    thread->processor_ptr->monotonic_pcycles,                              \
    stall_names[STALLTYPE]);                                               \
  thread->status |= EXEC_STATUS_REPLAY;                                    \
  thread->staller = staller_dmainsn;                                       \
  thread->stall_type = STALLTYPE;                                          \
  thread->processor_ptr->dma_insn_checker[thread->threadId] = INSNCHECKER; \
  }
#else
#define DMA_STALL_SET(STALLTYPE,INSNCHECKER)
#endif
#define INC_TSTAT(a)
#define INC_TSTATN(a,b)
#define CACHE_ASSERT(a,b)
#define ARCH_GET_PCYCLES(a) 0

#define sim_err_fatal(...)

typedef int64_t Word64;

//! Function pointer type of DMA instruction (command) implementation.
typedef size4u_t (*dma_adapter_cmd_impl_t)(thread_t*, size4u_t, size4u_t, dma_insn_checker_ptr*);

// List of the user-DMA instruction (command) implementation functions.
static size4u_t dma_adapter_cmd_start(thread_t *thread, size4u_t new, size4u_t dummy, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_link(thread_t *thread, size4u_t tail, size4u_t new, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_poll(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_wait(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_waitdescriptor(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_pause(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_resume(thread_t *thread, size4u_t arg1, size4u_t dummy, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_cfgrd(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_cfgwr(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_syncht(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);
static size4u_t dma_adapter_cmd_tlbsynch(thread_t *thread, size4u_t dummy1, size4u_t dummy2, dma_insn_checker_ptr *insn_checker);

//! Function table of the user-DMA instructions (commands).
// ATTENTION ==================================================================
// The element order should match to that of the enumerator dma_cmd_t
// in src/arch/external_api.h.
// ============================================================================
dma_adapter_cmd_impl_t dma_adapter_cmd_impl_tab[DMA_CMD_UND] = {
  &dma_adapter_cmd_start,             // dmstart.
  &dma_adapter_cmd_link,              // dmlink
  &dma_adapter_cmd_poll,              // dmpoll
  &dma_adapter_cmd_wait,              // dmwait
  &dma_adapter_cmd_syncht,            // dmsyncht
  &dma_adapter_cmd_waitdescriptor,    // dmwaitdescriptor
  &dma_adapter_cmd_cfgrd,             // dmcfgrd
  &dma_adapter_cmd_cfgwr,             // dmcfgwr
  &dma_adapter_cmd_pause,             // dmpause
  &dma_adapter_cmd_resume,            // dmresume
  &dma_adapter_cmd_tlbsynch,          // dmtlbsynch
};

#if 0
size4u_t pmu_event_mapping[DMA_PMU_NUM] = {
  tudma_active, //DMA_PMU_ACTIVE,
  tudma_stall_dfetch, //DMA_PMU_STALL_DESC_FETCH,
  tudma_stall_sync_response, //DMA_PMU_STALL_SYNC_RESP,
  tudma_stall_tlb_miss, //DMA_PMU_STALL_TLB_MISS,
	tudma_tlb_miss, //DMA_PMU_TLB_MISS,
  tudma_pause, //DMA_PMU_PAUSE_CYCLES,
  tudma_dmpoll_cycles, //DMA_PMU_DMPOLL_CYCLES,
  tudma_dmwait_cycles, //DMA_PMU_DMWAIT_CYCLES,
  tudma_dmsyncht_cycles, //DMA_PMU_SYNCHT_CYCLES,
  tudma_dmtlbsynch_cycles, //DMA_PMU_TLBSYNCH_CYCLES,
  tudma_desciptor_done,//DMA_PMU_DESC_DONE,
  tudma_dlbc_fetch,//DMA_PMU_DLBC_FETCH,
  tudma_dlbc_fetch_cycle,// DMA_PMU_DLBC_FETCH_CYCLES,
  tudma_unaligned_descriptor,//DMA_PMU_UNALIGNED_DESCRIPTOR,
  tudma_ordering_descriptor,//DMA_PMU_ORDERING_DESCRIPTOR,
  tudma_padding_descriptor,//DMA_PMU_PADDING_DESCRIPTOR,
  tudma_unaligned_rd,//DMA_PMU_UNALIGNED_RD,
  tudma_unaligned_wr,//DMA_PMU_UNALIGNED_WR,
  tudma_cycles_coherent_rd,//DMA_PMU_COHERENT_RD_CYCLES,
  tudma_cycles_coherent_wr,//DMA_PMU_COHERENT_WR_CYCLES,
  tudma_cycles_noncoherent_rd,//DMA_PMU_NONCOHERENT_RD_CYCLES,
  tudma_cycles_noncoherent_wr,//DMA_PMU_NONCOHERENT_WR_CYCLES,
  tudma_cycles_vtcm_rd,//DMA_PMU_VTCM_RD_CYCLES,
  tudma_cycles_vtcm_wr,//DMA_PMU_VTCM_WR_CYCLES,
  tudma_rd_buf_lvl_low,//DMA_PMU_RD_BUFFER_LEVEL_LOW,
  tudma_rd_buf_lvl_half,//DMA_PMU_RD_BUFFER_LEVEL_HALF,
  tudma_rd_buf_lvl_high,//DMA_PMU_RD_BUFFER_LEVEL_HIGH,
  tudma_rd_buf_lvl_full,//DMA_PMU_RD_BUFFER_LEVEL_FULL,
  tudma_dmstart,
  tudma_dmlink,
  tudma_dmresume
};
#endif


#if 0
//! Address range that can be serviced by the memory subsystem.
struct dma_addr_range_t* dma_addr_tab = NULL;

//! Current entry count of the address range table above.
unsigned int dma_addr_tab_cnt = 0;

//! Comparison function that will be used to sort the address range table.
static int dma_adapter_addrrange_cmp(const void *range_a, const void *range_b) {
  struct dma_addr_range_t *range_a_ptr = (struct dma_addr_range_t*)(range_a);
  struct dma_addr_range_t *range_b_ptr = (struct dma_addr_range_t*)(range_b);

  if ((range_a_ptr->base + range_a_ptr->size) <= range_b_ptr->base) {
    return -1;
  } else if ((range_b_ptr->base + range_b_ptr->size) <= range_a_ptr->base) {
    return 1;
  } else {
    return 0;
  }
}

//! Comparison function that will be used to search a certain address range.
static int dma_adapter_addr_find_cmp(const void *paddrkey, const void *range)
{
  struct dma_addr_range_t *range_ptr = (struct dma_addr_range_t*)(range);
  paddr_t paddr = *((paddr_t *)paddrkey);

  if (paddr < range_ptr->base) {
    return -1;
  } else if (paddr >= (range_ptr->base + range_ptr->size)) {
    return 1;
  } else {
    return 0;
  }
}
#if 0
uint32_t arch_dma_enable_timing(processor_t *proc) {

	for(int tnum = 0; tnum < proc->runnable_threads_max; tnum++) {
		dma_t *dma = proc->dma[tnum];
		if (dma) {
			dma_set_timing(dma, 1);
		}
	}
	return 0;
}
#endif


int dma_adapter_register_addr_range(void* owner, paddr_t base, paddr_t size) {
  struct dma_addr_range_t key;

  key.base = base;
  key.size = size;

  /* Make sure there is no address range already defined */
  if(dma_addr_tab != NULL) {
    if(bsearch(&key, dma_addr_tab, dma_addr_tab_cnt,
               sizeof(struct dma_addr_range_t), dma_adapter_addrrange_cmp)
       != NULL) {
      return 0;
    }
  }

  dma_addr_tab = realloc(dma_addr_tab,
                         (dma_addr_tab_cnt + 1) * sizeof(struct dma_addr_range_t));

  dma_addr_tab[dma_addr_tab_cnt] = key;
  dma_addr_tab_cnt++;

  qsort(dma_addr_tab, dma_addr_tab_cnt, sizeof(struct dma_addr_range_t),
        dma_adapter_addrrange_cmp);

  return 1;
}

struct dma_addr_range_t* dma_adapter_find_mem(paddr_t paddr) {
  if (dma_addr_tab == NULL) { return NULL; }

  struct dma_addr_range_t* mem =
    bsearch(&paddr, dma_addr_tab, dma_addr_tab_cnt,
            sizeof(struct dma_addr_range_t), dma_adapter_addr_find_cmp);

  return mem;
}
#endif

#if 0
static inline thread_t* dma_adapter_retrieve_thread(dma_t *dma) {
	return ((dma_adapter_engine_info_t *)dma->owner)->owner;
}
#endif
thread_t* dma_adapter_retrieve_thread(dma_t *dma) {
	return ((dma_adapter_engine_info_t *)dma->owner)->owner;
}

static inline dma_adapter_engine_info_t* dma_retrieve_dma_adapter(dma_t *dma) {
	return (dma_adapter_engine_info_t *)dma->owner;
}

#if 0
static int staller_dmainsn(thread_t *thread) {
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];
  if (thread->processor_ptr->dma_insn_checker[thread->threadId] == NULL) { return 0; }

  // This staller will check if a thread 0's DMA instruction is done with its
  // job or not. The DMA engine should know if the instruction completes the
  // job. Thus this adapter is instruction-agnostic - whatever a DMA instruction
  // is given, if the instruction has its own latency to model, this staller
  // will call the engine to check it via the instruction checker.
  int release = (*(thread->processor_ptr->dma_insn_checker[thread->threadId]))(dma);
  if (release == 1) {
    // Let's finish stalling!
    thread->status &= ~EXEC_STATUS_REPLAY;

    // Do not forget removing the instruction completion checker!
    // Otherwise, another staller will be set again successively.
    thread->processor_ptr->dma_insn_checker[thread->threadId] = NULL;
    return 1;
  }
  return 0;
}
#endif

//! Function to request stalling. The DMA engine calls this function to stall
//! an instruction stream.
static int dma_adapter_set_staller(dma_t *dma, dma_insn_checker_ptr insn_checker) {
  thread_t* thread __attribute__((unused)) = dma_adapter_retrieve_thread(dma);
#ifdef VERIFICATION
	warn("DMA %d: ADAPTER  dma_adapter_set_staller. tick count=%d", dma->num, dma_get_tick_count(dma));
	PRINTF(dma, "DMA %d: ADAPTER  dma_adapter_set_staller. tick count=%d ", dma->num,  dma_get_tick_count(dma));
#endif
  // Set up a staller per thread.
  DMA_STALL_SET(dmainsn, insn_checker);

  return 0;
}

int dma_adapter_in_monitor_mode(dma_t *dma) {
  return sys_in_monitor_mode(dma_adapter_retrieve_thread(dma));
}
int dma_adapter_in_guest_mode(dma_t *dma) {
  return sys_in_guest_mode(dma_adapter_retrieve_thread(dma));
}
int dma_adapter_in_user_mode(dma_t *dma) {
  return sys_in_user_mode(dma_adapter_retrieve_thread(dma));
}
int dma_adapter_in_debug_mode(dma_t *dma) {
  thread_t* thread __attribute__((unused)) = dma_adapter_retrieve_thread(dma);
#ifdef VERIFICATION
  if (fIN_DEBUG_MODE(thread->threadId)) {
		PRINTF(dma, "DMA %d: ADAPTER thread->debug_mode=%d ISDST=%x ",dma->num, thread->debug_mode, (fREAD_GLOBAL_REG_FIELD(ISDBST,ISDBST_DEBUGMODE) & 1<<thread->threadId));
  }
#endif
  return fIN_DEBUG_MODE(thread->threadId);
}

FILE * dma_adapter_debug_log(dma_t *dma) {
  thread_t* thread = dma_adapter_retrieve_thread(dma);
  return thread->processor_ptr->arch_proc_options->dmadebugfile;
}

int dma_adapter_debug_verbosity(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	return thread->processor_ptr->arch_proc_options->dmadebug_verbosity;
}

void do_callback(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info, int callback_state);
void do_callback(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info, int callback_state) {

#if 0
  thread_t * thread = dma_adapter_retrieve_thread(dma);

  dma_descriptor_callback_info_t info = {0};
  info.dmano = dma->num;
  info.desc_va = desc_va;
	info.callback_state = callback_state;
  info.type = (uint32_t)get_dma_desc_desctype((void*)desc_info);
  info.state = (uint32_t)get_dma_desc_dstate((void*)desc_info);
  info.src_va = (uint32_t)get_dma_desc_src((void*)desc_info);
  info.dst_va = (uint32_t)get_dma_desc_dst((void*)desc_info);
	info.desc_id = id;
  // Translate a VA.
  xlate_info_t xlate_task;   // Storage to get a PA returned back.
  exception_info e_info;     // Storage to retrieve an exception if it occurs.

  sys_xlate(thread, desc_va, TYPE_LOAD, access_type_load, 0, 4-1, &xlate_task, &e_info);
  info.desc_pa = xlate_task.pa;

  sys_xlate(thread, info.src_va, TYPE_LOAD, access_type_load, 0, 4-1, &xlate_task, &e_info);
  info.src_pa = xlate_task.pa;

  sys_xlate(thread, info.dst_va, TYPE_STORE, access_type_store, 0, 4-1, &xlate_task, &e_info);
  info.dst_pa = xlate_task.pa;

  if (info.type == 0) {
    info.len = (uint32_t)get_dma_desc_length((void*)desc_info);
  } else {
    info.src_offset = (uint32_t)get_dma_desc_srcwidthoffset((void*)desc_info);
    info.dst_offset = (uint32_t)get_dma_desc_dstwidthoffset((void*)desc_info);
    info.hlen = (uint32_t)get_dma_desc_srcwidthoffset((void*)desc_info) - (uint32_t)get_dma_desc_srcwidthoffset((void*)desc_info);
    info.vlen = (uint32_t)get_dma_desc_roiheight((void*)desc_info);
  }


  if (thread->processor_ptr->options->dma_callback) {
      thread->processor_ptr->options->dma_callback(thread->system_ptr, thread->processor_ptr, &info);
  }
#endif
}

void dma_adapter_set_target_descriptor(thread_t *thread, uint32_t dm0,  uint32_t va, uint32_t width, uint32_t height) {
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	PRINTF(dma, "DMA %d: ADAPTER  setting target descriptor dm0=%x va=%x width=%d height=%x", dma->num, dm0, va, width, height);
	dma_target_desc(dma, va, width, height);
}


int dma_adapter_match_tlb_entry (uint64_t entry, uint32_t asid, uint32_t va );
int dma_adapter_match_tlb_entry (uint64_t entry, uint32_t asid, uint32_t va ) {
	return 0; //tlb_match_entry( entry,  asid, va);
}


int dma_adapter_descriptor_start(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info) {
	//PRINTF(dma, "DMA %d: ADAPTER  dma_adapter_descriptor_start : desc_va = 0x%x id=%d", dma->num, desc_va, id);
	do_callback(dma, id, desc_va, desc_info, DMA_DESC_STATE_START);

  return 0;
}

int dma_adapter_descriptor_end(dma_t *dma, uint32_t id, uint32_t desc_va, uint32_t *desc_info, int pause, int exception) {
	PRINTF(dma, "DMA %d: ADAPTER  dma_adapter_descriptor_end : desc_va = 0x%x id=%d", dma->num, desc_va, id);
	int callback_state = DMA_DESC_STATE_DONE;
	callback_state = (pause) ? DMA_DESC_STATE_PAUSE : callback_state;
	callback_state = (exception) ? DMA_DESC_STATE_EXCEPT : callback_state;
	do_callback(dma, id, desc_va, desc_info, callback_state);
  return 0;
}


#ifndef UDMA_STANDALONE_MODEL
int dma_adapter_create_mem_xact(struct dma_mem_xact *param) {

  if (param == NULL) { return 0; }

  thread_t *thread = dma_adapter_retrieve_thread(param->dma);
  processor_t* proc = thread->processor_ptr;




  mem_access_info_t ma = {0};

  /* The basic stuff */
	ma.pc_va = param->pc;
  ma.paddr = param->paddr;
  ma.vaddr = param->vaddr;
  ma.width = param->len;
  ma.type = param->is_read ? access_type_udma_load : access_type_udma_store;
  ma.tnum = param->dma->num;
    ma.valid = 1;


  int status = proc->uarch.uarch_udma_if_log_ptr(proc, &ma, param->is_desc, param->is_bypass, param);

	PRINTF(param->dma, "DMA %d: ADAPTER  placing a mem xact 0x%x(0x%lx) : is_read=%u is_bypass=%u len=%ld status=%d @PCYCLE=%llu",
         param->dma->num, param->vaddr, param->paddr, param->is_read,
         param->is_bypass, param->len, status, thread->processor_ptr->monotonic_pcycles);

  return status;
}
#endif

int dma_adapter_callback(processor_t *proc, int dma_num, void * dma_xact) {
  struct dma_mem_xact * param = (struct dma_mem_xact *)dma_xact;
  param->callback(param);
  return 1;
}

int dma_adapter_xlate_desc_va(dma_t *dma, uint32_t va, uint64_t* pa, dma_memaccess_info_t * dma_memaccess_info) {

  // Translate a VA.
  xlate_info_t xlate_task;   // Storage to get a PA returned back.
	exception_info e_info = {0};     // Storage to retrieve an exception if it occurs.
  thread_t * thread = dma_adapter_retrieve_thread(dma);

  int ret = sys_xlate_dma(thread, va, TYPE_DMA_FETCH,  access_type_load,  0, 0, &xlate_task, &e_info);

  (*pa) = xlate_task.pa;
	dma_access_rights_t * perm = &dma_memaccess_info->perm;
  perm->val = 0;
  perm->u = xlate_task.pte_u;
  perm->w = xlate_task.pte_w;
  perm->r = xlate_task.pte_r;
  perm->x = xlate_task.pte_x;

	dma_memtype_t * memtype = &dma_memaccess_info->memtype;
    memtype->vtcm = xlate_task.memtype.vtcm;
    memtype->l2tcm = xlate_task.memtype.l2tcm;
    memtype->ahb = xlate_task.memtype.ahb;
    memtype->axi2 = xlate_task.memtype.axi2;
    memtype->l2vic = xlate_task.memtype.l2vic;
	memtype->invalid_cccc = xlate_task.memtype.invalid_cccc;
	memtype->invalid_dma = xlate_task.memtype.invalid_dma | xlate_task.memtype.invalid_cccc;

	dma_memaccess_info->exception = (ret == 0);
#if 0
	dma_memaccess_info->jtlb_idx   = dma_memaccess_info->exception ? -1 : xlate_task.jtlbidx;
	dma_memaccess_info->jtlb_entry = dma_memaccess_info->exception ? 0 : thread->processor_ptr->TLB_regs[xlate_task.jtlbidx];
#endif

	dma_memaccess_info->pa = *pa;
	dma_memaccess_info->va = va;
	dma_memaccess_info->access_type = TYPE_DMA_FETCH;
	if (ret == 0) {
		dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
		dma_info->einfo = e_info;

		warn("DMA %d: ADAPTER  exception on translation of descriptor for VA=%x badva0=%x badva1=%x cause=%x cccc=%x U:%x R:%x W:%x X:%x jtlbidx=%d", dma->num, va, dma_info->einfo.badva0, dma_info->einfo.badva1, dma_info->einfo.cause, xlate_task.cccc, perm->u, perm->r, perm->w, perm->x, xlate_task.jtlbidx);
//		PRINTF(dma, "DMA %d: Tick %d: ADAPTER exception on descriptor fetch=%x jtlbidx=%d invalid cccc=%d memory_invalid=%d va=0x%x PA=%llx", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), dma_info[dma->num].einfo.cause, xlate_task.jtlbidx , memtype->invalid_cccc, memtype->invalid_dma, (unsigned int)va, (long long int)pa);
	}



  return ret;
}

int dma_adapter_xlate_va(dma_t *dma, uint32_t va, uint64_t* pa, dma_memaccess_info_t * dma_memaccess_info, int width, int store) {
  int access_type = TYPE_LOAD;
  int maptr_type = access_type_load;
  if (store) {
    access_type = TYPE_STORE;
    maptr_type = access_type_store;
  }

  // Translate a VA.
  xlate_info_t xlate_task;   // Storage to get a PA returned back.
	exception_info e_info = {0};      // Storage to retrieve an exception if it occurs.
  thread_t * thread = dma_adapter_retrieve_thread(dma);

  int ret = sys_xlate_dma(thread, va, access_type, maptr_type, 0, width-1, &xlate_task, &e_info);
  // If an exception occurs, keep it for future.
  // Retrieve a PA.

  (*pa) = xlate_task.pa;

	dma_access_rights_t * perm = &dma_memaccess_info->perm;
    perm->val = 0;
    perm->u = xlate_task.pte_u;
    perm->w = xlate_task.pte_w;
    perm->r = xlate_task.pte_r;
    perm->x = xlate_task.pte_x;

	dma_memtype_t * memtype = &dma_memaccess_info->memtype;
	memtype->vtcm = xlate_task.memtype.vtcm;
	memtype->l2tcm = xlate_task.memtype.l2tcm;
	memtype->ahb = xlate_task.memtype.ahb;
	memtype->axi2 = xlate_task.memtype.axi2;
	memtype->l2vic = xlate_task.memtype.l2vic;
	memtype->invalid_cccc = xlate_task.memtype.invalid_cccc;
	memtype->invalid_dma = xlate_task.memtype.invalid_dma | xlate_task.memtype.invalid_cccc;

	dma_memaccess_info->exception = (ret == 0);
#if 0
	dma_memaccess_info->jtlb_idx   = dma_memaccess_info->exception ? -1 : xlate_task.jtlbidx;
	dma_memaccess_info->jtlb_entry = dma_memaccess_info->exception ? 0 : thread->processor_ptr->TLB_regs[xlate_task.jtlbidx];
#endif

	dma_memaccess_info->pa = *pa;
	dma_memaccess_info->va = va;
	dma_memaccess_info->access_type = access_type;
  if (ret == 0) {
		dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
		dma_info->einfo = e_info;

		PRINTF(dma, "DMA %d: Tick %d: ADAPTER %s Exception Encounted: VA=0x%x PA=0x%llx size=%d. cause=%x jtlbidx=%d invalid cccc=%d memory_invalid=%d U:%x R:%x W:%x X:%x",dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), (store) ? "WR" : "RD",
		(unsigned int)va, (long long int)*pa, width, dma_info->einfo.cause, xlate_task.jtlbidx , memtype->invalid_cccc, memtype->invalid_dma,
		perm->u, perm->w, perm->r, perm->x);

		warn("DMA %d: ADAPTER %s Exception Encounted: VA=0x%x PA=0x%llx size=%d cause=%x jtlbidx=%d invalid cccc=%d memory_invalid=%d U:%x R:%x W:%x X:%x",dma->num, (store) ? "WR" : "RD",
		(unsigned int)va, (long long int)pa,  width, dma_info->einfo.cause, xlate_task.jtlbidx , memtype->invalid_cccc, memtype->invalid_dma,
		perm->u, perm->w, perm->r, perm->x);

  }



  return ret;
}

int dma_adapter_register_error_exception(dma_t *dma, uint32_t va) {
#if 0
  thread_t * thread = dma_adapter_retrieve_thread(dma);
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
  exception_info e_info;
  register_dma_error_exception(thread, &e_info, va);
	dma_info->einfo = e_info;
	warn("DMA %d: ADAPTER  registering DMA error exception cause=%x va=%x", dma->num, e_info.cause, va);
#endif
  return 0;
};

#if 0
void dma_adapter_force_dma_error(thread_t *thread, uint32_t badva, uint32_t syndrome) {
	dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma_force_error(dma, badva, syndrome);
	PRINTF(dma, "DMA %d: ADAPTER  forcing DMA error for badva=%x syndrome=%d", dma->num, badva, syndrome);
	warn("DMA %d: ADAPTER  forcing DMA error for badva=%x syndrome=%d", dma->num, badva, syndrome);
}
#endif

int dma_adapter_register_perm_exception(dma_t *dma, uint32_t va,  dma_access_rights_t access_rights, int store);
int dma_adapter_register_perm_exception(dma_t *dma, uint32_t va,  dma_access_rights_t access_rights, int store) {
#if 0
  thread_t * thread = dma_adapter_retrieve_thread(dma);
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
  exception_info e_info;

  e_info.valid = 1;
  e_info.type = EXCEPT_TYPE_PRECISE;


  if (store) {
    e_info.cause = (access_rights.w)  ?  PRECISE_CAUSE_PRIV_NO_UWRITE : PRECISE_CAUSE_PRIV_NO_WRITE;
  } else {
    e_info.cause =  (access_rights.r) ?  PRECISE_CAUSE_PRIV_NO_UREAD : PRECISE_CAUSE_PRIV_NO_READ;
  }

  e_info.badva0 = va;
  e_info.badva1 = thread->gpr[HEX_SREG_BADVA1];
  e_info.bv0 = 1;
  e_info.bv1 = 0;
  e_info.bvs = 0;

  e_info.elr = thread->gpr[HEX_REG_PC];
  e_info.diag = 0;
  e_info.de_slotmask = 1;

	dma_info->einfo = e_info;
	warn("DMA %d: ADAPTER  registering permissions exception cause=%x access_writes= U:%x R:%x W:%x X:%x badva=%x", dma->num, e_info.cause, access_rights.u, access_rights.r, access_rights.w, access_rights.x, va );
#endif

  return 0;
};

#if 0
int dma_test_gen_mode(dma_t *dma) {
  thread_t * thread = dma_adapter_retrieve_thread(dma);
  return thread->processor_ptr->options->testgen_mode;
}
#endif

static size8u_t memread(thread_t *thread, mem_access_info_t *memaptr, int do_trace)
{
    paddr_t paddr = memaptr->paddr;
    int width = memaptr->width;
    size8u_t data;

    switch (width) {
#ifdef CONFIG_USER_ONLY
        case 1:
            get_user_u8(data, paddr);
            break;
        case 2:
            get_user_u16(data, paddr);
            break;
        case 4:
            get_user_u32(data, paddr);
            break;
        case 8:
            get_user_u64(data, paddr);
            break;
#else
        case 1:
        case 2:
        case 4:
        case 8:
            hexagon_tools_memory_read(thread, paddr, width, &data);
            break;
#endif
        default:
            g_assert_not_reached();
    }
    return data;
}

static void memwrite(thread_t *thread, mem_access_info_t *memaptr)
{
    paddr_t paddr = memaptr->paddr;
    int width = memaptr->width;
    size8u_t data = memaptr->stdata;

    switch (width) {
#ifdef CONFIG_USER_ONLY
        case 1:
            put_user_u8(data, paddr);
            break;
        case 2:
            put_user_u16(data, paddr);
            break;
        case 4:
            put_user_u32(data, paddr);
            break;
        case 8:
            put_user_u64(data, paddr);
            break;
#else
        case 1:
        case 2:
        case 4:
        case 8:
            hexagon_tools_memory_write(thread, paddr, width, data);
            break;
#endif
        default:
            g_assert_not_reached();
    }
}


int dma_adapter_memread(dma_t *dma, uint32_t va, uint64_t pa, uint8_t *dst, int width) {
  mem_access_info_t macc_task;
  thread_t * thread = dma_adapter_retrieve_thread(dma);
  macc_task.vaddr = va;
  macc_task.paddr = pa;
  macc_task.width = 1;
  while (width > 0) {
    (*dst) = memread(thread, &macc_task, 0);
#ifdef VERIFICATION
    //PRINTF(dma, "DMA ADAPTER:  %d: callback for DREAD at %llx byte=%x",thread->threadId, macc_task.paddr, (*dst));
    if (thread->processor_ptr->options->sim_memory_callback && thread->processor_ptr->options->testgen_mode) {
    //  PRINTF(dma, "DMA ADAPTER:  %d: callback for DREAD at %llx byte=%x",thread->threadId, macc_task.paddr, (*dst));
      thread->processor_ptr->options->sim_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId,macc_task.vaddr,macc_task.paddr,1,DREAD,(*dst));
    }
#endif
    macc_task.vaddr++;
    macc_task.paddr++;
    dst++;
    width--;

  }

  return ((*(uint32_t *)dst) != 0xdeadbeef);
}

int dma_adapter_memwrite(dma_t *dma, uint32_t va, uint64_t pa, uint8_t *src, int width) {
  mem_access_info_t macc_task;
  thread_t * thread = dma_adapter_retrieve_thread(dma);
  macc_task.vaddr = va;
  macc_task.paddr = pa;
  macc_task.width = 1;
  macc_task.stdata = (*src);
  while (width > 0) {
    memwrite(thread, &macc_task);
#ifdef VERIFICATION
    //PRINTF(dma, "DMA ADAPTER:  %d: callback for DWRITE at %llx byte=%x",thread->threadId, macc_task.paddr, (*src));
    if (thread->processor_ptr->options->sim_memory_callback && thread->processor_ptr->options->testgen_mode) { \
      //PRINTF(dma, "DMA ADAPTER:  %d: callback for DWRITE at %llx byte=%x",thread->threadId, macc_task.paddr, (*src));
      thread->processor_ptr->options->sim_memory_callback(thread->system_ptr,thread->processor_ptr, thread->threadId,macc_task.vaddr,macc_task.paddr,1,DWRITE,(*src));
    }
#endif
    macc_task.vaddr++;
    macc_task.paddr++;
    src++;
    macc_task.stdata = (*src);
    width--;


  }

  return 1;
}

dma_t *dma_adapter_init(processor_t *proc, int dmanum) {
	dma_t *dma = NULL;
	dma_adapter_engine_info_t *dma_adapter = NULL;

  if ((dma = calloc(1, sizeof(dma_t))) == NULL) {
    sim_err_fatal(proc->system_ptr, proc, 0, (char *) __FUNCTION__,
                  __FILE__, __LINE__, "cannot create dma");
    return NULL;
  }

	if ((dma_adapter = calloc(1, sizeof(dma_adapter_engine_info_t))) == NULL) {
		sim_err_fatal(proc->system_ptr, proc, 0, (char *) __FUNCTION__,
		              __FILE__, __LINE__, "cannot create dma_adapter");
		return NULL;
	}

  // Set up other arguments.
  dma->num = dmanum;

  // An owner of each DMA instance is a hardware thread.
	dma_adapter->owner = proc->thread[dmanum];
	dma->owner = (void*)dma_adapter;
	proc->dma_engine_info[dmanum] = (struct dma_adapter_engine_info *)dma_adapter;
	desc_tracker_init(proc, dmanum);

	queue_alloc(proc, &dma_adapter->desc_queue, dma_adapter->desc_entries, 256);
#ifdef FAKE_TIMING
	pipequeue_alloc(proc, &dma_adapter->fake_timing_queue, dma_adapter->fake_timing_entries, 64, 100, 0, 1);
#endif

  // At a thread side, we will maintain an instruction completeness checker
  // to manage stalling. Here, we initialize the checker to null by default.
  proc->dma_insn_checker[dmanum] = NULL;

  if (dma_init(dma, proc->thread[dmanum]->timing_on) == 0) {
    sim_err_fatal(proc->system_ptr, proc, 0, (char *) __FUNCTION__,
                  __FILE__, __LINE__, "failed dma_init");

    free(dma);
		free(dma_adapter);

    return NULL;
  }


#if 0
	dma_adapter_cmd_latency[DMA_INSN_LATENCY_DMSTART]  = ARCHOPT(udma_dmstart_latency);
	dma_adapter_cmd_latency[DMA_INSN_LATENCY_DMRESUME] = ARCHOPT(udma_dmresume_latency);
	dma_adapter_cmd_latency[DMA_INSN_LATENCY_DMLINK]   = ARCHOPT(udma_dmlink_latency);
	dma_adapter_cmd_latency[DMA_INSN_LATENCY_DMPAUSE]  = ARCHOPT(udma_dmpause_latency);
	dma_adapter_cmd_latency[DMA_INSN_LATENCY_DMPOLL]   = ARCHOPT(udma_dmpoll_latency);
	dma_adapter_cmd_latency[DMA_INSN_LATENCY_DMWAIT]   = ARCHOPT(udma_dmwait_latency);
#endif

  return dma;
}
void dma_adapter_tlb_invalidate(thread_t *thread, int tlb_idx, uint64_t tlb_entry_old, uint64_t tlb_entry_new) {
#if 0
	//dma_t *dma = NULL; // This is a global invalidate so it should go to all DMA's
  uint32_t tlb_asid = GET_FIELD(PTE_ASID, tlb_entry_old);

	// On TLB Write, single the dma's that the tlbw entry has been erased.
	// timing model will flush it self if it can't translate
	//if (proc->timing_on) {
		dma_t *dma_ptr = thread->processor_ptr->dma[0];
		PRINTF(dma_ptr, "DMA X: Tick %d: TLBW invalidate for tlb[%d]=%llx new entry: %llx asid: %x from tnum=%d", ARCH_GET_PCYCLES(thread->processor_ptr), tlb_idx, (long long int)tlb_entry_old, (long long int)tlb_entry_new,  (int)tlb_asid, thread->threadId);
		for (int tnum = 0; tnum < thread->processor_ptr->runnable_threads_max; tnum++) {
			dma_ptr = thread->processor_ptr->dma[tnum];

			if (thread->processor_ptr->timing_on) {
				dma_arch_tlbw(dma_ptr, tlb_idx);
			}
	//		dma_adapter_flush_timing(dma_ptr);
			dma_retry_after_exception(dma_ptr);
		}
	//}
#endif
}

desc_tracker_entry_t *  dma_adapter_insert_desc_queue(dma_t *dma, dma_decoded_descriptor_t * desc ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	desc_tracker_entry_t * entry = desc_tracker_acquire(proc, dma->num, desc);
	int idx = queue_push(proc, dma->num, &dma_info->desc_queue);
	dma_info->desc_entries[idx] = entry;
	entry->dnum = dma->num;
	entry->desc = *desc;
	entry->dma = (void *) dma;
//	PRINTF(dma, "DMA %d: Tick %d: ADAPTER  acquiring descriptor va=%x id=%d", dma->num, ARCH_GET_PCYCLES(proc), dma_info->desc_entries[idx]->desc.va, dma_info->desc_entries[idx]->id);
	return entry;
}

#ifdef FAKE_TIMING
int dma_adapter_insert_to_timing(dma_t *dma, desc_tracker_entry_t * entry ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = pipequeue_push(proc, 0,  &dma_info->fake_timing_queue);
	dma_info->fake_timing_entries[idx] = entry;
	return 0;
}
int dma_adapter_pop_from_timing(dma_t *dma ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);

	if (!pipequeue_empty(proc, 0,  &dma_info->fake_timing_queue)) {
		int pop_ready = pipequeue_pop_ready(proc, 0,  &dma_info->fake_timing_queue);
		int idx = pipequeue_peek(proc, 0,  &dma_info->fake_timing_queue);
		PRINTF(dma, "DMA %d: ADAPTER pcycle=%d  timing pop_ready=%d descriptor va=%x id=%d", dma->num, thread->processor_ptr->monotonic_pcycles, pop_ready,  dma_info->fake_timing_entries[idx]->desc.va, dma_info->fake_timing_entries[idx]->id);
	}

	if (pipequeue_pop_ready(proc, 0,  &dma_info->fake_timing_queue)) {
		int idx = pipequeue_pop(proc, 0,  &dma_info->fake_timing_queue);
		dma_info->fake_timing_entries[idx]->callback((void*)dma_info->fake_timing_entries[idx]);
	}
	return 0;
}

int dma_adapter_flush_timing(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	while (!pipequeue_empty(proc, 0,  &dma_info->fake_timing_queue)) {
		int idx = pipequeue_pop(proc, 0,  &dma_info->fake_timing_queue);
		PRINTF(dma, "DMA %d: ADAPTER  flushing timing  va=%x id=%d", dma->num, dma_info->fake_timing_entries[idx]->desc.va, dma_info->fake_timing_entries[idx]->id);
		dma_info->fake_timing_entries[idx]->callback((void*)dma_info->fake_timing_entries[idx]);
		dma_info->fake_timing_entries[idx] = NULL;
	}
	dma_adapter_pop_desc_queue_done(dma);
	return 0;
}
#endif

int dma_adapter_pop_desc_queue(dma_t *dma, desc_tracker_entry_t * entry ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = queue_pop(proc, dma->num, &dma_info->desc_queue);
	CACHE_ASSERT(proc, dma_info->desc_entries[idx]->id == entry->id);
	PRINTF(dma, "DMA %d: Tick: %d ADAPTER  dma_adapter_pop_desc_queue idx=%d id=%d", dma->num, ARCH_GET_PCYCLES(proc), idx, (int)(entry->id));
	dma_info->desc_entries[idx] = NULL;
	desc_tracker_release(proc, dma->num, entry);
	return 1;
}

uint64_t dma_adapter_get_pcycle(dma_t *dma){
	thread_t* thread __attribute__((unused)) = dma_adapter_retrieve_thread(dma);
	return ARCH_GET_PCYCLES(thread->processor_ptr);
}


desc_tracker_entry_t *   dma_adapter_peek_desc_queue_head(dma_t *dma) {
	thread_t* thread __attribute__((unused)) = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
	//PRINTF(dma, "DMA %d: ADAPTER  dma_adapter_peek_desc_queue_head idx=%d", dma->num, idx);
	return (idx >= 0) ? dma_info->desc_entries[idx] : 0;
}

uint32_t dma_adapter_peek_desc_queue_head_va(dma_t *dma) {
	desc_tracker_entry_t * entry = dma_adapter_peek_desc_queue_head(dma);
	return (entry) ? entry->desc.va : 0;
}

int dma_adapter_pop_desc_queue_done(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int done = 1;
	int is_empty = 0;
	while (done) {
		int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
		int pop = 0;
		pop = (idx>=0);
		pop &= (dma_info->desc_entries[idx]->desc.state == DMA_DESC_DONE) || (dma_info->desc_entries[idx]->desc.state ==  DMA_DESC_EXCEPT_ERROR);
		// If it's exception running, we're going to keep that descriptor
		if ( pop ) {
			idx = queue_pop(proc, dma->num, &dma_info->desc_queue);

			PRINTF(dma, "DMA %d: Tick: %d ADAPTER  releasing descriptor va=%x id=%d desc.state=%d pause=%d", dma->num, ARCH_GET_PCYCLES(proc), dma_info->desc_entries[idx]->desc.va, (int)(dma_info->desc_entries[idx]->id), dma_info->desc_entries[idx]->desc.state,  dma_info->desc_entries[idx]->desc.pause);


			// Update Descriptor
			dma_update_descriptor_done(dma, &dma_info->desc_entries[idx]->desc);

			desc_tracker_release(proc, dma->num, dma_info->desc_entries[idx]);
			dma_info->desc_entries[idx] = NULL;

			is_empty = queue_empty(proc, dma->num, &dma_info->desc_queue);
		} else {

			//if (dma_info->desc_entries[idx]->desc.state ==  DMA_DESC_EXCEPT_RUNNING) {
			//	PRINTF(dma, "DMA %d: Tick: %d ADAPTER not releasing descriptor va=%x id=%d desc.state=%d due to exception, but updating", dma->num, ARCH_GET_PCYCLES(proc), dma_info->desc_entries[idx]->desc.va, dma_info->desc_entries[idx]->id, dma_info->desc_entries[idx]->desc.state);
			//	dma_update_descriptor_done(dma, &dma_info->desc_entries[idx]->desc);
			//}

			done = 0;
		}

	}


	if (is_empty && !dma_in_error(dma)) {
		PRINTF(dma, "DMA %d: Tick: %d ADAPTER stopping DMA since we are done with all the descriptors in the chain.", dma->num, ARCH_GET_PCYCLES(proc));
		dma_stop(dma);
	} else if (is_empty && !dma_in_error(dma)){
		PRINTF(dma, "DMA %d: Tick: %d ADAPTER. We're done with all the descriptors in chain, but in exception mode. can't go to idle.", dma->num, ARCH_GET_PCYCLES(proc));
	}

	return is_empty;
}


int dma_adapter_pop_desc_queue_verif_pause(dma_t *dma);
int dma_adapter_pop_desc_queue_verif_pause(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int done = 1;
	int is_empty = 0;
	while (done) {
		int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
		int pop = 0;
		pop = (idx>=0);
		if ( pop ) {
			idx = queue_pop(proc, dma->num, &dma_info->desc_queue);
			PRINTF(dma, "DMA %d: Tick: %d ADAPTER cmd_pause releasing descriptor va=%x id=%d desc.state=%d pause=%d", dma->num, ARCH_GET_PCYCLES(proc), dma_info->desc_entries[idx]->desc.va, (int)(dma_info->desc_entries[idx]->id), dma_info->desc_entries[idx]->desc.state,  dma_info->desc_entries[idx]->desc.pause);

			// Update Descriptor
			dma_update_descriptor_done(dma, &dma_info->desc_entries[idx]->desc);
			desc_tracker_release(proc, dma->num, dma_info->desc_entries[idx]);
			dma_info->desc_entries[idx] = NULL;
			is_empty = queue_empty(proc, dma->num, &dma_info->desc_queue);
		} else {
			done = 0;
		}

	}
	if (is_empty && !dma_in_error(dma)) {
		dma_stop(dma);
	}

	return is_empty;
}


int dma_adapter_desc_queue_empty(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	return queue_empty(proc, dma->num, &dma_info->desc_queue);
}


#ifdef FAKE_TIMING
int dma_adapter_insert_to_timing(dma_t *dma, desc_tracker_entry_t * entry ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = pipequeue_push(proc, 0,  &dma_info->fake_timing_queue);
	dma_info->fake_timing_entries[idx] = entry;
	return 0;
}
int dma_adapter_pop_from_timing(dma_t *dma ) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);

	if (!pipequeue_empty(proc, 0,  &dma_info->fake_timing_queue)) {
		int pop_ready = pipequeue_pop_ready(proc, 0,  &dma_info->fake_timing_queue);
		int idx = pipequeue_peek(proc, 0,  &dma_info->fake_timing_queue);
		PRINTF(dma, "DMA %d: ADAPTER pcycle=%d  timing pop_ready=%d descriptor va=%x id=%d", dma->num, thread->processor_ptr->monotonic_pcycles, pop_ready,  dma_info->fake_timing_entries[idx]->desc.va, dma_info->fake_timing_entries[idx]->id);
	}

	if (pipequeue_pop_ready(proc, 0,  &dma_info->fake_timing_queue)) {
		int idx = pipequeue_pop(proc, 0,  &dma_info->fake_timing_queue);
		dma_info->fake_timing_entries[idx]->callback((void*)dma_info->fake_timing_entries[idx]);
	}
	return 0;
}

int dma_adapter_flush_timing(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	while (!pipequeue_empty(proc, 0,  &dma_info->fake_timing_queue)) {
		int idx = pipequeue_pop(proc, 0,  &dma_info->fake_timing_queue);
		PRINTF(dma, "DMA %d: ADAPTER  flushing timing  va=%x id=%d", dma->num, dma_info->fake_timing_entries[idx]->desc.va, dma_info->fake_timing_entries[idx]->id);
		dma_info->fake_timing_entries[idx]->callback((void*)dma_info->fake_timing_entries[idx]);
		dma_info->fake_timing_entries[idx] = NULL;
	}
	dma_adapter_pop_desc_queue_done(dma);
	return 0;
}
#endif

uint32_t dma_adapter_head_desc_queue_peek(dma_t *dma);
uint32_t dma_adapter_head_desc_queue_peek(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	processor_t * proc = thread->processor_ptr;
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);
	int idx = queue_peek(proc, dma->num, &dma_info->desc_queue);
	PRINTF(dma, "DMA %d: Tick: %d ADAPTER  dma_adapter_head_desc_queue_peek idx=%d", dma->num, ARCH_GET_PCYCLES(proc), idx);
	return (idx >= 0) ? dma_info->desc_entries[idx]->desc.va : 0;
}



void dma_adapter_free(processor_t *proc, int dmanum) {
  dma_free(proc->dma[dmanum]);
  free(proc->dma[dmanum]);
	desc_tracker_free(proc, dmanum);
	free(proc->dma_engine_info[dmanum]);
  proc->dma[dmanum] = NULL;
}

void arch_dma_cycle(processor_t *proc, int dmanum);
void arch_dma_cycle(processor_t *proc, int dmanum) {
  dma_t *dma = proc->dma[dmanum];
	dma_tick(dma, 1);
}

void arch_dma_cycle_no_desc_step(processor_t *proc, int dmanum);
void arch_dma_cycle_no_desc_step(processor_t *proc, int dmanum) {
	dma_t *dma = proc->dma[dmanum];
	dma_tick(dma, 0);
	desc_tracker_cycle(proc, dmanum);
}
void arch_dma_tick_until_stop(processor_t *proc, int dmanum) {
  dma_t *dma = proc->dma[dmanum];
  uint32_t status = 0;
  uint32_t ticks = 0;
  while(!status) {
		status = dma_tick(dma, 1);	// status == 0 running, status == 1, stopped for some reason
    ticks++;
  }

#ifdef VERIFICATION
  thread_t * thread=proc->thread[0]; // for warn
	warn("DMA %d: ADAPTER  arch_verif_dma_step ticks=%u", dmanum, ticks);
#endif

}





//! Report an exception happened to an owner thread.
static int dma_adapter_report_exception(dma_t *dma) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	dma_adapter_engine_info_t * dma_info = dma_retrieve_dma_adapter(dma);

	warn("DMA %d: ADAPTER  report_exception PC=%x Syndrome=%d for badva=%x", dma->num, thread->gpr[HEX_REG_PC], dma->error_state_reason, dma_info->einfo.badva0);
//	PRINTF(dma, "DMA %d: Tick %d: ADAPTER  report_exception for badva=%x",dma->num, thread->processor_ptr->monotonic_pcycles, dma_info->einfo.badva0 );

	// If an exception occurred while DMA operations just after DMSTART
	// in the same tick, ELR is set to a PC of DMSTART. Thus, later after
	// the exception is resolved, we come to hit the DMSTART again, which is
	// not correct. Since the exception is exposed to DMPOLL or DMWAIT,
	// elr should be adjusted to be a PC of DMPOLL or DMWAIT.
	dma_info->einfo.elr = thread->gpr[HEX_REG_PC];
	dma_info->einfo.badva1 = thread->t_sreg[HEX_SREG_BADVA1];

	// Take an owner thread an exception.
	//register_einfo(thread, &dma_info->einfo);

	INC_TSTAT(tudma_tlb_miss);

	return 0;
}

//! dmstart implementation.
size4u_t dma_adapter_cmd_start(thread_t *thread, size4u_t new, size4u_t dummy,
                               dma_insn_checker_ptr *insn_checker) {
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma->pc = thread->gpr[HEX_REG_PC];
  // mgl
	PRINTF(dma, "DMA %d: Tick %d: ADAPTER dma_start thread=%u new=0x%08x", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), thread->threadId, new);

  // Call the DMA engine.
  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;
  uint32_t new_ptr = (uint32_t)new;
  dma_cmd_start(dma, new_ptr, &report);

#if 0
  if (report.excpt == 1) {
    // If the command ran into an exception, we have to report it to an owner
    // thread, so that the command can be replayed again later.
    dma_adapter_report_exception(dma);
  } else {
    if (new != 0) {
			fINSERT_BITS(thread->gpr[REG_SSR], reg_field_info[SSR_ASID].width, reg_field_info[SSR_ASID].offset, (GET_SSR_FIELD(SSR_ASID)));
    }
  }
#endif

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

  return 0;
}

//! dmlink and its variants' implementation.
size4u_t dma_adapter_cmd_link(thread_t *thread, size4u_t tail, size4u_t new,
                              dma_insn_checker_ptr *insn_checker) {
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];
	dma->pc = thread->gpr[HEX_REG_PC];
	PRINTF(dma, "DMA %d: Tick: %d ADAPTER dma_link thread=%u tail=0x%08x new=0x%08x", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), thread->threadId, tail, new);

  // Call the DMA engine.
  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  uint32_t tail_ptr = (uint32_t)tail;
  uint32_t new_ptr = (uint32_t)new;
  dma_cmd_link(dma, tail_ptr, new_ptr, &report);
#if 0
  if (report.excpt == 1) {
    // If the command ran into an exception, we have to report it to an owner
    // thread, so that the command can be replayed again later.
    dma_adapter_report_exception(dma);
  } else {
    if (new != 0) {
			fINSERT_BITS(thread->gpr[REG_SSR], reg_field_info[SSR_ASID].width,  reg_field_info[SSR_ASID].offset, (GET_SSR_FIELD(SSR_ASID)));
    }
  }
#endif

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

  return 0;
}

//! dmpoll implementation.
size4u_t dma_adapter_cmd_poll(thread_t *thread, size4u_t dummy1,
                              size4u_t dummy2,
                              dma_insn_checker_ptr *insn_checker) {
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];

#ifdef VERIFICATION
  if (thread->dmpoll_hint_valid) {
    thread->dmpoll_hint_valid = 0;
		warn("DMA %d: ADAPTER dma_poll thread=%u using hint value of %08x instead of calling dma model",  dma->num, thread->threadId, thread->dmpoll_hint_val);
    return thread->dmpoll_hint_val;
  }
#endif


  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  size4u_t dst=0;   // Destination to get DM0 value returned back.
  dma_cmd_poll(dma, &dst, &report);

  if (report.excpt == 1) {
    dma_adapter_report_exception(dma);
		dma_retry_after_exception(dma);
  }

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

	PRINTF(dma, "DMA %d: Tick: %d ADAPTER dma_poll thread=%u dst=%x", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), thread->threadId, dst);
  return dst;
}

//! dmwait implementation.
size4u_t dma_adapter_cmd_wait(thread_t *thread, size4u_t dummy1,
                              size4u_t dummy2,
                              dma_insn_checker_ptr *insn_checker) {
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];



  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  size4u_t dst=0;

  // When a new exception occurred, we already had been in
  // "RUNNING | EXCEPTION | PAUSED". This command call will clear "EXCEPTION".
  // After the exception is resolved, we will revisit this command again.
  // Then, the command call below will clear "PAUSED" too, so that we can move
  // forward.
  dma_cmd_wait(dma, &dst, &report);

  if (report.excpt == 1) {
    // If we reach here, it means we had a new exception, and the command call
    // above cleared "EXCEPTION". Now, we have to report the exception
    // to an owner thread.
    dma_adapter_report_exception(dma);

    // When we reach here, we expect "RUNNING | PAUSED".
    // Later, an exception handler will be brought up, and thanks to replaying,
    // we will hit this command again. But, the DMA ticking interface will be
    // skipped - it will filter out "PAUSED" status not to run.
    // When we revisit this command again by the replying, we will hit the
    // clause below which will set up a staller while waiting for a DMA task
    // completed.

		dma_retry_after_exception(dma);
  }

  (*insn_checker) = report.insn_checker;
//	PRINTF(dma, "DMA %d: Tick: %d ADAPTER  dma_wait dst=%x", thread->threadId, thread->processor_ptr->monotonic_pcycles, dst);

  return dst;
}

//! dmwaitdescriptor implementation.
size4u_t dma_adapter_cmd_waitdescriptor(thread_t *thread,
                                        size4u_t desc,
                                        size4u_t dummy2,
                                        dma_insn_checker_ptr *insn_checker) {
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];



  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  size4u_t dst=0;

  // When a new exception occurred, we already had been in
  // "RUNNING | EXCEPTION | PAUSED". This command call will clear "EXCEPTION".
  // After the exception is resolved, we will revisit this command again.
  // Then, the command call below will clear "PAUSED" too, so that we can move
  // forward.
  uint32_t desc_ptr = (uint32_t)desc;
  dma_cmd_waitdescriptor(dma, desc_ptr, &dst, &report);

  if (report.excpt == 1) {
    // If we reach here, it means we had a new exception, and the command call
    // above cleared "EXCEPTION". Now, we have to report the exception
    // to an owner thread.
    dma_adapter_report_exception(dma);

    // When we reach here, we expect "RUNNING | PAUSED".
    // Later, an exception handler will be brought up, and thanks to replaying,
    // we will hit this command again. But, the DMA ticking interface will be
    // skipped - it will filter out "PAUSED" status not to run.
    // When we revisit this command again by the replying, we will hit the
    // clause below which will set up a staller while waiting for a DMA task
    // completed.
  }

  (*insn_checker) = report.insn_checker;

//	PRINTF(dma, "DMA %d: ADAPTER  dma_waitdescriptor PCYCLE=%llu dst=%x", thread->threadId,thread->processor_ptr->monotonic_pcycles,dst);

  return dst;
}

//! dmpause implementation.
size4u_t dma_adapter_cmd_pause(thread_t *thread, size4u_t dummy1,
                               size4u_t dummy2,
                               dma_insn_checker_ptr *insn_checker) {

  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];



  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  uint32_t dst=0;
  dma_cmd_pause(dma, &dst, &report);

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;
	PRINTF(dma, "DMA %d: ADAPTER dma_pause thread=%u dst=%x", dma->num, thread->threadId, dst);

  return dst;
}

//! dmresume implementation.
size4u_t dma_adapter_cmd_resume(thread_t *thread, size4u_t arg1, size4u_t dummy,
                                dma_insn_checker_ptr *insn_checker) {

  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];

	PRINTF(dma, "DMA %d: Tick: %d ADAPTER dma_resume thread=%u", dma->num, ARCH_GET_PCYCLES(thread->processor_ptr), thread->threadId);
  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  uint32_t ptr = ((uint32_t)arg1);
  dma_cmd_resume(dma, ptr, &report);

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

  return 0;
}

size4u_t dma_adapter_cmd_cfgrd(thread_t *thread,
                               size4u_t index,
                               size4u_t dummy2,
                               dma_insn_checker_ptr *insn_checker)
{
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];



  dma_cmd_report_t report;
    uint32_t val=0;
  report.excpt = 0;
  report.insn_checker = NULL;

    dma_cmd_cfgrd(dma, index, &val, &report);

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

	PRINTF(dma, "DMA %d: ADAPTER  dma_cfgrd thread=%u val=%x", dma->num, thread->threadId, val);

    return val;
}

size4u_t dma_adapter_cmd_cfgwr(thread_t *thread,
                               size4u_t index,
                               size4u_t val,
                               dma_insn_checker_ptr *insn_checker)
{
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];



  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

	if ((index == 5) || (index == 4) || (index == 2)) {
		// DM2,DM4,DM5 are global.
		// Hack unitl proper rewrite, simple write the same value to all
		for (int tnum = 0; tnum < thread->processor_ptr->runnable_threads_max; tnum++) {
      dma_t *dma_ptr = thread->processor_ptr->dma[tnum];
      if (dma_ptr) {
    			dma_cmd_cfgwr(dma_ptr, index, val, &report);
      }
		}
	} else {
    dma_cmd_cfgwr(dma, index, val, &report);
	}


  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

	PRINTF(dma, "DMA %d: ADAPTER  dma_cfgwr thread=%u val=%x", dma->num, thread->threadId, val);

    return val;
}
void dma_adapter_set_dm5(dma_t *dma, uint32_t addr) {
	thread_t* thread = dma_adapter_retrieve_thread(dma);
	for (int tnum = 0; tnum < thread->processor_ptr->runnable_threads_max; tnum++) {
		dma_t *dma_ptr = thread->processor_ptr->dma[thread->threadId];
    if (dma_ptr) {
  		PRINTF(dma_ptr, "DMA %d: ADAPTER  forcing dm5 on all DMA engines thread=%u val=%x", dma_ptr->num, thread->threadId, addr);
	  	dma_cmd_cfgwr(dma_ptr, 5, addr, NULL);
    }
	}
}


size4u_t dma_adapter_cmd(thread_t *thread, dma_cmd_t opcode,
                         size4u_t arg1, size4u_t arg2) {
#if 0
	exception_info einfo = {0};     // Storage to retrieve an exception if it occurs.
  /* Check if DMA is present. If there is none, just do a noop */
	if(!thread->processor_ptr->arch_proc_options->QDSP6_DMA_PRESENT) {
		if(!sys_in_monitor_mode(thread)) {
			decode_error(thread, &einfo, PRECISE_CAUSE_INVALID_PACKET);
			register_einfo(thread,&einfo);
		}
		return 0;
	}
#endif
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];

  size4u_t ret_val = 0;
  if (opcode < DMA_CMD_UND) {

		// Call an instruction implementation at the adapter side.warn("DMA %d: ADAPTER  dma_adapter_set_staller. tick count=%d", dma->num, dma_get_tick_count(dma));
    dma_adapter_cmd_impl_t impl = dma_adapter_cmd_impl_tab[opcode];


    if (impl != NULL) {
      // If the engine wants to stall, it will give us an instruction
      // completeness checker back.
      dma_insn_checker_ptr insn_checker = NULL;

      ret_val = (*impl)(thread, arg1, arg2, &insn_checker);

      // If the engine gave us an instruction completeness checker, let's set up
      // a staller.
      if (insn_checker != NULL) {
				PRINTF(dma, "DMA %d: ADAPTER  instruction %d setting staller", dma->num, opcode);
        dma_adapter_set_staller(dma, insn_checker);
      }
		} else {
			PRINTF(dma, "DMA %d: ADAPTER  instruction %d is not implemented yet", dma->num, opcode);
    }
  }

  return ret_val;
}

size4u_t dma_adapter_cmd_syncht(thread_t *thread, size4u_t dummy1, size4u_t dummy2,
                                dma_insn_checker_ptr *insn_checker) {
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];

	PRINTF(dma, "DMA %d: ADAPTER  dma_syncht thread=%u", dma->num, thread->threadId);
  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  dma_cmd_syncht(dma, dummy1, dummy2, &report);

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

  return 0;
}

size4u_t dma_adapter_cmd_tlbsynch(thread_t *thread, size4u_t dummy1, size4u_t dummy2,
                                dma_insn_checker_ptr *insn_checker) {
  // Obtain a current DMA instance from a thread ID.
  dma_t *dma = thread->processor_ptr->dma[thread->threadId];

	PRINTF(dma, "DMA %d: ADAPTER  dma_tlbsynch thread=%u", dma->num, thread->threadId);
  dma_cmd_report_t report;
  report.excpt = 0;
  report.insn_checker = NULL;

  dma_cmd_tlbsynch(dma, dummy1, dummy2, &report);

  // We should relay the instruction completeness checker.
  (*insn_checker) = report.insn_checker;

  return 0;
}

void dma_adapter_pmu_increment(dma_t *dma, int event, int increment) {
	if (increment && (event < DMA_PMU_NUM)) {
  thread_t* thread = dma_adapter_retrieve_thread(dma);
  INC_TSTATN(pmu_event_mapping[event], increment);
	if ((event == 0) && (thread->timing_on)){
		//dma_adapter_active_uarch_callback(dma, 0);

	}
}
}
int dma_adapter_get_insn_latency(dma_t *dma, int index) {
	return 0; //dma_adapter_cmd_latency[index];

}

void dma_adapter_active_uarch_callback(dma_t *dma, int lvl) {
#if 0
	thread_t* thread = dma_adapter_retrieve_thread(dma);

	if (thread->processor_ptr->arch_proc_options->uarch_udma_active_level ==  lvl) {
		CALLBACK(thread->processor_ptr->options->sim_active_callback, thread->processor_ptr->system_ptr, thread->processor_ptr, dma->num, UARCH_UDMA_ACTIVE);
	}
#endif
}

