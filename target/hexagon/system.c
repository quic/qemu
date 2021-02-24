/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

/*
* system architecture
 *
 * $Id: system.c,v
 *
 */

//#include "thread.h"
//#include "arch.h"
#ifndef CONFIG_USER_ONLY
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#endif
#include <assert.h>
#include "macros.h"
#include "mmvec/mmvec.h"
#include "system.h"
//#include "external_api.h"
//#include "iic.h"
//#include "uarch/uarch.h"

#define thread_t CPUHexagonState

#ifdef VERIFICATION
#include "ver_external_api.h"
#include "ver_exec.h"
#endif

#ifdef ZEBU_CHECKSUM_TRACE
#include "zebu_external_api.h"
#include "zebu_exec.h"
#endif


#include "string.h"
//#include "memwrap.h"

//#include "pmu.h"
//#include "isdb.h"

//#include "walk/walk.h"

//#include "clade_if.h"
//#include "clade2_if.h"

#define TLBGUESSIDX(VA) ( ((VA>>12)^(VA>>22)) & (MAX_TLB_GUESS_ENTRIES-1))

extern const size1u_t insn_allowed_uslot[][4];
extern const size1u_t insn_sitype[];
extern const size1u_t sitype_allowed_uslot[][4];
extern const char* sitype_name[];

void iic_flush_cache(processor_t * proc)

{
}

#ifdef FIXME
enum {
#define DEF_CACHEABILITY_ENTRY(NUM,L1_D_BEH,L1_I_BEH,L2_BEH) CCCC_##NUM,
#include "cacheability.odef"
#undef CACHEABILITY_ENTRY
};

/*
 * Remove this thread from the k0lock queue
 * This is safe to call even if we're not waiting in the queue
 */
void k0lock_queue_cancel(thread_t *thread)
{
	int id = thread->threadId;
	processor_t *proc = thread->processor_ptr;
	int i;
	for (i = 0; i < THREADS_MAX-1; i++) {
		/* Keep bumping my id to the end of the queue... */
		if (proc->k0lock_queue[i] == id) {
			proc->k0lock_queue[i] = proc->k0lock_queue[i+1];
			proc->k0lock_queue[i+1] = id;
		}
	}
	/* If my ID at the end, eliminate it */
	if (proc->k0lock_queue[THREADS_MAX-1] == id) proc->k0lock_queue[THREADS_MAX-1] = 0xFF;
}

/*
 * Add this thread to the k0lock queue
 * Checks for queue full and thread already waiting
 */
void k0lock_queue_append(thread_t *thread)
{
	int id = thread->threadId;
	processor_t *proc = thread->processor_ptr;
	int i;
	for (i = 0; i < THREADS_MAX; i++) {
		if (proc->k0lock_queue[i] == id) {
			panic("thread %d already in the k0lock queue!",id);
		}
		if (proc->k0lock_queue[i] == 0xFF) {
			proc->k0lock_queue[i] = id;
			return;
		}
	}
	panic("no room in the k0lock queue for thread\n");
}

/* Returns 1 if we are at the head of the queue, moves queue
 * Returns 0 otherwise and queue doesn't change
 */
int k0lock_queue_trypop(thread_t *thread)
{
	int id = thread->threadId;
	processor_t *proc = thread->processor_ptr;
	int i;
	if (proc->k0lock_queue[0] != id) return 0;
	for (i = 0; i < THREADS_MAX-1; i++) {
		proc->k0lock_queue[i] = proc->k0lock_queue[i+1];
	}
	proc->k0lock_queue[THREADS_MAX-1] = 0xFF;
	return 1;
}

int sys_k0lock_queue_empty(thread_t *thread)
{
	return (thread->processor_ptr->k0lock_queue[0] == 0xFF);
}

int sys_k0lock_queue_ready(thread_t *thread)
{
	if (sys_k0lock_queue_empty(thread)) return 1;
	return k0lock_queue_trypop(thread);
}

void k0lock_queue_init(processor_t *proc)
{
	int i;
	for (i = 0; i < THREADS_MAX; i++) {
		proc->k0lock_queue[i] = 0xFF;
	}
}
static inline void sys_coreready(thread_t *thread, int l2vicnum, int ready)
{
	CALLBACK(thread->processor_ptr->options->coreready_callback,
		 thread->system_ptr, thread->processor_ptr,
		 thread->threadId, (l2vicnum << 4) | ready);
}

void sys_ciad(thread_t * thread, size4u_t val)
{
	/* EJP: FIXME: coreready callback uses info field for which l2vic interface */
	if (fGETBIT(2, val)) sys_coreready(thread,0,1);
	if (fGETBIT(3, val)) sys_coreready(thread,1,1);
	if (fGETBIT(4, val)) sys_coreready(thread,2,1);
	if (fGETBIT(5, val)) sys_coreready(thread,3,1);
}

void sys_siad(thread_t * thread, size4u_t val)
{
	if (fGETBIT(2, val)) sys_coreready(thread,0,0);
	if (fGETBIT(3, val)) sys_coreready(thread,1,0);
	if (fGETBIT(4, val)) sys_coreready(thread,2,0);
	if (fGETBIT(5, val)) sys_coreready(thread,3,0);
}

int staller_cu_locked(thread_t * thread)
{
	if ((thread->cu_tlb_lock_waiting == 0)){
		thread->status &= ~EXEC_STATUS_REPLAY;
		return 0;
	}
	return 1;
}

static inline int sys_k0lock_queue_athead(thread_t *thread)
{
	return (thread->processor_ptr->k0lock_queue[0] == thread->threadId);
}

int staller_k0locked(thread_t * thread)
{
	int stalled = 1;
	/* first check if kernel is still locked, then check cu_locked (tlb lock) */
	if (fREAD_GLOBAL_REG_FIELD(SYSCONF, SYSCFG_K0LOCK) == 0) {
		if (sys_k0lock_queue_athead(thread)) {
			stalled = staller_cu_locked(thread);
		} else {
			warn("thread %d not at the head of the k0lock queue...", thread->threadId);
		}
	}
	return stalled;
}

void sys_waiting_for_tlb_lock(thread_t * thread)
{
	thread->cu_tlb_lock_waiting = 1;
	STALL_SET(cu_locked);
	return;
}

void sys_waiting_for_k0_lock(thread_t * thread)
{
	k0lock_queue_append(thread);
	STALL_SET(k0locked);
	return;
}

static inline int sys_thread_is_k0lock_stalled(thread_t *thread)
{
	if ((thread->staller == staller_k0locked)
		&& (thread->status & EXEC_STATUS_REPLAY)) return 1;
	return 0;
}

void sys_initiate_clear_k0_lock(thread_t * thread)
{
	//fLOG_GLOBAL_REG_FIELD(SYSCONF, SYSCFG_K0LOCK, 0);
	fWRITE_GLOBAL_REG_FIELD(SYSCONF, SYSCFG_K0LOCK, 0);
	return;
}


void sys_recalc_num_running_threads(processor_t * proc)
{
	int i, j, all_wait = 1;
	size4u_t running = ((proc->global_regs[REG_MODECTL] & 0xffff) &	/* Run bit set */
					   (~((proc->global_regs[REG_MODECTL] >> 16) & 0xffff)));	/* And not waiting */

	/* Take population count for # of running threads */
	running = (running & 0x5555) + ((running >> 1) & 0x5555);
	running = (running & 0x3333) + ((running >> 2) & 0x3333);
	running = (running & 0x0f0f) + ((running >> 4) & 0x0f0f);
	running = (running & 0x00ff) + ((running >> 8) & 0x00ff);
	proc->num_running_threads = running;
	UPDATE_UARCH_REG_MODECTL(proc, proc->global_regs[REG_MODECTL]);
	if (proc->global_regs[REG_MODECTL] == 0) {
		/* nothing running or waiting, set the allstop variable */
		if (proc->allstop_ptr) {
			*proc->allstop_ptr = 1;
		}
	}

	for (i = 0; i < proc->runnable_threads_max; i++) {
		if (THREAD_IS_ON(proc, i))
		{
			thread_t *thread = proc->thread[i];
			thread->iswait = fGET_WAIT_MODE(i);
			thread->ison = fGET_RUN_MODE(i);
			/* FIXME: should this just be a check if running == 0? */
			all_wait = all_wait && thread->iswait;
		}
	}
	proc->all_wait = all_wait;
	if ((running == 0) && (proc->arch_proc_options->all_wait_clobber)) {
		for (i = 0; i < proc->runnable_threads_max; i++) {
			if (THREAD_IS_ON(proc, i))
			{
				for (j = 0; j < 32; j++) {
					arch_set_thread_reg(proc,i,j,0xe0ffdead);
				}
			}
		}
	}
}

int is_du_badva_affecting_exception(int type, int cause)
{
	if (type == EXCEPT_TYPE_TLB_MISS_RW) {
		return 1;
	}
	if ((type == EXCEPT_TYPE_PRECISE) && (cause >= 0x20)
		&& (cause <= 0x28)) {
		return 1;
	}
	return (0);
}

void
register_exception_info(thread_t * thread, size4u_t type, size4u_t cause,
						size4u_t badva0, size4u_t badva1, size4u_t bvs,
						size4u_t bv0, size4u_t bv1, size4u_t elr,
						size4u_t diag, size4u_t de_slotmask)
{
#ifdef VERIFICATION
	warn("Oldtype=%d oldcause=0x%x newtype=%d newcause=%x de_slotmask=%x diag=%x", thread->einfo.type, thread->einfo.cause, type, cause, de_slotmask, diag);
#endif
	if ((EXCEPTION_DETECTED)
		&& (thread->einfo.type == EXCEPT_TYPE_TLB_MISS_RW)
		&& ((type == EXCEPT_TYPE_PRECISE)
			&& ((cause == 0x28) || (cause == 0x29)))) {
		warn("Footnote in v2 System Architecture Spec 5.1 says: TLB miss RW has higher priority than multi write / bad cacheability");
	}

	else if ((EXCEPTION_DETECTED) && (thread->einfo.cause == PRECISE_CAUSE_BIU_PRECISE) && (cause == PRECISE_CAUSE_REG_WRITE_CONFLICT)) {
		warn("RTL Takes Multi-write before BIU, overwriting BIU");
		thread->einfo.type = type;
		thread->einfo.cause = cause;
		thread->einfo.badva0 = badva0;
		thread->einfo.badva1 = badva1;
		thread->einfo.bvs = bvs;
		thread->einfo.bv0 = bv0;
		thread->einfo.bv1 = bv1;
		thread->einfo.elr = elr;
		thread->einfo.diag = diag;
	} else if ((EXCEPTION_DETECTED) && (thread->einfo.bv1 && bv0)  &&
			/*We've already seen a slot1 exception */
			   is_du_badva_affecting_exception(thread->einfo.type,
							   thread->einfo.cause)
			   && is_du_badva_affecting_exception(type, cause)) {

		/* We've already seen a slot1 D-side exception, so only
		   need to record the BADVA0 info */
		thread->einfo.badva0 = badva0;
		thread->einfo.bv0 = bv0;
	} else if ((!EXCEPTION_DETECTED) || (type < thread->einfo.type)) {
		SET_EXCEPTION;
		thread->einfo.type = type;
		thread->einfo.cause = cause;
		thread->einfo.badva0 = badva0;
		thread->einfo.badva1 = badva1;
		thread->einfo.bvs = bvs;
		thread->einfo.bv0 = bv0;
		thread->einfo.bv1 = bv1;
		thread->einfo.elr = elr;
		thread->einfo.diag = diag;
		thread->einfo.de_slotmask |= de_slotmask;
	} else if ((type == thread->einfo.type)
			   && (cause < thread->einfo.cause)) {
		thread->einfo.cause = cause;
		thread->einfo.badva0 = badva0;
		thread->einfo.badva1 = badva1;
		thread->einfo.bvs = bvs;
		thread->einfo.bv0 = bv0;
		thread->einfo.bv1 = bv1;
		thread->einfo.elr = elr;
		thread->einfo.diag = diag;
	} else if ((type == thread->einfo.type)
			   && (cause == thread->einfo.cause)
			   && (cause == PRECISE_CAUSE_DOUBLE_EXCEPT)) {
		if ((de_slotmask == 0)
			|| (thread->einfo.de_slotmask < de_slotmask)) {
			if (diag < thread->einfo.diag) {
				warn("Picking better proiroty root exception cause for DIAG: 0x%x", diag);
				thread->einfo.diag = diag;
			} else {
				warn("Not selecting lower priority DIAG: 0x%x", diag);
			}
			thread->einfo.de_slotmask |= de_slotmask;
		} else {
			warn("Trying to avoid slot0 DE in the presence of a slot1 DE, not setting slot0 DIAG of 0x%x", diag);
		}
	} else {
		/* do nothing, lower prio */
	}
}

void register_einfo(thread_t *thread, exception_info *einfo)
{
	int register_double_exception = (GET_SSR_FIELD(SSR_EX)>0);

	// Imprecise can't cause double exception, but TB doesn't check anything on imprecise exception
	// Precise, but higher priority than double, can't cause a double
	if ((einfo->type == EXCEPT_TYPE_PRECISE) && (einfo->cause < PRECISE_CAUSE_DOUBLE_EXCEPT))
		register_double_exception = 0;

	if (register_double_exception) {
		warn("Double Exception (from: type=%x cause=%x elr=%x)",einfo->type,einfo->cause,einfo->elr);
		register_exception_info(thread,EXCEPT_TYPE_PRECISE,PRECISE_CAUSE_DOUBLE_EXCEPT,
			thread->Regs[REG_BADVA0],thread->Regs[REG_BADVA1],
			GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0), GET_SSR_FIELD(SSR_V1),
			einfo->elr, einfo->cause, einfo->de_slotmask);
	} else {
		warn("Registering exception info: type=%x cause=%x elr=%x badva0=%x badva1=%x bvs=%d",
			einfo->type,einfo->cause,einfo->elr,einfo->badva0,einfo->badva1,einfo->bvs);
		register_exception_info(thread,einfo->type,einfo->cause,einfo->badva0,
			einfo->badva1, einfo->bvs,einfo->bv0,einfo->bv1,einfo->elr,
			thread->processor_ptr->global_regs[REG_DIAG],0);
	}
}

void rewind_exception_info(thread_t * thread)
{
	if (!EXCEPTION_DETECTED) {
		/* Nothing to return */
		return;
	}
	CLEAR_EXCEPTION;
	/* Clear out exception info for the next packet */
	memset(&thread->einfo, 0, sizeof(thread->einfo));
}

/* Spec says:
   Both SSR[V0] and SSR[V1] get set whenever either slot takes an exception and both
   slots contain valid and non-predicate-cancelled memory instructions.

   So, for packets with dual access, we need to make sure both BADVA0 and BADVA1
   are set even if only one of them had the exception. */
void
sys_set_badva_state_rwprecise(thread_t *thread)
{
	if (((thread->einfo.type == EXCEPT_TYPE_TLB_MISS_RW) ||
		 ((thread->einfo.type == EXCEPT_TYPE_PRECISE) &&
		  (thread->einfo.cause >= 0x20) && (thread->einfo.cause <= 0x28)))

		&& thread->last_pkt->double_access	/* double access */
		&& ((thread->last_pkt->slot_cancelled & 3) == 0)) {	/* both slots non-cancelled */

		thread->Regs[REG_BADVA0] = thread->mem_access[0].bad_vaddr;
		thread->Regs[REG_BADVA1] = thread->mem_access[1].bad_vaddr;
		SET_SSR_FIELD(SSR_V0, 1);
		SET_SSR_FIELD(SSR_V1, 1);
	}
}

void
sys_set_ssr_cause_code(thread_t * thread, int cause)
{
    thread->Regs[REG_SSR] &= ~(0xff);	/* Clear CAUSE, then set */
    thread->Regs[REG_SSR] |= (cause & 0xff);
    thread->t_mmvecx = set_thread_mmvecx(thread);
	thread->t_veclogsize = set_thread_veclogsize(thread);
}



static inline int commit_guest_event(thread_t *thread)
{

	fSET_FIELD(thread->Regs[REG_GSR],GSR_UM,!fREAD_REG_FIELD(SSR,SSR_GM));
	fSET_FIELD(thread->Regs[REG_GSR],GSR_IE,fREAD_REG_FIELD(CCR,CCR_GIE));
	fSET_FIELD(thread->Regs[REG_GSR],GSR_SS,fREAD_REG_FIELD(SSR,SSR_SS));
	fSET_FIELD(thread->Regs[REG_GSR],GSR_CAUSE,thread->einfo.cause);

	thread->lockvalid = 0;
	CLEAR_EXCEPTION;

	thread->Regs[REG_GELR] = thread->einfo.elr;
	SET_SSR_FIELD(SSR_SS,0);
	SET_SSR_FIELD(SSR_GM,1);
	fWRITE_REG_FIELD(CCR,CCR_GIE,0);
	thread->Regs[REG_PC] = fREAD_GEVB()|(thread->einfo.type << 2);

#ifdef VERIFICATION
    warn("commit_guest_event einfo.cause=%x gsr=%x gelr=%x", thread->einfo.type, thread->Regs[REG_GSR], thread->einfo.elr);
	if (thread->processor_ptr->options->exception_callback)
          thread->processor_ptr->options->exception_callback(NULL,NULL,0,0,0,0,0);
        rewind_exception_info(thread);
#endif
	return 1;
}

static inline int try_commit_error_to_guest(thread_t *thread)
{
	if (thread->einfo.cause < 0x10) return 0;
	if (thread->einfo.cause > 0x2f) return 0;
	if (fREAD_REG_FIELD(CCR,CCR_GEE) == 0) return 0;

    thread->lockvalid = 0;

    /* Set GBADVA on events that set BADVA */
    if ((thread->einfo.cause >= 0x20) && (thread->einfo.cause <= 0x2f))
    {
        thread->Regs[REG_GBADVA] = thread->einfo.bvs ? thread->einfo.badva1 : thread->einfo.badva0;
    }


    SET_SSR_FIELD(SSR_BVS, thread->einfo.bvs);
	SET_SSR_FIELD(SSR_V0, thread->einfo.bv0);
	SET_SSR_FIELD(SSR_V1, thread->einfo.bv1);
    thread->Regs[REG_BADVA0] = thread->einfo.badva0;
	thread->Regs[REG_BADVA1] = thread->einfo.badva1;

	sys_set_badva_state_rwprecise(thread);

    sys_set_ssr_cause_code(thread, thread->einfo.cause);

	return commit_guest_event(thread);
}

static inline int try_commit_trap0_to_guest(thread_t *thread)
{
	if (fREAD_REG_FIELD(CCR,CCR_GTE) == 0) return 0;
    sys_set_ssr_cause_code(thread, thread->einfo.cause);
	return commit_guest_event(thread);
}

static inline int try_commit_fptrap_to_guest(thread_t *thread)
{
	if (fREAD_REG_FIELD(CCR,CCR_GEE) == 0) return 0;
    sys_set_ssr_cause_code(thread, thread->einfo.cause);
	return commit_guest_event(thread);
}

static inline int try_commit_debug_to_guest(thread_t *thread)
{
	if (fREAD_REG_FIELD(CCR,CCR_GEE) == 0) return 0;
    sys_set_ssr_cause_code(thread, thread->einfo.cause);
	return commit_guest_event(thread);
}

static inline int try_commit_exception_to_guest(thread_t *thread)
{
#ifdef VERIFICATION
    warn("try_commit_exception_to_guest. einfo.cause=%x CCR=%x", thread->einfo.type, thread->Regs[REG_CCR]);
#endif

	if (sys_in_monitor_mode(thread)) return 0;
	switch (thread->einfo.type) {
	case EXCEPT_TYPE_RESET: return 0;
	case EXCEPT_TYPE_IMPRECISE: return 0;
	case EXCEPT_TYPE_PRECISE: return try_commit_error_to_guest(thread);
	case EXCEPT_TYPE_TLB_MISS_X: return 0;
	case EXCEPT_TYPE_TLB_MISS_RW: return 0;
	case EXCEPT_TYPE_TRAP0: return try_commit_trap0_to_guest(thread);
	case EXCEPT_TYPE_TRAP1: return 0;
	case EXCEPT_TYPE_FPTRAP: return try_commit_fptrap_to_guest(thread);
	case EXCEPT_TYPE_DEBUG: return try_commit_debug_to_guest(thread);
	default: fatal("unexpected exception type"); return 0;
	}
}


void commit_exception_info(thread_t * thread)
{
	if (!EXCEPTION_DETECTED) {
		fatal("Called commit_exception_info when no exception detected");
	}

    bool is_trap_exception = (thread->einfo.type == EXCEPT_TYPE_TRAP0) || (thread->einfo.type == EXCEPT_TYPE_TRAP1);

	if (thread->processor_ptr->options->exception_precommit_callback) {
		thread->processor_ptr->options->exception_precommit_callback(thread->system_ptr,
		   thread->processor_ptr, thread->threadId, thread->einfo.type, thread->einfo.cause,
		   thread->einfo.bvs?(thread->einfo.badva1):(thread->einfo.badva0), thread->einfo.elr);
	}

	if (try_commit_exception_to_guest(thread)) return;

	thread->lockvalid = 0;
	thread->fetch_samepage = 0;
	CLEAR_EXCEPTION;
	thread->Regs[REG_ELR] = thread->einfo.elr;
	thread->Regs[REG_SSR] &= ~(0xff);	/* Clear CAUSE, then set */
	thread->Regs[REG_BADVA0] = thread->einfo.badva0;
	thread->Regs[REG_BADVA1] = thread->einfo.badva1;
	SET_SSR_FIELD(SSR_BVS, thread->einfo.bvs);
	SET_SSR_FIELD(SSR_V0, thread->einfo.bv0);
	SET_SSR_FIELD(SSR_V1, thread->einfo.bv1);
	thread->processor_ptr->global_regs[REG_DIAG] = thread->einfo.diag;

	sys_set_badva_state_rwprecise(thread);

	if (((thread->einfo.type == EXCEPT_TYPE_TRAP0) ||
		 (thread->einfo.type == EXCEPT_TYPE_TRAP1) ||
		 (thread->einfo.type == EXCEPT_TYPE_FPTRAP)) &&
		GET_SSR_FIELD(SSR_EX)) {
		/* Commit trap as double exception w/ ELR=NPC */
		warn("Commit of trap/fptrap caused double exception. einfo.cause=%x\n",thread->einfo.cause);
		thread->Regs[REG_SSR] |= (PRECISE_CAUSE_DOUBLE_EXCEPT);
		thread->Regs[REG_PC] = fREAD_CURRENT_EVB()|(EXCEPT_TYPE_PRECISE << 2);
		thread->processor_ptr->global_regs[REG_DIAG] = (thread->einfo.cause & 0xff);
	} else {
		thread->Regs[REG_SSR] |= (thread->einfo.cause & 0xff);
		thread->Regs[REG_PC] = fREAD_CURRENT_EVB()|(thread->einfo.type << 2);
	}
	thread->Regs[REG_SSR] |= (1 << 17);	/* EX */
	if (thread->processor_ptr->options->exception_callback) {
		thread->processor_ptr->options->exception_callback(thread->system_ptr,
		   thread->processor_ptr, thread->threadId, thread->einfo.type, thread->einfo.cause,
		   thread->einfo.bvs?(thread->einfo.badva1):(thread->einfo.badva0), thread->einfo.elr);
	}

	/* Clear out exception info for the next packet */
	memset(&thread->einfo, 0, sizeof(thread->einfo));

#if defined(VERIFICATION) || defined(ZEBU_CHECKSUM_TRACE)
	ver_rewind_shadow(thread);
#endif
#ifdef VERIFICATION
	isdb_check(thread->processor_ptr, thread->threadId);
#endif

	if(thread->timing_on && !is_trap_exception) {
		uarch_update_iss_interrupt(thread->processor_ptr, thread->threadId, thread->Regs[REG_PC]);
	}
}

void
register_error_exception(thread_t * thread, size4u_t error_code,
						 size4u_t badva0, size4u_t badva1, size4u_t bvs,
						 size4u_t bv0, size4u_t bv1, size4u_t slotmask)
{
	warn("Error exception detected, tnum=%d code=0x%x pc=0x%x badva0=0x%x badva1=0x%x, bvs=%x, Pcycle=%lld msg=%s\n", thread->threadId, error_code, thread->Regs[REG_PC], badva0, badva1, bvs, thread->processor_ptr->monotonic_pcycles, thread->exception_msg ? thread->exception_msg : "");
	thread->exception_msg = NULL;
	if ((error_code > PRECISE_CAUSE_DOUBLE_EXCEPT)
		&& GET_SSR_FIELD(SSR_EX)) {
		warn("Double Exception...");
		register_exception_info(thread, EXCEPT_TYPE_PRECISE,
								PRECISE_CAUSE_DOUBLE_EXCEPT,
								thread->Regs[REG_BADVA0],
								thread->Regs[REG_BADVA1],
								GET_SSR_FIELD(SSR_BVS),
								GET_SSR_FIELD(SSR_V0),
								GET_SSR_FIELD(SSR_V1),
								thread->Regs[REG_PC], error_code,
								slotmask);
		return;
	}

	register_exception_info(thread, EXCEPT_TYPE_PRECISE, error_code,
							badva0, badva1, bvs, bv0, bv1,
							thread->Regs[REG_PC],
							thread->processor_ptr->global_regs[REG_DIAG],
							0);
}

void
register_imprecise_exception(thread_t * thread, int cause, size1u_t diag)
{
	warn("**Imprecise** Error exception detected, tnum=%d pc=0x%x diag=0x%x Pcycle=%lld msg=%s\n", thread->threadId, thread->Regs[REG_PC], diag, thread->processor_ptr->monotonic_pcycles, thread->exception_msg ? thread->exception_msg : "");
	register_exception_info(thread, EXCEPT_TYPE_IMPRECISE, cause,
							thread->Regs[REG_BADVA0],
							thread->Regs[REG_BADVA1],
							GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0),
							GET_SSR_FIELD(SSR_V1), thread->Regs[REG_PC],
							diag, 0);
}

void register_debug_exception(thread_t * thread, size4u_t cause)
{
	warn("Single Step Debug Exception, tnum=%d npc=%x\n",
		 thread->threadId, thread->Regs[REG_PC]);
	register_exception_info(thread, EXCEPT_TYPE_DEBUG, cause,
							thread->Regs[REG_BADVA0],
							thread->Regs[REG_BADVA1],
							GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0),
							GET_SSR_FIELD(SSR_V1), thread->Regs[REG_PC],
							thread->processor_ptr->global_regs[REG_DIAG],
							0);
}

void register_fp_exception(thread_t * thread, size4u_t cause)
{
	warn("Floating Point Exception, tnum=%d npc=%x\n",
		 thread->threadId, thread->Regs[REG_PC]);
	register_exception_info(thread, EXCEPT_TYPE_FPTRAP, cause,
							thread->Regs[REG_BADVA0],
							thread->Regs[REG_BADVA1],
							GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0),
							GET_SSR_FIELD(SSR_V1), thread->Regs[REG_PC],
							thread->processor_ptr->global_regs[REG_DIAG],
							0);
}

void register_coproc_exception(thread_t * thread)
{
	warn("Coprocessor Exception, tnum=%d npc=%x\n",
		 thread->threadId, thread->Regs[REG_PC]);
	register_error_exception(thread, PRECISE_CAUSE_NO_COPROC_ENABLE,
							 thread->Regs[REG_BADVA0],
							 thread->Regs[REG_BADVA1],
							 GET_SSR_FIELD(SSR_BVS),
							 GET_SSR_FIELD(SSR_V0),
							 GET_SSR_FIELD(SSR_V1), 0);
}

void register_coproc2_exception(thread_t * thread)
{
	warn("Secondary Coprocessor Exception, tnum=%d npc=%x\n",
		 thread->threadId, thread->Regs[REG_PC]);
	register_error_exception(thread, PRECISE_CAUSE_NO_COPROC2_ENABLE,
							 thread->Regs[REG_BADVA0],
							 thread->Regs[REG_BADVA1],
							 GET_SSR_FIELD(SSR_BVS),
							 GET_SSR_FIELD(SSR_V0),
							 GET_SSR_FIELD(SSR_V1), 0);
}
#endif

void register_coproc_ldst_exception(thread_t * thread, int slot, size4u_t badva)
{
#ifdef FIXME
	warn("Coprocessor LDST Exception, tnum=%d npc=%x\n", thread->threadId, thread->Regs[REG_PC]);
	if (slot == 0) {
		register_error_exception(thread, PRECISE_CAUSE_COPROC_LDST,
			 badva,
			 thread->Regs[REG_BADVA1],
			 0 /* select 0 */,
			 1 /* set bv0 */,
			 0 /* clear bv1 */, 1<<slot);
	} else {
		register_error_exception(thread,PRECISE_CAUSE_COPROC_LDST,
			thread->Regs[REG_BADVA0],
			badva,
			1 /* select 1 */,
			0 /* clear bv0 */,
			1 /* set bv1 */, 1<<slot);
	}
#else
    printf("ERROR: register_coproc_ldst_exception not implemented\n");
    g_assert_not_reached();
#endif
}

#ifdef FIXME
void
register_guest_trap_exception(thread_t * thread, size4u_t elr, int trapno, int imm)
{
}

void
register_trap_exception(thread_t * thread, size4u_t elr, int trapno, int imm)
{
    // TODO: BQ Work, not sure how to handle this yet
    // Maybe we just bypass fTRAP macro or split this into two steps and
    // register exception separately
    if (!thread->bq_on)
    {
        if (trapno == 0) {
            CALLBACK(thread->processor_ptr->options->trap0_callback,
                     thread->system_ptr, thread->processor_ptr,
                     thread->threadId, imm);
        } else {
		thread->trap1_info = TRAP1; /* Logging for uarchtrace/verif */
            CALLBACK(thread->processor_ptr->options->trap1_callback,
                     thread->system_ptr, thread->processor_ptr,
                     thread->threadId, imm);
        }
    }
	register_exception_info(thread, EXCEPT_TYPE_TRAP0 + trapno, imm,
							thread->Regs[REG_BADVA0],
							thread->Regs[REG_BADVA1],
							GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0),
							GET_SSR_FIELD(SSR_V1), elr,
							thread->processor_ptr->global_regs[REG_DIAG],
							0);

}

void
register_tlb_missx_exception(thread_t * thread, size4u_t badva,
							 int access_type)
{
	int cause;
	size4u_t badva_val;
	warn("TLB miss-X exception detected on tnum=%d PC:%x, badva=0x%x cycle=%lld\n", thread->threadId, thread->Regs[REG_PC], badva, thread->processor_ptr->monotonic_pcycles);
	if (GET_SSR_FIELD(SSR_EX)) {
		warn("Double Exception...");
		if (access_type == TYPE_ICINVA) {
			cause = 0x62;
		} else {
			cause = (((thread->Regs[REG_PC] & 0x0ff0) == 0x0ff0) &&
					 ((badva & 0x0ff0) == 0)
					 && (thread->Regs[REG_PC] < badva));
			cause |= 0x60;
		}
		register_exception_info(thread, EXCEPT_TYPE_PRECISE,
								PRECISE_CAUSE_DOUBLE_EXCEPT,
								thread->Regs[REG_BADVA0],
								thread->Regs[REG_BADVA1],
								GET_SSR_FIELD(SSR_BVS),
								GET_SSR_FIELD(SSR_V0),
								GET_SSR_FIELD(SSR_V1),
								thread->Regs[REG_PC], cause, 0);

		return;
	}
	if (access_type == TYPE_ICINVA) {
		cause = 0x62;
		badva_val = badva & 0xFFFFF000;

		register_exception_info(thread, EXCEPT_TYPE_TLB_MISS_X, cause,
								badva_val, thread->Regs[REG_BADVA1],
								0 /* select badva0 */ ,
								1 /*Set bv0 */ , 0 /*clear v1 */ ,
								thread->Regs[REG_PC], thread->processor_ptr->global_regs[REG_DIAG], 0);	/* ELR */

	} else {					/* This is a TLBmissX from  a normal fetch */

		/* 1 if next page ... PC must be near the end of a 4k boundary and badva must
		 * be on the other side of the boundary and they must not be equal ... we used
		 * to be able to check for unequal, but now we can be looking at different halves
		 * at different points in time... */
		cause = (((thread->Regs[REG_PC] & 0x0ff0) == 0x0ff0) &&
				 ((badva & 0x0ff0) == 0)
				 && (thread->Regs[REG_PC] < badva));
		cause |= 0x60;

		/* BADVA stuff is unchanged */
		register_exception_info(thread, EXCEPT_TYPE_TLB_MISS_X, cause, thread->Regs[REG_BADVA0], thread->Regs[REG_BADVA1], GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0), GET_SSR_FIELD(SSR_V1), thread->Regs[REG_PC], thread->processor_ptr->global_regs[REG_DIAG], 0);	/* ELR */
	}


	thread->processor_ptr->tlbmiss++;
    if (!thread->bq_on) {
        INC_TSTAT(ttlbmissx);
    }
}

void
register_tlb_missrw_exception(thread_t * thread, size4u_t badva,
							  int access_type, int slot)
{
	warn("TLB miss-RW exception detected on tnum=%d PC:%x, slot=%x, badva=0x%x access_type=%c cycle=%lld\n", thread->threadId, thread->Regs[REG_PC], slot, badva, access_type, thread->processor_ptr->monotonic_pcycles);
	if (GET_SSR_FIELD(SSR_EX)) {
		warn("Double Exception...");
		register_exception_info(thread, EXCEPT_TYPE_PRECISE,
								PRECISE_CAUSE_DOUBLE_EXCEPT,
								thread->Regs[REG_BADVA0],
								thread->Regs[REG_BADVA1],
								GET_SSR_FIELD(SSR_BVS),
								GET_SSR_FIELD(SSR_V0),
								GET_SSR_FIELD(SSR_V1),
								thread->Regs[REG_PC],
								((access_type ==
								  TYPE_LOAD) ? TLBMISSRW_CAUSE_READ :
								 TLBMISSRW_CAUSE_WRITE), (1 << slot));
		return;
	}
	if (slot == 0) {
		register_exception_info(thread, EXCEPT_TYPE_TLB_MISS_RW, ((access_type == TYPE_LOAD) ? TLBMISSRW_CAUSE_READ : TLBMISSRW_CAUSE_WRITE), badva, thread->Regs[REG_BADVA1], 0,	/* select 0 */
								1 /*set bv0 */ , 0 /*clear v1 */ ,
								thread->Regs[REG_PC],
								thread->processor_ptr->
								global_regs[REG_DIAG], 0);
	} else {
		register_exception_info(thread, EXCEPT_TYPE_TLB_MISS_RW, ((access_type == TYPE_LOAD) ? TLBMISSRW_CAUSE_READ : TLBMISSRW_CAUSE_WRITE), thread->Regs[REG_BADVA0], badva, 1,	/* select 0 */
								0 /*clear v0 */ , 1 /*set bv1 */ ,
								thread->Regs[REG_PC],
								thread->processor_ptr->
								global_regs[REG_DIAG], 0);
	}
	thread->processor_ptr->tlbmiss++;
    if (!thread->bq_on) {
        INC_TSTAT(ttlbmissrw);
    }
}

void register_reset_interrupt(thread_t * source_thread, int tnum)
{
	thread_t *thread = source_thread->processor_ptr->thread[tnum];

	/* FIXME NOW VERIFICATION: Check if this is a new feature to support for V5 */
	/* FOR VERIFICATION --
	 * Set ELR to PC.  Hardware doesn't require this.
	 */
	//thread->Regs[REG_ELR] = thread->Regs[REG_PC];

	/* Set PC to EVB */

	/* If self is reset, then treat it like an exception */
	if (thread == source_thread) {
		SET_EXCEPTION;
		thread->exception_PC = fREAD_CURRENT_EVB();
	}
	/* PC always goes to EVB */
	thread->Regs[REG_PC] = fREAD_CURRENT_EVB();

	/* Clear all SSR and set SSR.TNUM and SSR.EX */
	thread->Regs[REG_SSR] = 0;
	SET_SSR_FIELD(SSR_EX, 1);
	SET_SSR_FIELD(SSR_XA, thread->processor_ptr->arch_proc_options->default_ext);
	thread->t_mmvecx = set_thread_mmvecx(thread);
	thread->t_veclogsize = set_thread_veclogsize(thread);
	/* SET_SSR_FIELD(SSR_TNUM,tnum); */

	/* Clear LL reservations on interrupt */
	thread->lockvalid = 0;
	thread->fetch_samepage = 0;

	/* force transition to run mode */
	fSET_RUN_MODE_NOW(tnum);
}

void register_nmi_interrupt(thread_t * thread)
{
	/* Wake thread if waiting... */
	fCLEAR_WAIT_MODE(thread->threadId);
	/* Set PC to EVB */



	register_exception_info(thread, EXCEPT_TYPE_IMPRECISE,
							IMPRECISE_CAUSE_NMI, thread->Regs[REG_BADVA0],
							thread->Regs[REG_BADVA1],
							GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0),
							GET_SSR_FIELD(SSR_V1), thread->Regs[REG_PC],
							thread->processor_ptr->global_regs[REG_DIAG],
							0);


}

#ifndef NEW_INTERRUPTS
void recalculate_level_ipend(processor_t *proc)
{
        proc->global_regs[REG_IPEND] &= proc->global_regs[REG_IEL];
        proc->global_regs[REG_IPEND] |= (~proc->global_regs[REG_IEL] &
                 ((proc->global_regs[REG_IAHL] &
                   proc->interrupt_pins) |
                        (~(proc-> global_regs[REG_IAHL] | proc->interrupt_pins))));
}
#endif

static inline void register_interrupt_common(processor_t *proc, thread_t *thread, int tnum, int intnum)
{
	/* Wake up, time to get to work */
	/* Clear WAIT mode */
	fCLEAR_WAIT_MODE(tnum);
	/* Clear Pause state also */
	thread->pause_count = 0;
	/* Clear LL reservations on interrupt */
	thread->lockvalid = 0;
	thread->fetch_samepage = 0;
	CALLBACK(thread->processor_ptr->options->interrupt_serviced_callback,
		thread->system_ptr,proc,tnum,intnum);
}

void
register_general_interrupt(processor_t * proc, thread_t * thread,
						   int tnum, int intnum)
{
	warn("Interrupt taken: tnum=%d int=%d PC=%x Pcycle=%lld", tnum,
		 intnum, thread->Regs[REG_PC],
		 thread->processor_ptr->monotonic_pcycles);
	register_interrupt_common(proc,thread,tnum,intnum);
	/* Clear the interrupt from pending */
	fTAKEN_INTERRUPT_EDGECLEAR(proc, intnum);
	fSET_IAD(thread, intnum);
	/* set cause and EX in SSR */
	SET_SSR_FIELD(SSR_EX, 1);
	SET_SSR_FIELD(SSR_CAUSE, (INTERRUPT_CAUSE_INTERRUPT0 + intnum));
	thread->t_mmvecx = set_thread_mmvecx(thread);
	thread->t_veclogsize = set_thread_veclogsize(thread);
	/* Set ELR to PC */
	thread->Regs[REG_ELR] = thread->Regs[REG_PC];
	/* Set PC to event vector */
	thread->Regs[REG_PC] = fREAD_CURRENT_EVB() | ((intnum + 16) << 2);
	if(thread->timing_on) {
		uarch_update_iss_interrupt(thread->processor_ptr, thread->threadId, thread->Regs[REG_PC]);
	}
}
void
register_virtvic_interrupt(processor_t *proc, thread_t *thread, int tnum, int intnum)
{
	warn("VIRTVIC Interrupt taken: tnum=%d int=%d PC=%x Pcycle=%lld", tnum,
		 intnum, thread->Regs[REG_PC],
		 thread->processor_ptr->monotonic_pcycles);
	register_interrupt_common(proc,thread,tnum,intnum);
	fTAKEN_INTERRUPT_EDGECLEAR(proc, intnum);
	int whichvic;
	unsigned int vidval = 0;
	switch (intnum) {
		case 3:
			whichvic = 1;
			vidval = fREAD_GLOBAL_REG_FIELD(VID,VID_VID1);
			break;
		case 4:
			whichvic = 2;
			vidval = fREAD_GLOBAL_REG_FIELD(VID1,VID1_VID2);
			break;
		case 5:
			whichvic = 3;
			vidval = fREAD_GLOBAL_REG_FIELD(VID1,VID1_VID3);
			break;
		default:
			fatal("virtvic interrupt with wrong intnum: %d\n",intnum);
			return;
	}
	sys_coreready(thread,whichvic,1);
	/* Set GELR */
	thread->Regs[REG_GELR] = thread->Regs[REG_PC];
	/* SET GSR */
	fSET_FIELD(thread->Regs[REG_GSR],GSR_CAUSE,vidval);
	fSET_FIELD(thread->Regs[REG_GSR],GSR_UM,!GET_SSR_FIELD(SSR_GM));
	fSET_FIELD(thread->Regs[REG_GSR],GSR_IE,fGET_FIELD(thread->Regs[REG_CCR],CCR_GIE)); // == 1
	fSET_FIELD(thread->Regs[REG_GSR],GSR_SS,GET_SSR_FIELD(SSR_SS));
	/* CLEAR GIE */
	fSET_FIELD(thread->Regs[REG_CCR],CCR_GIE,0);
	/* Set SSR.G, clear SSR.SS */
	SET_SSR_FIELD(SSR_GM,1);
	SET_SSR_FIELD(SSR_SS,0);
	SET_SSR_FIELD(SSR_CAUSE, (INTERRUPT_CAUSE_INTERRUPT0 + intnum));

#ifdef VERIFICATION
    warn("register_virtvic_interrupt interupt=%d whichvic=%d vidval=%x REG_GSR=%x",  intnum, whichvic, vidval, thread->Regs[REG_GSR]);
#endif

	/* SET PC to GEVB + whatever */
	thread->Regs[REG_PC] = thread->Regs[REG_GEVB] + (0x10 << 2);
	if(thread->timing_on) {
		uarch_update_iss_interrupt(thread->processor_ptr, thread->threadId, thread->Regs[REG_PC]);
	}
}

static inline int sys_is_virtvic_interrupt(processor_t *proc, int tnum, int intno)
{
	unsigned int ccr_or = 0;
#ifdef VERIFICATION
	thread_t * thread =  proc->thread[tnum];
	warn("TB told me to take interrupt: %d on tnum: %d, so only looking at that threads CCR=%x",  intno, tnum, thread->Regs[REG_CCR]);
	ccr_or = thread->Regs[REG_CCR];
#else
	for (int i = 0; i < proc->runnable_threads_max; i++) {
		if (THREAD_IS_ON(proc, i)) {
			ccr_or |= proc->thread[i]->Regs[REG_CCR];
		}
	}
#endif

	if ((intno == 3) && fGET_FIELD(ccr_or,CCR_VV1)) return 1;
	if ((intno == 4) && fGET_FIELD(ccr_or,CCR_VV2)) return 1;
	if ((intno == 5) && fGET_FIELD(ccr_or,CCR_VV3)) return 1;
	return 0;
}

static inline int sys_avoid_virtvic_interrupt(processor_t *proc, int tnum, int intno)
{
	unsigned int my_ccr = proc->thread[tnum]->Regs[REG_CCR];
	int virtarch_ie = fGET_FIELD(my_ccr,CCR_GIE);
	/* If I can take this interrupt virtvic, don't avoid it */
	if ((intno == 3) && (fGET_FIELD(my_ccr,CCR_VV1))) return 1-virtarch_ie;
	if ((intno == 4) && (fGET_FIELD(my_ccr,CCR_VV2))) return 1-virtarch_ie;
	if ((intno == 5) && (fGET_FIELD(my_ccr,CCR_VV3))) return 1-virtarch_ie;
	/* Otherwise avoid it if it's a virtvic interrupt */
	return (sys_is_virtvic_interrupt(proc,tnum,intno));
}

static inline int sys_avoid_steered_interrupt(processor_t *proc, int tnum, int intno)
{
	unsigned int imask_or = 0;
	unsigned int lowest_prio = 1;
	unsigned int i;

	for (i = 0; i < proc->runnable_threads_max; i++) {
		if (THREAD_IS_ON(proc, i)) {
			/* Or together imask for all threads */
			imask_or |= proc->thread[i]->Regs[REG_IMASK];
			/* If any other thread has a worse priority (> #) then we're not the lowest */
			if ((fGET_FIELD(proc->thread[i]->Regs[REG_SSR],SSR_EX) != 0) ||
				(fGET_FIELD(proc->thread[i]->Regs[REG_SSR],SSR_IE) != 1)) continue;
			if (fGET_FIELD(proc->thread[tnum]->Regs[REG_TID],STID_PRIO) <
				fGET_FIELD(proc->thread[i]->Regs[REG_TID],STID_PRIO)) lowest_prio = 0;


		}
	}
	/* Virtvic interrupts are not HW steered */
	if (sys_is_virtvic_interrupt(proc,tnum,intno)) return 0;
	/* We can take the interrupt if any thread has IMASK bit set or we are lowest priority */
	return (!(((imask_or >> intno) & 1) || (lowest_prio)));
}

void sys_take_interrupt(processor_t *proc, int tnum, int intnum)
{
	thread_t *thread = proc->thread[0]; // for warn
	if (sys_is_virtvic_interrupt(proc,tnum,intnum)) {
		if (sys_avoid_virtvic_interrupt(proc,tnum,intnum)) {
			warn("OOPS: asked me to take a virtvic interrupt that I can't take!");
		}
		register_virtvic_interrupt(proc,proc->thread[tnum],tnum,intnum);
	} else {
		register_general_interrupt(proc,proc->thread[tnum],tnum,intnum);
	}
}

void
sys_reschedule_interrupt(processor_t * proc)
{
    thread_t *thread = proc->thread[0]; // for warn
    if (fGET_FIELD(proc->global_regs[REG_SCHEDCFG], SCHEDCFG_ENABLED)) {
        int intnum = fGET_FIELD(proc->global_regs[REG_SCHEDCFG], SCHEDCFG_INTNO);
        fSET_FIELD(proc->global_regs[REG_BESTWAIT], BESTWAIT_PRIO, 0x1FF); // Reset best priority
        // OOPS: clobbers other pending interrupts fSET_FIELD(proc->global_regs[REG_IPENDAD], IPENDAD_IPEND, 1<<intnum); // Set interrupt on ipend
        arch_trigger_interrupt(proc,intnum);
#ifdef VERIFICATION
        warn("Setting Reschedule Interrupt to pending and resetting BESTWAIT - set IPENDAD=%x for interrupt=%d", proc->global_regs[REG_IPENDAD], intnum);
#endif
    } else {
        warn("Tryinging reschedule interrupt but no enabled");
    }
}


void
arch_iss_external_event(processor_t * proc, int tnum, int eventno,
						int cause, size4u_t badva0, size4u_t badva1,
						size4u_t bvs, size4u_t bv0, size4u_t bv1)
{
	thread_t *thread = proc->thread[tnum];
	/* Wake up, time to get to work */
	/* Clear WAIT mode */
	fCLEAR_WAIT_MODE(tnum);
	warn("External Event (like NMI)");
	if (GET_SSR_FIELD(SSR_EX) &&
		((eventno > EXCEPT_TYPE_PRECISE) ||
		 ((eventno == EXCEPT_TYPE_PRECISE) &&
		  (cause > PRECISE_CAUSE_DOUBLE_EXCEPT)))) {
		warn("Double Exception...");
		register_exception_info(thread, EXCEPT_TYPE_PRECISE,
								PRECISE_CAUSE_DOUBLE_EXCEPT,
								thread->Regs[REG_BADVA0],
								thread->Regs[REG_BADVA1],
								GET_SSR_FIELD(SSR_BVS),
								GET_SSR_FIELD(SSR_V0),
								GET_SSR_FIELD(SSR_V1),
								thread->Regs[REG_PC], cause, 0);

		commit_exception_info(thread);
		return;
	}
	register_exception_info(thread, eventno, cause, badva0, badva1, bvs,
							bv0, bv1, thread->Regs[REG_PC],
							thread->processor_ptr->global_regs[REG_DIAG],
							0);
	commit_exception_info(thread);
}

static inline int
sys_tlb_is_match(thread_t * thread, size8u_t tlb1, size8u_t tlb2, int consider_gbit)
{
	size8u_t baddr1, baddr2;
	size8u_t size1, size2;
	size4u_t asid1, asid2;
	size4u_t gbit1, gbit2;
	size4u_t valid1, valid2;
	size1 = 1 << (12 + 2 * get_pgsize(GET_PPD(tlb1)));
	size2 = 1 << (12 + 2 * get_pgsize(GET_PPD(tlb2)));
	baddr1 = GET_FIELD(PTE_VPN, tlb1) << 12;
	baddr2 = GET_FIELD(PTE_VPN, tlb2) << 12;
	baddr1 &= ~((size1) - 1);
	baddr2 &= ~((size2) - 1);
	asid1 = GET_FIELD(PTE_ASID, tlb1);
	asid2 = GET_FIELD(PTE_ASID, tlb2);
	gbit1 = GET_FIELD(PTE_G, tlb1);
	gbit2 = GET_FIELD(PTE_G, tlb2);
	valid1 = GET_FIELD(PTE_V, tlb1);
	valid2 = GET_FIELD(PTE_V, tlb2);




	if ((!valid1) || (!valid2))
		return 0;
	if (((baddr1 <= baddr2) && (baddr2 < (baddr1 + size1))) ||
		((baddr2 <= baddr1) && (baddr1 < (baddr2 + size2)))) {
		if (asid1 == asid2) {
			warn("Compare two TLB entries tlb1 (0x%llx) size1=%x baddr1=%x  gbit1=%x valid1=%x tlb2 (0x%llx) size2=%x baddr2=%x  gbit2=%x valid2=%x", tlb1, size1, baddr1, gbit1, valid1, tlb2, size2, baddr2, gbit2, valid2);
			return 1;
		}
		/* For CTLBW instruction, tlb1 is the data from Rss,
		   tlb2 is the JU entry */
		if ((consider_gbit && gbit1) || gbit2) {
			warn("Compare (CTLBW) two TLB entries tlb1 (0x%llx) size1=%x baddr1=%x  gbit1=%x valid1=%x tlb2 (0x%llx) size2=%x baddr2=%x  gbit2=%x valid2=%x", tlb1, size1, baddr1, gbit1, valid1, tlb2, size2, baddr2, gbit2, valid2);
			return 1;
		}

	}
	return 0;
}

static inline int sys_tlb_aliases(size8u_t tlb1, size8u_t tlb2)
{
	size8u_t baddr1, baddr2;
	size8u_t size1, size2, maxsize;
	size4u_t valid1, valid2;
	size1 = 1 << (12 + 2 * get_pgsize(GET_PPD(tlb1)));
	size2 = 1 << (12 + 2 * get_pgsize(GET_PPD(tlb2)));
	maxsize = fMAX(size1, size2);
	baddr1 = get_ppn(GET_PPD(tlb1)) << 12;
	baddr2 = get_ppn(GET_PPD(tlb2)) << 12;
	baddr1 &= ~((maxsize) - 1);
	baddr2 &= ~((maxsize) - 1);
	valid1 = GET_FIELD(PTE_V, tlb1);
	valid2 = GET_FIELD(PTE_V, tlb2);
	if ((!valid1) || (!valid2))
		return 0;
	if (baddr1 == baddr2)
		return 1;
	return 0;
}

void sys_tlb_check(thread_t * thread)
{
	int i, j;
	for (i = 0; i < NUM_TLB_REGS(thread->processor_ptr); i++) {
		size8u_t tlbi = thread->processor_ptr->TLB_regs[i];
		if (GET_FIELD(PTE_V, tlbi)
			&& (get_pgsize(GET_PPD(tlbi)) == 0x7)) {
			warn("[UNDEFINED] PGSIZE field 0x7 on entry %d", i);
		}
		for (j = 0; j < NUM_TLB_REGS(thread->processor_ptr); j++) {
			if (i == j)
				continue;
			size8u_t tlbj = thread->processor_ptr->TLB_regs[j];
			if (sys_tlb_is_match(thread, tlbi, tlbj, 1)) {
				/* Change string to UNDEF so Raven can handle this one specially */
				warn("[UNDEF] TLB: entry %d (0x%llx) matches entry %d (0x%llx)!!", i, tlbi, j, tlbj);
			}
			if (sys_tlb_aliases(tlbi, tlbj) &&
				(GET_FIELD(PTE_C, tlbi) != GET_FIELD(PTE_C, tlbj))) {
				warn("[UNDEFINED] aliasing pages %d (0x%llx) and %d (0x%llx) have differing Cacheability", i, tlbi, j, tlbj);
			}
		}
	}
}

/* return codes:
   0 or positive number = index of match
   -1 = multiple matches
   -2 = no matches
*/
int sys_check_overlap(thread_t * thread, size8u_t data)
{
	int j;
	int matches = 0;
	int last_match = 0;
	for (j = 0; j < NUM_TLB_REGS(thread->processor_ptr); j++) {
		size8u_t tlbj = thread->processor_ptr->TLB_regs[j];
		if (sys_tlb_is_match(thread, data, tlbj, 0)) {
			warn("ctlbw overlap (0x%llx) and %d (0x%llx)", data, j, tlbj);
			matches++;
			last_match = j;
		}
	}
	if (matches == 1) {
		return last_match;
	}
	if (matches == 0) {
		return -2;
	}
	return -1;
}


void sys_tlb_write(thread_t * thread, int index, size8u_t data)
{
	processor_t *proc = thread->processor_ptr;

	thread->next_pkt_guess = 0;
	thread->processor_ptr->flush_detected = 1;
	if (thread->last_pkt) {
		thread->last_pkt->taken_ptr = 0;
	}
	VERIFY_EXTENSIVELY(proc, sys_tlb_check(thread));
	/* Invalidate u-tlb */
	sys_utlb_invalidate(thread->processor_ptr, NULL);

	/* /\* When updating a TLB entry, also update the decode info *\/ */
	/* thread->processor_ptr->TLB_decoded_attributes[index] = */
	/* 	sys_decode_cccc(thread->processor_ptr, GET_FIELD(PTE_C, data), */
	/* 					GLOBAL_REG_READ(REG_SYSCONF)); */

	/* Invalidate guess for this virtual page in case we've created a multiple TLB match */
	proc->VA_to_idx_guess[TLBGUESSIDX(GET_FIELD(PTE_VPN, data) << 12)] = -1;

	if(thread->timing_on) {
		cache_invalidate_jtlbentry_in_utlbs(thread->processor_ptr, thread->threadId, index);
		fe_reset_on_iic_kill(proc);
	}
}

void
sys_update_pmu_counter_update_allowed_check(processor_t *proc)
{
    int i, j;
    size4u_t mystidval;
    thread_t *thread;

    for(i=0;i<NUM_PMU_REGS;i++) {
        if(i < 4) {
            mystidval = proc->global_regs[REG_PMUSTID0] >> (8 * i) & 0xff;
        } else {
            mystidval = proc->global_regs[REG_PMUSTID1] >> (8 * i) & 0xff;
        }

        for(j=0;j<proc->runnable_threads_max;j++) {
			if (THREAD_IS_ON(proc, j))
			{
				thread = proc->thread[j];
				if(mystidval == 0x0) {
					thread->pmu_update_allowed[i] = 1;
				} else {
					if(mystidval == GET_FIELD(STID_STID, thread->Regs[REG_TID])) {
						thread->pmu_update_allowed[i] = 1;
					} else {
						thread->pmu_update_allowed[i] = 0;
					}
				}
			}
        }
    }
}

void
sys_check_ssr_xe(thread_t *thread)
{
	processor_t *proc = thread->processor_ptr;
	int nclusters = proc->runnable_threads_max / proc->threads_per_cluster;
	int xe_thread_en;
	int i, j, tnum;
	if(!proc->options->check_ssr_xe) return;

	for(i=0;i<nclusters;i++) {
		xe_thread_en = 0;
		for(j=0;j<proc->threads_per_cluster;j++) {
			tnum = (i * proc->threads_per_cluster) + (j % proc->threads_per_cluster);
			thread = proc->thread[tnum];
			if(GET_SSR_FIELD(SSR_XE) == 1) {
				xe_thread_en++;
			}
			/*If XE count already is above 2, no need to keep counting */
			if(xe_thread_en > 2) {
				fprintf(stderr,"More than 2 threads in cluster <%d> have xe set in SSR\n", i);

				for(i=0;i<nclusters;i++) {
					for(j=0;j<proc->threads_per_cluster;j++) {
						tnum = (i * proc->threads_per_cluster) + (j % proc->threads_per_cluster);
						thread = proc->thread[tnum];
						fprintf(stderr,"Cluster: %d Thread ID: %d SSR_XE = %d\n", i, tnum, (int)GET_SSR_FIELD(SSR_XE));
					}
				}
				return;
			}
		}
	}
}

/*
 * EJP: This must do nothing if a register is passed in that we don't care about.
 */
void sys_write_local_creg(thread_t * thread, int rnum, size4u_t data)
{
	processor_t *proc = thread->processor_ptr;
	int tnum = thread->threadId;

	if (thread->next_pkt_guess) {
		thread->next_pkt_guess = 0;
	}

	if (thread->last_pkt) {
		thread->last_pkt->taken_ptr = 0;
	}

	if (rnum == REG_SSR) {
		/* writing SSR and ASID changed */
		if (GET_FIELD(SSR_ASID, data) !=
			GET_FIELD(SSR_ASID, READ_RREG(REG_SSR))) {
			proc->flush_detected = 1;
		}
		/* Any SSR update clears u-tlbs, invalidates FMC for this thread only */
		sys_utlb_invalidate(proc, thread);

		/* Shadow the single-step bit so we can check this quickly */
		thread->single_step_set = GET_FIELD(SSR_SS, data);

#ifdef VERIFICATION
    warn("sys_write_local_creg updating SSR=%x thread->single_step_set=%d\n",data, thread->single_step_set);
#endif

		thread->t_mmvecx = set_thread_mmvecx_val(thread, data);
		thread->t_veclogsize = set_thread_veclogsize(thread);

		sys_check_ssr_xe(thread);
	}

    if(rnum == REG_TID) {
        sys_update_pmu_counter_update_allowed_check(proc);
    }

	if (thread->timing_on) {
		proc->uarch.uarch_update_local_reg_change_ptr(proc, tnum, rnum,
													  data);
	}
}

static inline void sys_check_syscfg(thread_t * thread, size4u_t data)
{
#if 0							/* "FIX" bug 2476 */
	if (GET_FIELD(SYSCFG_MMUEN, data) &&
		(GET_FIELD(SYSCFG_DCEN, data) == 0)) {
		warn("[UNDEFINED] SYSCFG: MMUEN on, DCEN off");
	}
	if (GET_FIELD(SYSCFG_MMUEN, data) &&
		(GET_FIELD(SYSCFG_ICEN, data) == 0)) {
		warn("[UNDEFINED] SYSCFG: MMUEN on, ICEN off");
	}
#endif
	if (GET_FIELD(SYSCFG_MMUEN, data))
		sys_tlb_check(thread);
}

#define C_RESV1(C)      (C<<CCC_RESV1)
#define C_RESV2(C)      (C<<CCC_RESV2)
#define C_DEVICE(C)     (C<<CCC_DEVICE)
#define C_L1IC(C)       (C<<CCC_L1IC)
#define C_L1DC(C)       (C<<CCC_L1DC)
#define C_L2C(C)        (C<<CCC_L2C)
#define C_L1WT(C)       (C<<CCC_L1WT)
#define C_L1WB(C)       (C<<CCC_L1WB)
#define C_L2WT(C)       (C<<CCC_L2WT)
#define C_L2WB(C)       (C<<CCC_L2WB)
#define C_L2AUX(C)      (C<<CCC_L2AUX)


static const size4u_t cccc_decode_table[] = {
	/* 0x0 D=WB, I=C L2=UC */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(1) | C_L1WT(0) |
	 C_L1IC(1) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0x1 D=WT, I=C L2=UC */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(0) | C_L1WT(1) |
	 C_L1IC(1) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0x2 D=device,SFC (Source Flow Controlled) , I=UC L2=UC */
	(C_DEVICE(1) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0x3 D=device,SFC , I=UC L2=UC */
	(C_DEVICE(1) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0x4 D=device, I=UC L2=UC */
	(C_DEVICE(1) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0x5 D=WT, I=UC L2=WT */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(0) | C_L1WT(1) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(1) | C_L2WB(0) | C_L2WT(1)),

	/* 0x6 D=UC, I=UC L2=UC */
	(C_DEVICE(0) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0x7 D=WB, I=C  L2=WB */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(1) | C_L1WT(0) |
	 C_L1IC(1) | C_L2AUX(0) | C_L2C(1) | C_L2WB(1) | C_L2WT(0)),

	/* 0x8 D=WB, I=C  L2=WT */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(1) | C_L1WT(0) |
	 C_L1IC(1) | C_L2AUX(0) | C_L2C(1) | C_L2WB(0) | C_L2WT(1)),

	/* 0x9 D=WT, I=C  L2=WB */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(0) | C_L1WT(1) |
	 C_L1IC(1) | C_L2AUX(0) | C_L2C(1) | C_L2WB(1) | C_L2WT(0)),

	/* 0xA D=WB, I=UC  L2=WBAUX */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(1) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(1) | C_L2C(1) | C_L2WB(1) | C_L2WT(0)),

	/* 0xB D=WT, I=UC  L2=WTAUX */
	(C_DEVICE(0) |
	 C_L1DC(1) | C_L1WB(0) | C_L1WT(1) |
	 C_L1IC(0) | C_L2AUX(1) | C_L2C(1) | C_L2WB(0) | C_L2WT(1)),

	/* 0xC Reserved, D=UC, I=UC L2=UC */
	(C_DEVICE(0) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0xD D=UC, I=UC L2=WT */
	(C_DEVICE(0) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(1) | C_L2WB(0) | C_L2WT(1)),

	/* 0xE Reserved, D=UC, I=UC L2=UC */
	(C_DEVICE(0) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0)),

	/* 0xF D=UC, I=UC L2=WB */
	(C_DEVICE(0) |
	 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
	 C_L1IC(0) | C_L2AUX(0) | C_L2C(1) | C_L2WB(1) | C_L2WT(0))
};




size4u_t sys_decode_cccc(processor_t * proc, int cbits, size4u_t syscfg)
{
	size4u_t memaccess;
	int mmu_on = GET_FIELD(SYSCFG_MMUEN, syscfg);
	int l1d_on = GET_FIELD(SYSCFG_DCEN, syscfg);
	int l1i_on = GET_FIELD(SYSCFG_ICEN, syscfg);
	int l2_on = (GET_FIELD(SYSCFG_L2CFG, syscfg) != 0);
	int l2_wb = GET_FIELD(SYSCFG_L2WB, syscfg);
	int l2_wa = (GET_FIELD(SYSCFG_L2NWA, syscfg) == 0);

	/* Setup cbits */
	if (!mmu_on) {
		/* Select the defaults for MMU OFF: D$=WT, I$=C, L2$=UC */
		memaccess =
			(C_DEVICE(0) |
			 C_L1DC(1) | C_L1WB(0) | C_L1WT(1) |
			 C_L1IC(1) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0));
		CCC_SET_CBITS(memaccess, 1);
	} else {
		/* Get them from TLB */
		memaccess = cccc_decode_table[cbits & 0xf];
		CCC_SET_CBITS(memaccess, cbits);
	}

	/* If archstring forcing L2WB, do that */
	if (proc->arch_proc_options->l2cache_write_policy & 0x1) {
		l2_wb = 1;
	}

	/* Override L2 settings based on L2WB=0 which forces L2WT mode */
	if (!l2_wb) {
		/* CCC wanted L2WB, but change it to WT */
		if (CCC_GET_L2WB(memaccess)) {
			CCC_SET_L2WB(memaccess, 0);
			CCC_SET_L2WT(memaccess, 1);
		}
	}

	/* Overrides based on cache enables */
	if (!l1d_on) {				/* D$ is OFF */
		CCC_SET_L1DC(memaccess, 0);
	}
	if (!l1i_on) {				/* I$ is OFF */
		CCC_SET_L1IC(memaccess, 0);
	}
	if (!l2_on) {				/* L2$ is OFF */
		CCC_SET_L2C(memaccess, 0);
	}

	if (proc->arch_proc_options->l2cache_write_allocate == 1) {
		l2_wa = 1;
	} else if (proc->arch_proc_options->l2cache_write_allocate == 2) {
		l2_wa = 0;
	}
	CCC_SET_L2WA(memaccess, l2_wa);

	return (memaccess);
}

/* void sys_tlb_refresh_attributes(processor_t * proc, size4u_t syscfg) */
/* { */
/* 	int i; */
/* 	thread_t *thread = proc->thread[0]; */
/* 	size8u_t entry; */

/* 	for (i = 0; i < NUM_TLB_REGS(proc); i++) { */
/* 		entry = thread->processor_ptr->TLB_regs[i]; */
/* 		proc->TLB_decoded_attributes[i] = */
/* 			sys_decode_cccc(proc, GET_FIELD(PTE_C, entry), syscfg); */
/* 	} */
/* } */

void
sys_stats_reset(processor_t * proc)
{
    CALLBACK(proc->options->arch_stats_reset_callback, proc->system_ptr, proc);
    return;
}

void sys_write_syscfg(thread_t * thread, int rnum, size4u_t data)
{
	processor_t *proc = thread->processor_ptr;
	int tnum = thread->threadId;
	int i;

	//	sys_tlb_refresh_attributes(proc, data);
	sys_utlb_invalidate(proc, NULL);
	if (thread->pmu_on) {
		iic_flush_cache(proc);
	}

	/* PCYCLEEN 0->1 transition  clears the counter */
	if (((GET_FIELD(SYSCFG_PCYCLEEN, proc->global_regs[REG_SYSCONF]) ==
		  0) && (GET_FIELD(SYSCFG_PCYCLEEN, data) == 1))) {
		proc->pcycle_counter = 0;
		CLEAR_PCYCLE_REGS(proc);
		if (proc->arch_proc_options->stats_clear_on_pcycle_en) {
			sys_stats_reset(proc);
		}
	}

	/* Flush the internal tlbs and cache when MMU is enabled */
	if ((GET_FIELD(SYSCFG_MMUEN, proc->global_regs[REG_SYSCONF]) == 0) &&
		(GET_FIELD(SYSCFG_MMUEN, data) == 1)) {
		proc->flush_detected = 1;
	}

	/* PMU enable */
	if (proc->arch_proc_options->pmu_enable == 2 ) {
		proc->pmu_on = (GET_FIELD(SYSCFG_PM, data)) & thread->timing_on;
		for (i = 0; i < proc->runnable_threads_max; i++) {
			if (THREAD_IS_ON(proc, i)) {
                proc->thread[i]->pmu_on = proc->pmu_on;
			}
		}
	}

	if (thread->timing_on || (proc->uarch.cache_lean_ready)) {
		proc->uarch.uarch_update_global_reg_change_ptr(proc, tnum, REG_SYSCONF, data); //do I need to worry about other instnces of this?
	}

	if (thread->pmu_on) {
		iic_flush_cache(proc);
	}

	/* CLADE enable */
#ifdef CLADE
	if ((GET_FIELD(SYSCFG_CLADEN, proc->global_regs[REG_SYSCONF]) == 0) &&
			(GET_FIELD(SYSCFG_CLADEN, data) == 1)) {  // disabled->enabled
		clade_enable(proc);
	} else if ((GET_FIELD(SYSCFG_CLADEN, proc->global_regs[REG_SYSCONF]) == 1) &&
						 (GET_FIELD(SYSCFG_CLADEN, data) == 0)) {  // enabled->disabled
		clade_disable(proc);
	}
#endif

	VERIFY_EXTENSIVELY(proc, sys_check_syscfg(thread, data));
}

static inline void sys_global_creg_change_onoff(thread_t *thread, size4u_t old, size4u_t new)
{
	size4u_t idx;
	while (new > old) {
		/* Threads Turned On */
		idx = __builtin_ctz(old ^ new);
		fRESET_THREAD(thread,idx);
		old |= (1<<idx);
	}
}

static inline void sys_global_creg_change_waiting(thread_t *thread, size4u_t old, size4u_t new)
{
}

/* EJP: note: this doesn't actually write the creg, just notifies of it */
/* EJP: actual write in exec_commit_globals */
/* Must ignore non-cregs */
void sys_write_global_creg(thread_t * thread, int rnum, size4u_t data)
{
	processor_t *proc = thread->processor_ptr;

	thread->next_pkt_guess = 0;
	if (thread->last_pkt) thread->last_pkt->taken_ptr = 0;
	if (rnum >= NUM_GLOBAL_REGS) fatal("write_global_creg with too big of a register: %d",rnum);


	if (rnum == REG_SYSCONF) {
		sys_write_syscfg(thread, rnum, data);
	}

#ifndef NEW_INTERRUPTS
        if ((rnum == REG_IAHL) || (rnum == REG_IEL)) {
                /* Write a bit early */
                thread->processor_ptr->global_regs[rnum] = data;
                recalculate_level_ipend(thread->processor_ptr);
        }
#endif

	if (rnum == REG_ISDBMBXOUT) {
		fWRITE_GLOBAL_REG_FIELD(ISDBST, ISDBST_MBXOUTSTATUS, 1);
	}

	if (rnum == REG_PMUCFG) {
		/* Recalc PMU stuff only if it's on and the cfg is actually changing */
		if (thread->pmu_on && (proc->global_regs[rnum] != data)) {
			iic_flush_cache(thread->processor_ptr);
		}
		if (thread->timing_on || proc->uarch.cache_lean_ready) {
			proc->uarch.uarch_update_global_reg_change_ptr(proc, thread->threadId,
																										 REG_PMUCFG, data);
		}
	}

	if (rnum == REG_PMUEVTCFG || rnum == REG_PMUEVTCFG1 || rnum == REG_PMUCNT0 || rnum == REG_PMUCNT1
			|| rnum == REG_PMUCNT2 || rnum == REG_PMUCNT3) {

		if (thread->pmu_on && (proc->global_regs[rnum] != data)) {
			iic_flush_cache(thread->processor_ptr);
		}
	}

	if ((rnum == REG_PMUSTID0) || (rnum == REG_PMUSTID1)) {
		sys_update_pmu_counter_update_allowed_check(proc);
	}
	if (rnum == REG_MODECTL) {
		size4u_t old_enabled = fREAD_GLOBAL_REG_FIELD(MODECTL,MODECTL_E);
		size4u_t old_waiting = fREAD_GLOBAL_REG_FIELD(MODECTL,MODECTL_W);
		size4u_t new_enabled = fGET_FIELD(data,MODECTL_E);
		size4u_t new_waiting = fGET_FIELD(data,MODECTL_W);
		if (old_enabled != new_enabled) sys_global_creg_change_onoff(thread,old_enabled,new_enabled);
		if (old_waiting != new_waiting) sys_global_creg_change_waiting(thread,old_waiting,new_waiting);
		/* Write early for sys_recalc_num_running_threads */

#ifdef VERIFICATION
    warn("sys_write_global_creg updating MODECTL=%x (old=%x)\n",data, thread->processor_ptr->global_regs[rnum]);
#endif

		thread->processor_ptr->global_regs[rnum] = data;
		sys_recalc_num_running_threads(thread->processor_ptr);
	}

}

/* Read user control register */
size4u_t sys_read_ucreg(const thread_t * thread, int reg)
{
	int ssr_ce = GET_SSR_FIELD(SSR_CE);
	switch (REG_SA0+reg) {
	case REG_UPCYCLE_LO: return ssr_ce ? GLOBAL_REG_READ(REG_PCYCLELO) : 0;
	case REG_UPCYCLE_HI: return ssr_ce ? GLOBAL_REG_READ(REG_PCYCLEHI) : 0;
	case REG_UTIMERLO: return ssr_ce ? GLOBAL_REG_READ(REG_TIMERLO) : 0;
	case REG_UTIMERHI: return ssr_ce ? GLOBAL_REG_READ(REG_TIMERHI) : 0;
	}
	return thread->Regs[REG_SA0+reg];
}


#include "q6v_system.c"



size4u_t sys_read_creg(processor_t * proc, int rnum)
{
	thread_t *thread = proc->thread[0];	/* for macros */ /* EJP: not const b/c ISDBMBXIN */
	size4u_t retval = proc->global_regs[rnum];
/* FIXME NOW VERIFICATION: Remove if ifndef verification */
	switch (rnum) {
#ifndef VERIFICATION
	case REG_PCYCLELO:
	case REG_PCYCLEHI:
		retval = GET_FIELD(SYSCFG_PCYCLEEN,proc->global_regs[REG_SYSCONF]) ? retval : 0;
		break;
#endif
	case REG_ISDBMBXIN:
		fWRITE_GLOBAL_REG_FIELD(ISDBST, ISDBST_MBXINSTATUS, 0);
		break;
	case REG_ISDBMBXOUT: retval = 0; break;
	case REG_TIMERLO:
	case REG_TIMERHI:
		/* Notify other side that we're about to (re)read the timer register */
		/* This allows the timer to be lazy about updating time */
	    CALLBACK(proc->options->qtimer_read_callback, proc->system_ptr, proc);
		retval = proc->global_regs[rnum];
		break;
	}
	return (retval);
}

size4u_t sys_read_sreg(const thread_t * thread, int rnum)
{
	size4u_t retval;

	if (rnum >= 16) {
		retval = GLOBAL_REG_READ(rnum - 16);
	} else {
		retval = thread->Regs[rnum + REG_SGP];
	}

	if (rnum == (REG_BADVA - REG_SGP)) {
		if (GET_SSR_FIELD(SSR_BVS)) {
			retval = thread->Regs[REG_BADVA1];
		} else {
			retval = thread->Regs[REG_BADVA0];
		}
	}

	return (retval);
}

void sys_check_privs(thread_t * thread)
{
	if (!sys_in_monitor_mode(thread)) {
#ifdef VERIFICATION
	SYSVERWARN("privilege checked and failed");
#endif
		register_error_exception(thread, PRECISE_CAUSE_PRIV_USER_NO_SINSN,
								 thread->Regs[REG_BADVA0],
								 thread->Regs[REG_BADVA1],
								 GET_SSR_FIELD(SSR_BVS),
								 GET_SSR_FIELD(SSR_V0),
								 GET_SSR_FIELD(SSR_V1), 0);
	}
}

void sys_check_guest(thread_t * thread)
{
	if (sys_in_user_mode(thread)) {
		register_error_exception(thread, PRECISE_CAUSE_PRIV_USER_NO_GINSN,
			 thread->Regs[REG_BADVA0],
			 thread->Regs[REG_BADVA1],
			 GET_SSR_FIELD(SSR_BVS),
			 GET_SSR_FIELD(SSR_V0),
			 GET_SSR_FIELD(SSR_V1), 0);
	}
}

void sys_illegal(thread_t *thread)
{
	register_error_exception(thread,PRECISE_CAUSE_INVALID_OPCODE,
		 thread->Regs[REG_BADVA0],
		 thread->Regs[REG_BADVA1],
		 GET_SSR_FIELD(SSR_BVS),
		 GET_SSR_FIELD(SSR_V0),
		 GET_SSR_FIELD(SSR_V1), 0);
}

#ifdef NEW_INTERRUPTS

/*
 * EJP: maybe need to have can_accept_interrupt and should_accept_interrupt to distinguish
 * what's legal for TB to hand us vs. what we should do in simulation ...
 */
int can_accept_interrupt(processor_t * proc, int tnum, int *intnum)
{
	thread_t *thread = proc->thread[tnum];
	unsigned int ipend = (fREAD_GLOBAL_REG_FIELD(IPENDAD,IPENDAD_IPEND) &
		~fREAD_GLOBAL_REG_FIELD(IPENDAD,IPENDAD_IAD));
	unsigned int imask = thread->Regs[REG_IMASK];
	int i;
	ipend &= (~imask);
	if (ipend == 0) return 0;
	/* We've already accepted an interrupt this pcycle */
	if (proc->last_interrupt_accepted_pcycle == proc->monotonic_pcycles) return 0;
	if ((GET_FIELD(SYSCFG_GIE, proc->global_regs[REG_SYSCONF]) == 0)
		|| (GET_SSR_FIELD(SSR_EX) != 0) || (GET_SSR_FIELD(SSR_IE) == 0)) {
		return 0;
	}
	for (i = 0; i < INTERRUPT_MAX; i++) {
		if (sys_avoid_virtvic_interrupt(proc,tnum,i)) continue;
		if (sys_avoid_steered_interrupt(proc,tnum,i)) continue;
		if (ipend & INT_NUMTOMASK(i)) {
			*intnum = i;
			proc->last_interrupt_accepted_pcycle = proc->monotonic_pcycles;
			return 1;
		}
	}
	return 0;
}

#else
int can_accept_interrupt(processor_t * proc, int tnum, int *intnum)
{
        unsigned int ipend = proc->global_regs[REG_IPEND] & ~(proc->global_regs[REG_IAD]);
        unsigned int imask = proc->thread[tnum]->Regs[REG_IMASK];
        thread_t *thread = proc->thread[tnum];
        int i;
        ipend &= (~imask);
        if (ipend == 0) return 0;
        /* We've already accepted an interrupt this pcycle */
        if (proc->last_interrupt_accepted_pcycle == proc->monotonic_pcycles) return 0;
        if ((GET_FIELD(SYSCFG_GIE, proc->global_regs[REG_SYSCONF]) == 0)
                || (GET_SSR_FIELD(SSR_EX) != 0) || (GET_SSR_FIELD(SSR_IE) == 0)) {
                return 0;
        }
        for (i = 0; i < INTERRUPT_MAX; i++) {
		if (sys_avoid_virtvic_interrupt(proc,tnum,i)) continue;
		if (sys_avoid_steered_interrupt(proc,tnum,i)) continue;
                if (ipend & INT_NUMTOMASK(i)) {
                        *intnum = i;
                        proc->last_interrupt_accepted_pcycle = proc->monotonic_pcycles;
                        return 1;
                }
        }
        return 0;
}
#endif

#define INVALID_MASK 0xffffffffLL

/* TBD: define the invalid masks to HW behavior ? */
size8u_t encmask_2_mask[] = {
	/* 4k,   0000 */ 0x0fffLL,
	/* 16k,  0001 */ 0x3fffLL,
	/* 64k,  0010 */ 0xffffLL,
	/* 256k, 0011 */ 0x3ffffLL,
	/* 1m,   0100 */ 0xfffffLL,
	/* 4m,   0101 */ 0x3fffffLL,
	/* 16M,  0110 */ 0xffffffLL,
	/* RSVD, 0111 */ INVALID_MASK,
};

size8u_t apply_mask_to_page(size8u_t addr, size4u_t encmask)
{
	size8u_t mask;

	mask = encmask_2_mask[encmask & 0x7];
	if (mask == -1) {
		/* What is the behavior of unknown masks ? */
	}
	return ((addr & ~mask) >> 12);
}


/* Do a linear search of the TLB starting at index
   'start_search_idx'. For each entry, the match
   is (VA match && (Global || (ASID match))). Return
   the first entry to match or -1 if no match.

   Match if (VA match && (Global || (ASID match))).
   Return the first 1 or 0 for no match
*/

int tlb_match_idx(processor_t * P, int i, size4u_t asid, size4u_t VA)
{
	size4u_t entry_VA, entry_G, entry_PGSIZE;
	size8u_t entry_PPD, entry_PPN;
	size4u_t entry_ASID, entry_VALID;
	size8u_t entry;

#ifdef VERIFICATION
	if (P->tlbptr == NULL)
		entry = P->TLB_regs[i];
	else
		entry = P->tlbptr[i];
#else
	entry = P->TLB_regs[i];
#endif

	entry_VALID = GET_FIELD(PTE_V, entry);
	/* It's a valid entry */
	if (entry_VALID) {
		entry_VA = GET_FIELD(PTE_VPN, entry);
		entry_PPD = GET_PPD(entry);
		entry_PGSIZE = get_pgsize(entry_PPD);
		entry_PPN = get_ppn(entry_PPD);
		/* VA's match */
		if (apply_mask_to_page((entry_VA << 12), entry_PGSIZE) ==
			apply_mask_to_page(VA, entry_PGSIZE)) {
			entry_G = GET_FIELD(PTE_G, entry);
			entry_ASID = GET_FIELD(PTE_ASID, entry);
			/* And it's Global or ASIDs match */
			if (entry_G || (entry_ASID == asid)) {
				/* hit on this idx */
				return (1);
			}
		}
	}

	/* miss in TLB */
	return (0);
}


/* Return the index that hit, or -1 if miss */
/* FIXME: is_tlbp unused */
size4u_t tlb_lookup(thread_t * thread, size4u_t ssr, size4u_t VA, int is_tlbp) {
	return tlb_lookup_by_asid(thread, GET_FIELD(SSR_ASID, ssr), VA, is_tlbp);
}

size4u_t tlb_lookup_by_asid(thread_t * thread, size4u_t asid, size4u_t VA, int is_tlbp) {

	int i;
	int guessidx, guess;
	size4u_t idx;
	int max_tlb_reg;
	processor_t *proc = thread->processor_ptr;
#ifdef VERIFICATION
	int saw_once = 0;
#endif
	guessidx = TLBGUESSIDX(VA);
	guess = 0;
	max_tlb_reg = NUM_TLB_REGS(proc);

/* FIXME NOW VERIFICATION:  Remove this ifndef so we always do the guess thing. Put the warning in the
   inside guess */
#ifndef VERIFICATION
	if ((guess = proc->VA_to_idx_guess[guessidx]) >= 0) {  // is valid
		guess &= max_tlb_reg - 1;
		if (tlb_match_idx(thread->processor_ptr, guess, asid, VA)) {
			return (guess);
		}
	}
#endif

	/* Guess failed, do a search */
	idx = (int) (0x80000000);
	for (i = 0; i < max_tlb_reg; i++) {
		if (tlb_match_idx(thread->processor_ptr, i, asid, VA)) {
#ifdef VERIFICATION
			//warn("match: VA=%x tlbptr=%s idx=%d thread=%d", VA, (thread->processor_ptr->tlbptr == NULL) ? "null" : "valid", i, thread->threadId);
#endif
			if (idx != 0x80000000) {
/* FIXME NOW VERIFICATION: Talk to the verif team to remove inhibit_multi_tlbmatch */
#ifdef VERIFICATION
				if (thread->processor_ptr->inhibit_multi_tlbmatch) {
					size8u_t entry;
					if (!saw_once) {
						if (thread->processor_ptr->tlbptr == NULL)
							entry = thread->processor_ptr->TLB_regs[idx];
						else
							entry = thread->processor_ptr->tlbptr[idx];
						warn("Access @ 0x%08x would cause imprecise exception: Multi TLB Match.  Matching TLB Entries:", VA);
						warn(" >>> %d: 0x%016llx", idx, entry);
						saw_once = 1;
					}
					if (thread->processor_ptr->tlbptr == NULL)
						entry = thread->processor_ptr->TLB_regs[i];
					else
						entry = thread->processor_ptr->tlbptr[i];
					warn(" >>> %d: 0x%016llx", i, entry);
				} else {
					/* Raise TLB Double Hit Exception */
					warn("Access @ 0x%08x: imprecise exception: Multi TLB Match.  Entry %d vs. %d", VA, idx, i);
					register_imprecise_exception(thread,
												 IMPRECISE_CAUSE_MULTI_TLB_MATCH,
												 0x40 | thread->threadId);
					return idx;
				}
#else
				/* Raise TLB Double Hit Exception */
				warn("Access @ 0x%08x: imprecise exception: Multi TLB Match.  Entry %d vs. %d", VA, idx, i);
				register_imprecise_exception(thread,
											 IMPRECISE_CAUSE_MULTI_TLB_MATCH,
											 0x40 | thread->threadId);
				return idx;
#endif
			} else {
				/* Hit return the idx that hit */
				idx = i;
			}
			/* Save the guess */
			proc->VA_to_idx_guess[guessidx] = idx;
		}
	}

#ifdef VERIFICATION
    if (thread->processor_ptr->inhibit_multi_tlbmatch) {
        if (saw_once && (thread->processor_ptr->tlbmatch_hint>-1)) {

            idx = thread->processor_ptr->tlbmatch_hint;
            thread->processor_ptr->tlbmatch_hint = -1;
            warn(" TLB Multihit using hint for idx=%d", idx);
        }
    }
#endif

	return (idx);

}


void sys_utlb_invalidate(processor_t * proc, thread_t *thread)
{
	int i;
	if (LIKELY(thread != NULL)) {
		/* For the specified thread, or if all threads if th==NULL... */
		/* Clear utlbs */
		for (i = 0; i < NUM_MEM_UTLB_ENTRIES; i++) {
			thread->arch_utlb_tags[i] = 0;
		}
		thread->arch_utlb_replacement_idx = 0;
		/* Clear I-tlbs */
		thread->fetch_samepage = 0;
		/* Increment FMC use-counter for requested thread, or for all threads if NULL */
		thread->fmc_use_counter++;
		if (UNLIKELY(thread->fmc_use_counter == 0)) {
		/* If the counter rolled over, bulk invalidate */
			for (i = 0; i < FAST_MEM_CACHE_SIZE; i++) {
				thread->fast_mem_cache[i].hostptr = 0;
			}
		}
	} else {
		/* Do the per-thread thing for all threads */
		for (i = 0; i < proc->runnable_threads_max; i++) {
			if (THREAD_IS_ON(proc, i)) {
				sys_utlb_invalidate(proc,proc->thread[i]);
			}
		}
	}
}

int fmc_insert(thread_t * thread, size4u_t va)
{
	processor_t *proc = thread->processor_ptr;
	exception_info einfo;
	xlate_info_t xinfo;
	int idx = FMC_IDX(va);
	int stable_page = 0;
	char *hostptr;

    if (!thread->bq_on) {
        INC_PSTAT(pfmcmiss);
    }

	if (UNLIKELY(sys_xlate(thread,va,TYPE_LOAD,access_type_load,0,0,&xinfo,&einfo) == 0)) return 0;
	if (UNLIKELY(sys_xlate(thread,va,TYPE_STORE,access_type_store,0,0,&xinfo,&einfo) == 0)) return 0;
	if(TCM_RANGE(proc, xinfo.pa)
#ifdef CLADE
		 || CLADE_REG_RANGE(proc, xinfo.pa, 1)
		 || CLADE_RANGE(proc, xinfo.pa, 1)
#endif

#ifdef CLADE2
		 || CLADE2_REG_RANGE(proc, xinfo.pa, 1)
		 || CLADE2_RANGE(proc, xinfo.pa, 1)
#endif
		 ) return 0;
	hostptr = sim_mem_get_host_ptr(thread->system_ptr,xinfo.pa & ~0x0FFFULL, 1, &stable_page);
	if (stable_page) {
		thread->fast_mem_cache[idx].hostptr = hostptr;
		thread->fast_mem_cache[idx].va_tag = va & ~0x0FFF;
		thread->fast_mem_cache[idx].use_counter = thread->fmc_use_counter;
		return FMC_ENABLED;
	}
	return 0;
}



/* EJP: FIXME: make generic with speedup */
void mem_itlb_update(thread_t * thread, size8u_t entry, int jtlbidx)
{
	size4u_t pagemask, ppd;
	ppd = GET_PPD(entry);
	pagemask = encmask_2_mask[get_pgsize(ppd) & 0x7];
	thread->fetch_pagemask = pagemask;
//  printf("%x:%d\n", ~thread->fetch_pagemask, jtlbidx);
	thread->fetch_pa = (((size8u_t) get_ppn(ppd)) << 12) & (~((size8u_t) pagemask));
	thread->fetch_samepage = 1;
}


/* FIXME: types should match mem_access_types */

static void fill_einfo_fetch(thread_t *thread, exception_info *einfo,
		size4u_t type, size4u_t cause, size4u_t va)
{
	memset(einfo,0,sizeof(*einfo));
	einfo->valid = 1;
	einfo->type = type;
	einfo->cause = cause;
	einfo->badva0 = thread->Regs[REG_BADVA0];
	einfo->badva1 = thread->Regs[REG_BADVA1];
	einfo->bvs = GET_SSR_FIELD(SSR_BVS);
	einfo->bv0 = GET_SSR_FIELD(SSR_V0);
	einfo->bv1 = GET_SSR_FIELD(SSR_V1);
	einfo->elr = thread->Regs[REG_PC];
	if (cause == TLBMISSX_CAUSE_ICINVA) {
		einfo->badva0 = va & 0xFFFFF000;
		einfo->bvs = 0;
		einfo->bv0 = 1;
		einfo->bv1 = 0;
	}

#ifdef VERIFICATION
	SYSVERWARN("fill_einfo_fetch type=%d cause=%d va=%x", type, cause, va);
#endif


}

static void fill_einfo_ldst(thread_t *thread, exception_info *einfo, size4u_t type, size4u_t slot,
		size4u_t cause, size4u_t va)
{
	memset(einfo,0,sizeof(*einfo));
	einfo->valid = 1;
	einfo->type = type;
	einfo->cause = cause;
	if (slot == 0) {
		einfo->badva0 = va;
		einfo->badva1 = thread->Regs[REG_BADVA1];
		einfo->bv0 = 1;
		einfo->bv1 = 0;
		einfo->bvs = 0;
	} else {
		einfo->badva0 = thread->Regs[REG_BADVA0];
		einfo->badva1 = va;
		einfo->bv0 = 0;
		einfo->bv1 = 1;
		einfo->bvs = 1;
	}
	einfo->elr = thread->Regs[REG_PC];
	einfo->de_slotmask = 1<<slot;

}


void register_dma_error_exception(thread_t * thread, exception_info *einfo, size4u_t va) {
	fill_einfo_ldst(thread,einfo, EXCEPT_TYPE_PRECISE, 0, PRECISE_CAUSE_DMA_ERROR, va);
}


static inline void fill_einfo_ldsterror(thread_t *thread, exception_info *einfo, size4u_t slot,
		size4u_t cause, size4u_t va) {
	//SYSVERWARN("fill_einfo_ldsterror EXCEPT_TYPE_PRECISE cause=%x va=%x", cause, va);
	fill_einfo_ldst(thread,einfo,EXCEPT_TYPE_PRECISE,slot,cause,va);
}

static inline void fill_einfo_fetcherror(thread_t *thread, exception_info *einfo,
		size4u_t cause, size4u_t va) {
	fill_einfo_fetch(thread,einfo,EXCEPT_TYPE_PRECISE,cause,va);
}

static inline void fill_einfo_tlbmissrw(thread_t *thread, exception_info *einfo, size4u_t va,
		int access_type, int slot) {
	//SYSVERWARN("fill_einfo_ldsterror EXCEPT_TYPE_TLB_MISS_RW read=%d va=%x", (access_type == TYPE_LOAD), va);
	fill_einfo_ldst(thread, einfo, EXCEPT_TYPE_TLB_MISS_RW, slot,
		(access_type == TYPE_LOAD) ? TLBMISSRW_CAUSE_READ : TLBMISSRW_CAUSE_WRITE, va);
}

static inline void fill_einfo_tlbmissx(thread_t *thread, exception_info *einfo, size4u_t va,
		int access_type) {
	int cause;
	if (access_type == TYPE_ICINVA) cause = TLBMISSX_CAUSE_ICINVA;
	else if (((thread->Regs[REG_PC] ^ va) & 0xFFFFF000) != 0) cause = TLBMISSX_CAUSE_NEXTPAGE;
	else cause = TLBMISSX_CAUSE_NORMAL;
	//SYSVERWARN("fill_einfo_tlbmissx  EXCEPT_TYPE_TLB_MISS_X cause=%x", cause);
	fill_einfo_fetch(thread,einfo,EXCEPT_TYPE_TLB_MISS_X, cause, va);
}

static inline void fill_einfo_align(thread_t *thread, exception_info *einfo, int slot, int access_type, size4u_t va)
{
	if ((access_type == TYPE_FETCH) || (access_type == TYPE_ICINVA)) {
		fill_einfo_fetcherror(thread,einfo,PRECISE_CAUSE_PC_NOT_ALIGNED,va);
	} else {
		fill_einfo_ldsterror(thread,einfo,slot,(access_type == TYPE_LOAD) ?
			PRECISE_CAUSE_MISALIGNED_LOAD :
			PRECISE_CAUSE_MISALIGNED_STORE, va);
	}
}


void sys_check_framelimit_slowpath(thread_t *thread, size4u_t vaddr, size4u_t limit, size4u_t badva)
{
	exception_info einfo;
	if (sys_in_monitor_mode(thread)) return;
	if (EXCEPTION_DETECTED) return;
	fill_einfo_ldsterror(thread,&einfo,0,PRECISE_CAUSE_STACK_LIMIT,badva);
	register_einfo(thread,&einfo);
}

static int sys_xlate_mmuoff(thread_t *thread, size4u_t va, int access_type, int maptr_type, int slot,
	size4u_t align_mask, xlate_info_t *xinfo, exception_info *einfo)
{
	if (va & align_mask) {
		fill_einfo_align(thread,einfo,slot,access_type,va);
		return 0;
	}
	xinfo->transtype = XLATE_TRANSTYPE_NONE;
	xinfo->perms.p_xwr = xinfo->perms.u_xwr = 0x7;
	xinfo->pa = va;
	xinfo->size = 12;
	xinfo->inner.raw = xinfo->outer.raw = xinfo->memtype.raw = 0;

	xinfo->pte_atr0 = 0;
	xinfo->pte_atr1 = 0;


	if ((access_type == TYPE_FETCH) || (access_type == TYPE_ICINVA)) {
		if (fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_ICEN)) {
			xinfo->inner.cacheable = 1;
		}
	} else if (fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_DCEN)) {
		xinfo->memtype.arch_cacheable = xinfo->inner.cacheable = 1;
	}
	else {  // data access, dcache disabled
		if (maptr_type <= access_type_barrier)	// Can't use NUM_CORE_ACCESS_TYPES since memcpy are not in this case
			xinfo->memtype.device = 1;
	 }
	/* FIXME: Restore this case if vmem and memcpy accesses with mmu off, dcache
		 off are device-type like everything else */

	return 1;
}

static int pa_is_in_vtcm(processor_t * proc, paddr_t paddr) {
	int in_tcm = 0;
	if (proc->features->QDSP6_VX_PRESENT) {
        paddr_t vtcm_lowaddr = proc->options->l2tcm_base + ARCHOPT(vtcm_offset);
        paddr_t vtcm_highaddr = vtcm_lowaddr+ARCHOPT(vtcm_size);
        int in_tcm = ((paddr >= vtcm_lowaddr) && (paddr < vtcm_highaddr));
        if (in_tcm) {
			thread_t * thread = proc->thread[0];
			warn("Non-monitor fetch from VTCM space causes exception (%016llx <= %016llx < %016llx)",vtcm_lowaddr,paddr,vtcm_highaddr);
		}
    }
	return in_tcm;
}


static int sys_xlate_nowalk(thread_t *thread, size4u_t va, int access_type, int slot,
	size4u_t align_mask, xlate_info_t *xinfo, exception_info *einfo)
{
	processor_t *proc = thread->processor_ptr;
	int idx = -1;
	int in_monitor_mode = sys_in_monitor_mode(thread);
	int in_user_mode = sys_in_user_mode(thread);
	size8u_t entry,entry_PA,entry_PGSIZE;
	size4u_t cccc;
	idx = tlb_lookup(thread,READ_RREG(REG_SSR),va,0);
	xinfo->jtlbidx = (idx < 0) ? 0xff : idx;
	xinfo->transtype = XLATE_TRANSTYPE_ARCHTLB;
	if (UNLIKELY(idx < 0)) {
		if (va & align_mask) {
			fill_einfo_align(thread,einfo,slot,access_type,va);
			return 0;
		}
		if ((access_type == TYPE_FETCH) || (access_type == TYPE_ICINVA)) {
			fill_einfo_tlbmissx(thread,einfo,va,access_type);
			return 0;
		} else {
			fill_einfo_tlbmissrw(thread,einfo,va,access_type,slot);
			return 0;
		}
	}
	entry = proc->TLB_regs[idx];
	entry_PA = get_ppn(GET_PPD(entry));
	entry_PGSIZE = get_pgsize(GET_PPD(entry));
	IFVERIF(ver_tlb_used(proc,thread->threadId,idx);)


	paddr_t pa = ((entry_PA << 12) & ~(encmask_2_mask[entry_PGSIZE & 0x7])) | (va & encmask_2_mask[entry_PGSIZE & 0x7]);

	switch (access_type) {
	case TYPE_FETCH:
		// If page is in VTCM, except HEXDEV-4932
		if (pa_is_in_vtcm(proc, pa)) {
			SYSVERWARN("PA in VTCM on fetch pa=%llx", pa);
			fill_einfo_fetcherror(thread,einfo,PRECISE_CAUSE_FETCH_NO_XPAGE,va);
			return 0;
		}
		if (!in_monitor_mode && !GET_FIELD(PTE_X,entry)) {
			SYSVERWARN("Not in monitor mode and PTE_X no set entry=%llx idx=%d", entry, idx);
			fill_einfo_fetcherror(thread,einfo,PRECISE_CAUSE_FETCH_NO_XPAGE,va);
			return 0;
		}
		if (in_user_mode && !GET_FIELD(PTE_U,entry)) {
			SYSVERWARN("In user mode and PTE_U not set entry=%llx idx=%d", entry, idx);
			fill_einfo_fetcherror(thread,einfo,PRECISE_CAUSE_FETCH_NO_UPAGE,va);
			return 0;
		}
		/* EJP: FIXME: perms check on icinva causes nop */
		/* EJP: FIXME: icinva doesn't do alignment? */
		/* FALLTHROUGH */
	case TYPE_ICINVA:
		if (va & align_mask) {
			SYSVERWARN("alignment on ICINVA alignment=%x", va & align_mask);
			fill_einfo_align(thread,einfo,0,access_type,va);
			return 0;
		}
		break;
	case TYPE_LOAD:
		if (va & align_mask) {
			//SYSVERWARN("load alignment=%x", va & align_mask);
			fill_einfo_align(thread,einfo,slot,access_type,va);
			return 0;
		}
		if (!in_monitor_mode && !GET_FIELD(PTE_R,entry)) {
			SYSVERWARN("Not in monitor mode and PTE_R not set entry=%llx idx=%d", entry, idx);
			fill_einfo_ldsterror(thread,einfo,slot,PRECISE_CAUSE_PRIV_NO_READ,va);
			return 0;
		}
		if (in_user_mode && !GET_FIELD(PTE_U,entry)) {
			SYSVERWARN("In user mode and PTE_U not set entry=%llx idx=%d", entry, idx);
			fill_einfo_ldsterror(thread,einfo,slot,PRECISE_CAUSE_PRIV_NO_UREAD,va);
			return 0;
		}
		break;
	case TYPE_STORE:
		if (va & align_mask) {
			//SYSVERWARN("store alignment=%x", va & align_mask);
			fill_einfo_align(thread,einfo,slot,access_type,va);
			return 0;
		}
		if (!in_monitor_mode && !GET_FIELD(PTE_W,entry)) {
			SYSVERWARN("Not in monitor mode and PTE_W not entry=%llx idx=%d", entry, idx);
			fill_einfo_ldsterror(thread,einfo,slot,PRECISE_CAUSE_PRIV_NO_WRITE,va);
			return 0;
		}
		if (in_user_mode && !GET_FIELD(PTE_U,entry)) {
			SYSVERWARN("In user mode and PTE_U not entry=%llx idx=%d", entry, idx);
			fill_einfo_ldsterror(thread,einfo,slot,PRECISE_CAUSE_PRIV_NO_UWRITE,va);
			return 0;
		}
		break;
	default:
		fatal("bad access type in sys_xlate call");
	}
	xinfo->pa = pa;
	xinfo->size = (entry_PGSIZE*2)+12;
	xinfo->global = GET_FIELD(PTE_G, entry);

	xinfo->pte_atr0 = GET_FIELD(PTE_ATR0, entry);
	xinfo->pte_atr1 = GET_FIELD(PTE_ATR1, entry);

	xinfo->pte_u = GET_FIELD(PTE_U, entry);
	xinfo->pte_r = GET_FIELD(PTE_R, entry);
	xinfo->pte_w = GET_FIELD(PTE_W, entry);
	xinfo->pte_x = GET_FIELD(PTE_X, entry);
	xinfo->memtype.vtcm = in_vtcm_space_proc(proc, pa);

	cccc = GET_FIELD(PTE_C,entry);
	xinfo->cccc = cccc;
	if (!((cccc == CCCC_2) || (cccc == CCCC_3) ||(cccc == CCCC_4) || (cccc == CCCC_6))) xinfo->memtype.arch_cacheable = 1;
	switch (cccc) {
	case CCCC_0: xinfo->inner.cacheable = xinfo->inner.write_back = 1; break;
	case CCCC_1: xinfo->inner.cacheable = 1; break;
	case CCCC_2: xinfo->memtype.device = 1; break;  //device-type SFC
	case CCCC_3: xinfo->memtype.device = 1;; break; // Uncached SFC, but treated like Device type per HW-Issue 2794
	case CCCC_4: xinfo->memtype.device = 1; break;
	case CCCC_5:
        if(access_type==TYPE_FETCH) {
            xinfo->inner.cacheable = 0;
            xinfo->outer.cacheable = 1;
        } else {
            xinfo->inner.cacheable = xinfo->outer.cacheable = 1;
        }
        break;
	case CCCC_6: break;
	case CCCC_7: xinfo->inner.cacheable = xinfo->outer.cacheable = 1;
		xinfo->inner.write_back = xinfo->outer.write_back = 1; break;
	case CCCC_8: xinfo->inner.cacheable = xinfo->outer.cacheable = 1;
		xinfo->inner.write_back = 1; break;
	case CCCC_9: xinfo->inner.cacheable = xinfo->outer.cacheable = 1;
		xinfo->outer.write_back = 1; break;
	case CCCC_10:
        if(access_type==TYPE_FETCH) {
            xinfo->inner.cacheable = 0;
            xinfo->outer.cacheable = 1;
        } else {
            xinfo->inner.cacheable = xinfo->outer.cacheable = 1;
        }
		xinfo->inner.write_back = xinfo->outer.write_back = 1;
		xinfo->inner.transient = xinfo->outer.transient = 1;
        break;
	case CCCC_11:
        if(access_type==TYPE_FETCH) {
            xinfo->inner.cacheable = 0;
            xinfo->outer.cacheable = 1;
        } else {
            xinfo->inner.cacheable = xinfo->outer.cacheable = 1;
        }
        xinfo->inner.transient = xinfo->outer.transient = 1;
        break;
	case CCCC_12: break; /* RSVD */
	case CCCC_13: xinfo->outer.cacheable = 1; break;
	case CCCC_14: break; /* RSVD */
	case CCCC_15: xinfo->outer.cacheable = xinfo->outer.write_back = 1; break;
	default: break; /* RSVD */
	};

	// HEXDEV-9788 Small Core doesn't support all the CCCC fields
	// Want to assert so user knows not use this properly
	// RTL won't test these either
	if (proc->arch_proc_options->small_cccc_table) {
		switch (cccc) {
			case CCCC_1: break;
			case CCCC_2: break;
			case CCCC_3: break;
			case CCCC_4: break;
			case CCCC_5: break;
			case CCCC_7: break;
			default:
				xinfo->memtype.arch_cacheable = 0;
				xinfo->inner.cacheable = 0;
				xinfo->outer.cacheable = 0;
				warn("Unsupported CCCC=%d in small core is reserved, undefined and untested: %d\n",cccc);
		}
	}



	if (in_vtcm_space_proc(proc, xinfo->pa) && ARCHOPT(vtcm_scalar_uncached)) {
		xinfo->memtype.arch_cacheable = 0;
	}

	if (fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_L2CFG) == 0)
		xinfo->outer.cacheable = xinfo->outer.write_back = 0;
	if (!fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_L2WB)) xinfo->outer.write_back = 0;
	xinfo->inner.read_allocate = xinfo->inner.cacheable;
	xinfo->inner.write_allocate = xinfo->inner.write_back;
	xinfo->outer.read_allocate = xinfo->outer.cacheable;
	xinfo->outer.write_allocate = xinfo->outer.write_back;
	if (fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_L2NWA)) xinfo->outer.write_allocate = 0;
	if (fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_L2NRA)) xinfo->outer.read_allocate = 0;
#ifdef VERIFICATION
	if (!thread->verif_dma_xlate) {
		SYSVERWARN("xlate success: type=%c VA=0x%08x PA=0x%016llx tlbidx=%d cccc=0x%x size=%d tlbentry=0x%016llx",
			access_type,va,xinfo->pa,idx,cccc,xinfo->size,entry);
	}
#endif

	return 1;
}

static void walkdebugcallback(void *vthread, vaddr_t transaddr, int stage, int level,
	int width, paddr_t ipa, paddr_t pa, const walk_mem_info_t *access_meminfo,
	const walk_mem_perms_t tableperms) {
	thread_t *thread = vthread;
	warn("---> transaddr=%x level=%d pa=%016llx val=%016llx",transaddr,level,pa,
		sim_mem_read8(thread->system_ptr,0,pa));
}

static int sys_xlate_armwalk(thread_t *thread, size4u_t va, int access_type, int slot,
	size4u_t align_mask, xlate_info_t *xinfo, exception_info *einfo)
{
	walk_info_t info;
	size4u_t perms;
	xinfo->transtype = XLATE_TRANSTYPE_ARCHTLB;
	if (va & align_mask) {
		fill_einfo_align(thread,einfo,slot,access_type,va);
		return 0;
	}
	if (walk(thread,va,&info,walkdebugcallback) != 0) {
		fatal("Some kind of page walk error! Should except!!!???");
	}
	xinfo->pa = info.pa;
	xinfo->perms.p_xwr = info.meminfo.perms.p_xwr;
	xinfo->perms.u_xwr = info.meminfo.perms.u_xwr;
	xinfo->memtype.raw = info.meminfo.attribs.mem.raw;
	xinfo->inner.raw = info.meminfo.attribs.inner.raw;
	xinfo->outer.raw = info.meminfo.attribs.outer.raw;
	xinfo->size = info.pagesize;
	xinfo->global = info.global;

	xinfo->pte_atr0 = 0;
	xinfo->pte_atr1 = 0;

	if (info.meminfo.perms.access == 0) {
		warn("xlate fail: No access!");
		fill_einfo_ldsterror(thread,einfo,slot,PRECISE_CAUSE_DMA_ERROR,va);
		return 0;
	}
	if (sys_in_monitor_mode(thread) || sys_in_user_mode(thread)) perms = xinfo->perms.u_xwr;
	else perms = xinfo->perms.p_xwr;
	switch (access_type) {
	case TYPE_FETCH:
	case TYPE_ICINVA:
		if ((perms & 4) == 0) {
			warn("xlate fail: No fetch!");
			fill_einfo_fetcherror(thread,einfo,PRECISE_CAUSE_FETCH_NO_XPAGE,va);
			return 0;
		}
		break;
	case TYPE_LOAD:
		if ((perms & 1) == 0) {
			warn("xlate fail: No read!");
			fill_einfo_ldsterror(thread,einfo,slot,PRECISE_CAUSE_PRIV_NO_READ,va);
			return 0;
		}
		break;
	case TYPE_STORE:
		if ((perms & 2) == 0) {
			warn("xlate fail: No store!");
			fill_einfo_ldsterror(thread,einfo,slot,PRECISE_CAUSE_PRIV_NO_WRITE,va);
			return 0;
		}
	default:
		fatal("bad access type");
	}
	//warn("OK, this is pretty cool.  pa=%016llx meminfo=%016llx size=%x global=%x",info.pa,info.meminfo.raw, info.pagesize, info.global);
	//warn("FIXME: check permissions");
	//fatal("Walk succeeded, but fix this part too");
	return 1;
}

/*
 * Try and get information from translation cache
 * If we return 0, it doesn't mean that the translation failed, just that you need to
 * do it the slow and more complete way
 */
#define UTLB_MINPAGE_SIZE 4096
static inline int sys_xlate_cached(thread_t *thread, size4u_t va, int access_type,
	size4u_t align_mask, xlate_info_t *xinfo)
{
	size4u_t tag;
	int i;
#ifdef VERIFICATION
	return 0;					// verif always xlate prints & callbacks
#endif
	if (va & align_mask) return 0;
	tag = (va & -UTLB_MINPAGE_SIZE) ^ access_type;
	for (i = 0; i < NUM_MEM_UTLB_ENTRIES; i++) {
		if (thread->arch_utlb_tags[i] == tag) {
			*xinfo = thread->arch_utlb_data[i];
			xinfo->pa |= (va & (UTLB_MINPAGE_SIZE-1));
			return 1;
		}
	}
	return 0;
}

static inline void sys_xlate_cache_update(thread_t *thread, size4u_t va, int access_type, xlate_info_t *xinfo)
{
	size4u_t tag;
	int idx = thread->arch_utlb_replacement_idx;
	tag = (va & -UTLB_MINPAGE_SIZE) ^ access_type;
	thread->arch_utlb_tags[idx] = tag;
	thread->arch_utlb_data[idx] = *xinfo;
	thread->arch_utlb_data[idx].pa &= -UTLB_MINPAGE_SIZE;
	idx = (idx + 1) % NUM_MEM_UTLB_ENTRIES;
	thread->arch_utlb_replacement_idx = idx;
}

void sys_xlate_cache_error(thread_t *thread, xlate_info_t *cached, xlate_info_t *xinfo, size4u_t va)
{
	warn("OOPS: cached xlate info != looked up xlate info.  Va=%08x\n",va);
}

void xlate_in_l1s_space_proc(thread_t *thread, paddr_t paddr) {
	processor_t *proc = thread->processor_ptr;
	if(!proc->arch_proc_options->l1s_enable || !proc->arch_proc_options->xlate_l1s_check){
		return;
	}
	paddr_t l1s_lowaddr = proc->options->l2tcm_base + ARCHOPT(l1s_size) + ARCHOPT(l1s_offset0);
	paddr_t l1s_highaddr = proc->options->l2tcm_base + ARCHOPT(tcm_region_size);
	if((paddr >= l1s_lowaddr) && (paddr < l1s_highaddr)) {
		if (ARCHOPT(l1s_offset1)) {
			l1s_lowaddr = proc->options->l2tcm_base + ARCHOPT(l1s_offset1);
			l1s_highaddr = proc->options->l2tcm_base + ARCHOPT(l1s_size) + ARCHOPT(l1s_offset1);
			if((paddr >= l1s_lowaddr) && (paddr < l1s_highaddr)){
				return;
			}
		}
		pfatal(thread->processor_ptr, "Invalid Region of Memory at Translation: Thread=%d PA=0x%09llx",
		thread->threadId,
		paddr);
	}
}




int
sys_xlate(thread_t *thread, size4u_t va, int access_type, int maptr_type,int slot, size4u_t align_mask,
	xlate_info_t *xinfo, exception_info *einfo)
{
	int ret;
	if (LIKELY(ret = sys_xlate_cached(thread,va,access_type,align_mask,xinfo))) {
		xlate_in_l1s_space_proc(thread, xinfo->pa);
		return ret;
	}
	memset(xinfo,0,sizeof(*xinfo));
	if (UNLIKELY((GLOBAL_REG_READ(REG_SYSCONF) & 0x1)) == 0) {
		ret = sys_xlate_mmuoff(thread,va,access_type,maptr_type,slot,align_mask,xinfo,einfo);
		xlate_in_l1s_space_proc(thread, xinfo->pa);
		return ret;
	} else if (1 /*LIKELY(GET_SSR_FIELD(SSR_TW) == 0)*/ ) {
		ret = sys_xlate_nowalk(thread,va,access_type,slot,align_mask,xinfo,einfo);
	} else {
		ret = sys_xlate_armwalk(thread,va,access_type,slot,align_mask,xinfo,einfo);
	}
	if (ret) sys_xlate_cache_update(thread,va,access_type,xinfo);

	xlate_in_l1s_space_proc(thread, xinfo->pa);

	return ret;
}

/* Translate with specified SSR */
/*
 * EJP: FIXME: this is a huge kludge
 * But I expect that when we have HW PTW we have to redo a lot of things
 * So... quick kludge it is.
 */
int
sys_xlate_ssr(thread_t *thread, size4u_t ssr, size4u_t va, int access_type, int slot, size4u_t align_mask,
	xlate_info_t *xinfo, exception_info *einfo)
{
	size4u_t ssr_old;
	int ret;
	/* Capture SSR */
	ssr_old = thread->Regs[REG_SSR];
	/* Replace SSR temporarily */
	thread->Regs[REG_SSR] = ssr;
	/* Do the translation... it's not supposed to change state or anything... */
	ret = sys_xlate(thread,va,access_type,access_type_load,slot,align_mask,xinfo,einfo);
	/* Restore original SSR... hope that works right! */
	thread->Regs[REG_SSR] = ssr_old;
	return ret;
}

size4u_t imem_try_read4(thread_t * thread, size4u_t vaddr, size4u_t * value, xlate_info_t *xinfo, exception_info *einfo)
{
	mem_access_info_t memaccess;
	size8u_t data;

	if (REPLAY_DETECTED) return 0;
	if (sys_xlate(thread,vaddr,TYPE_FETCH,access_type_fetch,0,PCALIGN_MASK,xinfo,einfo) == 0) {
		return 0;
	}
	memaccess.paddr = xinfo->pa;
	memaccess.vaddr = vaddr;
	memaccess.width = 4;
	memaccess.type = access_type_fetch;
	memaccess.bp = GET_SSR_FIELD(SSR_BP);
	memaccess.tnum = thread->threadId;
	data = memread(thread, &memaccess, 0);
	*value = (size4u_t)data;

	//	*value = sim_mem_read4(thread->system_ptr, thread->threadId, xinfo.pa);
	SYSVERWARN("read instruction word: thread=%d VA=0x%08x PA=0x%09llx value=0x%08x",
		thread->threadId,
		vaddr,
		xinfo->pa,
		*value);
	return 1;
}


void mem_finished_slot_cleanup_timing(thread_t * thread, int slot)
{
	thread->mem_access[slot].check_page_crosses = 0;
	thread->mem_access[slot].post_updated = 0;
}

#endif

paddr_t
mem_init_access(thread_t * thread, int slot, size4u_t vaddr, int width,
				enum mem_access_types mtype, int type_for_xlate)
{
	mem_access_info_t *maptr = &thread->mem_access[slot];


#ifdef FIXME
	maptr->is_memop = 0;
	maptr->log_as_tag = 0;
	maptr->no_deriveumaptr = 0;
	maptr->is_dealloc = 0;
	maptr->dropped_z = 0;

	exception_info einfo;
#endif

	/* The basic stuff */
#ifdef FIXME
	maptr->bad_vaddr = maptr->vaddr = vaddr;
#else
	maptr->vaddr = vaddr;
#endif
	maptr->width = width;
	maptr->type = mtype;
#ifdef FIXME
	maptr->tnum = thread->threadId;
#endif
    maptr->cancelled = 0;
    maptr->valid = 1;

	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
#ifdef FIXME
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->xe = GET_SSR_FIELD(SSR_XE);
    maptr->xa = GET_SSR_FIELD(SSR_XA);

	/* For trace in the uarch */
	maptr->pc_va = thread->Regs[REG_PC];

	/* Something is in uTLB only if permissions are good, it's cacheable,
	   MMU en, and $on
	   Then all we need to check is alignment.
	   This fast-path needs to be disabled for verification as the testbench
	   forces what we use for uTLB snapshots */
	if (sys_xlate(thread,vaddr,type_for_xlate,mtype,slot,width-1,&maptr->xlate_info,&einfo)==0) {
		register_einfo(thread,&einfo);
	}
	maptr->paddr = maptr->xlate_info.pa;

	/* BIU has higher precedence than TLB miss, but if translation fails then we don't have a PA to check here, */
	/* so only check this once we have a valid PA */
	if (L2VIC_RANGE(thread->processor_ptr, maptr->paddr) &&
			((mtype != access_type_load
				&& mtype != access_type_store
				&& mtype != access_type_load_phys
				&& mtype != access_type_load_locked
				&& mtype != access_type_store_conditional) ||                       // require ldst
			 (width != 4) ||                                                      // require word access
			 (maptr->xlate_info.memtype.arch_cacheable))) {                       // require non-cacheable
		size4u_t badva = slot ? thread->Regs[REG_BADVA1] :  thread->Regs[REG_BADVA0];
		fill_einfo_ldsterror(thread, &einfo, slot, PRECISE_CAUSE_BIU_PRECISE, badva);
		register_einfo(thread, &einfo);
	}
#else
    maptr->paddr = vaddr;
#endif



// HW-Issue 3528
#ifdef VERIFICATION
    if (C_DEVICE(maptr->xlate_info.memtype.device) && (type_for_xlate == TYPE_LOAD)) {
        thread->has_devtype_ld |= (1<<slot);
    }
    if (C_DEVICE(maptr->xlate_info.memtype.device) && (type_for_xlate == TYPE_STORE)) {
        thread->has_devtype_st |= (1<<slot);
    }

#endif


#ifdef FIXME
	if((EXCEPTION_DETECTED) && ((thread->einfo.type == EXCEPT_TYPE_TLB_MISS_RW) ||
		(thread->einfo.type == EXCEPT_TYPE_TLB_MISS_X)) ) {
        if (!thread->bq_on) {
            INC_TSTAT(tjtlb_miss);
        }
	}
#endif

	return (maptr->paddr);
}

paddr_t
mem_init_access_unaligned(thread_t *thread, int slot, size4u_t vaddr, size4u_t realvaddr, int size,
	enum mem_access_types mtype, int type_for_xlate)
{
	paddr_t ret;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	ret = mem_init_access(thread,slot,vaddr,1,mtype,type_for_xlate);
	maptr->vaddr = realvaddr;
	maptr->paddr -= (vaddr-realvaddr);
	maptr->width = size;
	return ret;
}


#ifdef FIXME
/* This one will not cause exceptions. If a VA to PA translation is not found,
   it sets the paddr to 0x0 */
paddr_t
mem_init_access_silent(thread_t * thread, int slot, size4u_t vaddr,
					   int width, enum mem_access_types mtype,
					   int type_for_xlate, int cancel)
{
	mem_access_info_t *maptr = &thread->mem_access[slot];
	exception_info einfo;

    memset(maptr, 0, sizeof(mem_access_info_t));

	/* The basic stuff */
	maptr->bad_vaddr = maptr->vaddr = vaddr;
	maptr->width = width;
	maptr->type = mtype;
	maptr->tnum = thread->threadId;
    maptr->cancelled = 0;
    maptr->valid = 1;
	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
	maptr->bp = GET_SSR_FIELD(SSR_BP);


	/* For trace in the uarch */
	maptr->pc_va = thread->Regs[REG_PC];

	if(sys_xlate(thread, vaddr, type_for_xlate, mtype, slot, width-1, &maptr->xlate_info, &einfo) == 0) {
	    /* Do not generate an exception for silent */
	    maptr->paddr = 0;
	} else {
	    maptr->paddr = maptr->xlate_info.pa;
	}

	return (maptr->paddr);
}

inline paddr_t
mem_init_access_direct(thread_t * thread, int slot, size4u_t vaddr,
					   int width, enum mem_access_types mtype,
					   int type_for_xlate)
{
	mem_access_info_t *maptr = &thread->mem_access[slot];

	/* The basic stuff */
	maptr->bad_vaddr = maptr->vaddr = vaddr;
	maptr->width = width;
	maptr->type = mtype;
	//maptr->iswrite = (type_for_xlate == TYPE_STORE);
	maptr->tnum = thread->threadId;
    maptr->cancelled = 0;
    maptr->valid = 1;


	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
	maptr->bp = GET_SSR_FIELD(SSR_BP);

	/* For trace in the uarch */
	maptr->pc_va = thread->Regs[REG_PC];

	return (maptr->paddr);
}

static inline size8u_t sys_mem_merge_inflight_store(thread_t *thread,
		paddr_t ldpaddr, int ldwidth, size8u_t data)
{
	union {
		size1u_t bytes[8];
		size8u_t data;
	} retdata,mergedata;
	int i,j;
	int stwidth = thread->mem_access[1].width;
	int bigmask = ((-ldwidth) & (-stwidth));
	paddr_t stpaddr = thread->mem_access[1].paddr;
	if (LIKELY((ldpaddr & bigmask) != (stpaddr & bigmask))) return data;	// no overlap?
	retdata.data = data;
	mergedata.data = thread->mem_access[1].stdata;
	i = stpaddr & (ldwidth-1);
	j = ldpaddr & (stwidth-1);
	while ((i < ldwidth) && (j < stwidth)) {
		retdata.bytes[i] = mergedata.bytes[j];
		i++;
		j++;
	}
#ifdef VERIFICATION
	if (data != retdata.data) {
		warn("merged inflight data: old=0x%llx merge=0x%llx new=%llx\n",
			data,mergedata.data,retdata.data);
	}
#endif
	return retdata.data;
}

int check_release(thread_t * thread, size4u_t vaddr, paddr_t paddr, size1u_t data, insn_t *insn) {
	processor_t *proc = (processor_t *)thread->processor_ptr;

	size8u_t entry;
	size4u_t cccc = 0;
	int in_ahb, in_tcm, in_vtcm, in_axi2, in_l2itcm;
	int MMU_check = 0;
	int idx = -1;
	int slot = insn->slot;
	exception_info einfo;

	if(GET_ATTRIB(insn->opcode,A_RELEASE)) {
		paddr_t ahb_lowaddr = proc->arch_proc_options->ahb_lowaddr;
		paddr_t ahb_highaddr = proc->arch_proc_options->ahb_highaddr;
		paddr_t axi2_lowaddr = proc->arch_proc_options->axi2_lowaddr;
		paddr_t axi2_highaddr = proc->arch_proc_options->axi2_highaddr;
		paddr_t tcm_lowaddr =  thread->processor_ptr->options->l2tcm_base;
		paddr_t tcm_highaddr = tcm_lowaddr + arch_get_tcm_size(thread->processor_ptr) - 1;
		paddr_t l2itcm_lowaddr =  ARCHOPT(l2itcm_base);
		paddr_t l2itcm_highaddr = l2itcm_lowaddr + ARCHOPT(l2itcm_size) - 1;
		paddr_t vtcm_lowaddr = proc->options->l2tcm_base + ARCHOPT(vtcm_offset);
		paddr_t vtcm_highaddr = vtcm_lowaddr+ARCHOPT(vtcm_size)-1;

		in_ahb  = ((paddr < ahb_highaddr) && (paddr >= ahb_lowaddr)) && ((ahb_lowaddr != 0) && ((ahb_highaddr != ahb_lowaddr) && ARCHOPT(ahb_enable)));
		in_tcm  = ((paddr >= tcm_lowaddr) && (paddr <= tcm_highaddr));
		in_l2itcm  = ((paddr >= l2itcm_lowaddr) && (paddr <= l2itcm_highaddr));
		in_vtcm = ((paddr >= vtcm_lowaddr) && (paddr <= vtcm_highaddr));
		in_axi2 = ((paddr < axi2_highaddr) && (paddr >= axi2_lowaddr)) && ((axi2_lowaddr != 0) && ((axi2_highaddr != axi2_lowaddr) && ARCHOPT(axi2_enable)));

		if(GET_FIELD(SYSCFG_MMUEN, GLOBAL_REG_READ(REG_SYSCONF))) {
			idx = tlb_lookup(thread,READ_RREG(REG_SSR),vaddr,0);
			if (UNLIKELY(idx < 0)) {
				fill_einfo_tlbmissrw(thread,&einfo,vaddr,TYPE_STORE,slot);
				register_einfo(thread,&einfo);
				return 0;
			}
			entry = proc->TLB_regs[idx];
			if (!sys_in_monitor_mode(thread) && !GET_FIELD(PTE_W,entry)) {
				SYSVERWARN("Not in monitor mode and PTE_W not entry=%llx idx=%d", entry, idx);
				fill_einfo_ldsterror(thread,&einfo,slot,PRECISE_CAUSE_PRIV_NO_WRITE,vaddr);
				register_einfo(thread,&einfo);
				return 0;
			}
			if (sys_in_user_mode(thread) && !GET_FIELD(PTE_U,entry)) {
				SYSVERWARN("In user mode and PTE_U not entry=%llx idx=%d", entry, idx);
				fill_einfo_ldsterror(thread,&einfo,slot,PRECISE_CAUSE_PRIV_NO_UWRITE,vaddr);
				register_einfo(thread,&einfo);
				return 0;
			}

			cccc = GET_FIELD(PTE_C,entry);
			if (cccc == CCCC_2 || cccc == CCCC_3 || cccc == CCCC_4) {
				MMU_check = 1;
			}
		} else if(GET_FIELD(SYSCFG_DCEN, GLOBAL_REG_READ(REG_SYSCONF)) == 0) {
			MMU_check = 1;
		}
#ifdef VERIFICATION
		warn("paddr: %llx vaddr: %x in_ahb: %d in_tcm: %d in_vtcm: %d in_axi2: %d in_l2itcm: %d MMU_check: %d MMU_en: %d cccc: %d dcache_en: %d", paddr, vaddr, in_ahb, in_tcm, in_vtcm, in_axi2, in_l2itcm, MMU_check, GET_FIELD(SYSCFG_MMUEN, GLOBAL_REG_READ(REG_SYSCONF)),cccc, GET_FIELD(SYSCFG_DCEN, GLOBAL_REG_READ(REG_SYSCONF)));
#endif
		if((in_ahb || in_axi2 || MMU_check) && !in_tcm && !in_vtcm && !in_l2itcm) {
			register_error_exception(thread,PRECISE_CAUSE_BIU_PRECISE,
				thread->Regs[REG_BADVA0],thread->Regs[REG_BADVA1],
				GET_SSR_FIELD(SSR_BVS), GET_SSR_FIELD(SSR_V0),
				GET_SSR_FIELD(SSR_V1),0);
			return 0;
		}
	}
	return 1;
}



size8u_t
mem_general_load(thread_t * thread, size4u_t vaddr, int width, insn_t *insn)
{
	paddr_t paddr;
	size8u_t data;
    int slot = insn->slot;
    //	processor_t *proc = thread->processor_ptr;

	FATAL_REPLAY;

	/* If this load has already completed, then the results are in the shadow
	   so get them from there. We don't want to do loads many times if this
	   is slot1 and slot0 is replaying.
	   Note that we will do this twice in the case that slot0 has an
	   exception. This is not strictly correct for device-type memory,
	   but we won't hold the simulator to that standard. If it does need to be
	   supported, then we'll have to check the slot0 address execeptions prior
	   to executing slot1.
	*/
	if (thread->shadow_valid[slot]) {
		return (thread->shadow_data[slot]);
	}

	/* Do address translation and bail if we hit any exception conditions */
	paddr = mem_init_access(thread, slot, vaddr, width, access_type_load, TYPE_LOAD);
	/* EJP: try and fix case where we don't want pc misaligned on dealloc_return w/ exception */
	if (EXCEPTION_DETECTED) return fFRAME_SCRAMBLE(0ULL);

	/* Do the data read */
	data = memread(thread, &thread->mem_access[slot], 1);

	LOG_MEM_LOAD(vaddr,paddr,width,data,slot);


	if ((slot == 0) && (thread->store_pending[1]) && !(thread->mem_access[1].cancelled)) {
		data = sys_mem_merge_inflight_store(thread,paddr,width,data);
	}

	/* Remember this result in the shadow in case this packet gets restarted due
	   to the other slot
	*/
	thread->shadow_valid[slot] = 1;
	thread->shadow_data[slot] = data;
	if (thread->timing_on ) {
        thread->mem_access[slot].lddata = data;
	}

	return data;
}

// Remove this code, if no one complains that it's missing...
#if 0
#define L1S_PAGE(VA) ((VA & (MAX_L1S_SIZE-1))>>12)
char *get_l1s_hostptr(thread_t * thread, size4u_t vaddr)
{
	char *hostptr = thread->processor_ptr->host_l1s_page_ptr[L1S_PAGE(vaddr)];
	int *stable_page = 0;
	if (hostptr == NULL) {
		hostptr = sim_mem_get_host_ptr(thread->system_ptr, vaddr & 0xfffff000, 1, stable_page);
		if (stable_page) {
			thread->processor_ptr->host_l1s_page_ptr[L1S_PAGE(vaddr)] =	hostptr;
		} else {
			hostptr = NULL;		/* Won't give us something to use :( */
		}
	}
	return (hostptr + (vaddr & 0xfff));
}
#endif




void
mem_general_store(thread_t * thread, size4u_t vaddr, int width, size8u_t data, insn_t *insn)
{
	paddr_t paddr;
    int slot = insn->slot;

	FATAL_REPLAY;

	/* Do address translation to get PA. */
	paddr = mem_init_access(thread, slot, vaddr, width, access_type_store, TYPE_STORE);

    if(EXCEPTION_DETECTED) return;

	if(check_release(thread,vaddr,paddr,data,insn)) {
		thread->mem_access[insn->slot].is_release = 1;
	} else {
		/* Hit BIU exception or tlb miss, so exit*/
		return;
	}

	/* We always commit stores into the store log and move on.
	   Before the next packet can execute, the store log
	   must be empty, which may require multiple stall cycles

	   If there were any exceptions from the above address translation,
	   the exec_step will cancel everything else in the slot including
	   the logging below.

	   All store-side interactions with the D$ take place at or after commit and
	   are called from exec.c
	*/

	if ( (thread->last_pkt->slot_zero_byte_store >> slot) & 0x1) {
		//data = 0;
		SYSVERWARN("store.new source cancelled, converted to store of zeros:vaddr=%x:width=%d:data=%llx\n", vaddr, width, data);
	}

	thread->mem_access[slot].stdata = data;
	thread->store_pending[slot] = 1;

	LOG_MEM_STORE(vaddr,paddr,width,data,slot);

}
#endif

#define FATAL_REPLAY
void
mem_dmalink_store(thread_t * thread, size4u_t vaddr, int width, size8u_t data, int slot)
{
	FATAL_REPLAY;

	mem_access_info_t *maptr = &thread->mem_access[slot];


	maptr->is_memop = 0;
	maptr->log_as_tag = 0;
	maptr->no_deriveumaptr = 0;
	maptr->is_dealloc = 0;
	//maptr->dropped_z = 0;

	//exception_info einfo = {0};

	/* The basic stuff */
	maptr->bad_vaddr = maptr->vaddr = vaddr;
	maptr->width = width;
	maptr->type = access_type_store;
	maptr->tnum = thread->threadId;
    maptr->cancelled = 0;
    maptr->valid = 1;

	/* Attributes of the packet that are needed by the uarch */
    maptr->slot = slot;
#if 0
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->xe = GET_SSR_FIELD(SSR_XE);
    maptr->xa = GET_SSR_FIELD(SSR_XA);

	/* For trace in the uarch */
	maptr->pc_va = thread->Regs[REG_PC];


	// Different here, we're not going to take an exception on dmlink, but the dmwait
	// if this packet has an exception, don't log the store
	if(sys_xlate_dma(thread,vaddr,TYPE_STORE,access_type_store, slot, width-1, &maptr->xlate_info, &einfo)==0) {
		SYSVERWARN("not doing dmlink store due to potential exception");
		if (!thread->processor_ptr->options->testgen_mode) {
			MEMTRACE_ST(thread, thread->Regs[REG_PC], vaddr, 0, width, DWRITE, data);
		}
		return;
	}

	maptr->paddr = maptr->xlate_info.pa;
#else
  maptr->paddr = vaddr; // mgl should be xlate
#endif

	thread->mem_access[slot].stdata = data;
	thread->store_pending[slot] = 1;

	LOG_MEM_STORE(vaddr,maptr->paddr, width, data, slot);

}


#ifdef FIXME

size8s_t mem_load_locked(thread_t * thread, size4u_t vaddr, int width, insn_t *insn)
{
	size8u_t data;
	paddr_t paddr;
	mem_access_info_t *maptr = &thread->mem_access[0];
	//int mmu_on = GET_FIELD(SYSCFG_MMUEN, GLOBAL_REG_READ(REG_SYSCONF));
	//int l1d_on = GET_FIELD(SYSCFG_DCEN, GLOBAL_REG_READ(REG_SYSCONF));
	int slot = 0;

	FATAL_REPLAY;

	/* Do address translation to see if this is cached or UC */
	paddr = mem_init_access(thread, 0, vaddr, width, access_type_load_locked, TYPE_LOAD);

    if (EXCEPTION_DETECTED) return 0;				/* Bail on exception */

	SYSVERWARN("load_locked:begin:vaddr=%x:width=%d\n", vaddr, width);

	thread->lockaddr  = paddr & 0xfffffffffffffff8LL;
	thread->lockvalid = 1;

	/* Cases for system monitor */
	if (maptr->xlate_info.memtype.arch_cacheable) {
		data = memread(thread, &thread->mem_access[slot], 1);
	} else {					/* Uses internal monitor */
		data = memwrap_read_locked(thread, &thread->mem_access[slot]);
	}

	thread->shadow_valid[slot] = 1;
	thread->shadow_data[slot] = data;
	thread->mem_access[slot].llsc = 1;

	LOG_MEM_LOAD(vaddr,paddr,width,data,slot);

	SYSVERWARN("load_locked:lock_paddr=%llx\n", thread->lockaddr);

	return (data);
}

int
mem_store_conditional(thread_t * thread, size4u_t vaddr, size8u_t data, int width, insn_t *insn)
{
	paddr_t paddr;
	mem_access_info_t *maptr = &thread->mem_access[0];
	int retval = -1;
	//int mmu_on = GET_FIELD(SYSCFG_MMUEN, GLOBAL_REG_READ(REG_SYSCONF));
	//int l1d_on = GET_FIELD(SYSCFG_DCEN, GLOBAL_REG_READ(REG_SYSCONF));
	int slot = 0;
	int i;

	FATAL_REPLAY;

	/* Do address translation to see if this is cached or UC */
	paddr = mem_init_access(thread, 0, vaddr, width, access_type_store_conditional, TYPE_STORE);

	if (EXCEPTION_DETECTED) return 0;				/* Bail on exception */

	/* It looks like a normal store to the caches,
	   but has the extra llsc attribute */
	thread->mem_access[0].stdata = data;
	SYSVERWARN("store_conditional:begin:paddr=%llx:vaddr=%x:width=%d current lockaddr=%llx lockvalid=%x:cacheable=%d\n",paddr, vaddr, width,thread->lockaddr,thread->lockvalid,maptr->xlate_info.memtype.arch_cacheable);
	if (((maptr->paddr& 0xfffffffffffffff8LL) != thread->lockaddr) || (!thread->lockvalid)) {
		SYSVERWARN("store_conditional fails internally. paddr=%llx:vaddr=%x",paddr,vaddr);
		return 0;
	}



	if (maptr->xlate_info.memtype.arch_cacheable) {
		memwrite(thread, &thread->mem_access[slot]);
		retval = 1;
	} else {
		retval = memwrap_write_locked(thread, maptr);
	}

	if (retval) {
		/* Clear all that match */
		for (i = 0; i < thread->processor_ptr->runnable_threads_max; i++) {
			if (THREAD_IS_ON(thread->processor_ptr, i)) {
				SYSVERWARN("thread=%d lock_addr=%llx\n",i,thread->processor_ptr->thread[i]->lockaddr);
				if ((thread->processor_ptr->thread[i]->lockaddr == (maptr->paddr& 0xfffffffffffffff8LL))) {
					thread->processor_ptr->thread[i]->lockvalid = 0;
					SYSVERWARN("lock cleared\n");
				}
			}
		}

		LOG_MEM_STORE(vaddr,paddr,width,data,slot);
	}

// HW-Issue 3528
#ifdef VERIFICATION
    if (!maptr->xlate_info.memtype.arch_cacheable) {
		thread->has_ucsc |=  (1<<slot);
	}
#endif

	if(retval) {
		/* Currently, do the store ONLY when the lock is set. */
		/* EJP: I think this is wrong for UC SC ... */
		if(thread->timing_on) {
			thread->mem_access[slot].lockval = retval;
		}
	}

	return (retval);
}

static inline int in_l1s_space_proc(processor_t *proc, paddr_t paddr) {
	int in_l1s = 0;
	paddr_t l1s_lowaddr = proc->options->l2tcm_base + ARCHOPT(l1s_offset0);
	paddr_t l1s_highaddr = proc->options->l2tcm_base + ARCHOPT(l1s_size);
	in_l1s = ((paddr >= l1s_lowaddr) && (paddr < l1s_highaddr));

	if (ARCHOPT(l1s_offset1)) {
		l1s_lowaddr = proc->options->l2tcm_base + ARCHOPT(l1s_offset1);
		l1s_highaddr = proc->options->l2tcm_base + ARCHOPT(l1s_size) + ARCHOPT(l1s_offset1);
		in_l1s |= ((paddr >= l1s_lowaddr) && (paddr < l1s_highaddr));
	}
	return in_l1s;
}

static inline int in_tcm_space_proc(processor_t *proc, paddr_t paddr) {

	paddr_t tcm_lowaddr = ARCHOPT(tcm_lowaddr);
	paddr_t tcm_highaddr = tcm_lowaddr + arch_get_tcm_size(proc) - 1;
	int in_tcm = ((paddr >= tcm_lowaddr) && (paddr < tcm_highaddr));
	return in_tcm;
}

static inline int in_ahb_space_proc(processor_t *proc, paddr_t paddr) {

	paddr_t ahb_lowaddr = ARCHOPT(ahb_lowaddr);
	paddr_t ahb_highaddr = ARCHOPT(ahb_highaddr);
	int in_ahb = ((paddr >= ahb_lowaddr) && (paddr < ahb_highaddr));
	return in_ahb;
}

static inline int in_axi2_space_proc(processor_t *proc, paddr_t paddr) {

	paddr_t axi2_lowaddr = ARCHOPT(axi2_lowaddr);
	paddr_t axi2_highaddr = ARCHOPT(axi2_highaddr);
	int in_axi2 = ((paddr >= axi2_lowaddr) && (paddr < axi2_highaddr));
	return in_axi2;
}

static inline int in_axi_space_proc(processor_t *proc, paddr_t paddr) {

	paddr_t axi_lowaddr = ARCHOPT(axi_lowaddr);
	paddr_t axi_highaddr = ARCHOPT(axi_highaddr);
	int in_axi = ((paddr >= axi_lowaddr) && (paddr < axi_highaddr));
	return in_axi && !(in_l1s_space_proc(proc, paddr)
										 || in_tcm_space_proc(proc, paddr)
										 || in_ahb_space_proc(proc, paddr)
										 || in_axi2_space_proc(proc, paddr)
										 || in_vtcm_space_proc(proc, paddr));
}



void mem_vtcm_memcpy(thread_t *thread, insn_t *insn, vaddr_t dst, vaddr_t src, size4u_t length) {

	processor_t *proc = thread->processor_ptr;

	mem_access_info_t *src_maptr;
	mem_access_info_t *dst_maptr;
	paddr_t src_save;
	paddr_t dst_save;

	vaddr_t align_bytes = ~( ARCHOPT(l2cache_fillsize) - 1);

	vaddr_t src_aligned;
	vaddr_t dst_aligned;

	vaddr_t src_page_mask;
	vaddr_t dst_page_mask;

	// Save VA's for ETM callback, uses full VA
	vaddr_t src_etm = src;
	vaddr_t dst_etm = dst;

	size4u_t bytes = length;
	size4u_t bytes_save;
	int src_slot = 1;
	int dst_slot = 0;
	int width = 1;

	int src_exception = 0;
	int dst_exception = 0;
	int src_tlbmiss = 0;
	int dst_tlbmiss = 0;

	src_aligned = src & align_bytes;
	dst_aligned = dst & align_bytes;
	bytes_save = bytes = (bytes | ~align_bytes) + 1;

	//printf("memcpy bytes=%d\n", bytes);

	memset(&thread->einfo, 0, sizeof(thread->einfo));

	/* Slot 1 */
	mem_init_access_unaligned(thread, src_slot, src, src_aligned, width, access_type_memcpy_load, TYPE_LOAD);
	if (EXCEPTION_DETECTED) {
		src_exception = 1;
		/* FIXME: these should have accessors */
		src_tlbmiss = (TLBMISSRW_CAUSE_READ == thread->einfo.cause ? 1 : 0);
		thread->einfo.badva0 = dst;  // hack alert. RTL expects badva0 to have dst on slot 1 exception
	}

	src_maptr = &thread->mem_access[src_slot];
	src_page_mask = (~0) << src_maptr->xlate_info.size;
	src_maptr->stride_in_bytes = length;

	if (!src_tlbmiss) {
		if (!in_axi_space_proc(proc, src_maptr->paddr) ||
				!in_axi_space_proc(proc, src_maptr->paddr + length) ||          // src not in AXI?
				(((src + length) & src_page_mask) != (src & src_page_mask)) ||  // page cross
				src_maptr->xlate_info.memtype.device                            // device type
				) {
			SYSVERWARN("memcpy exception on src addr pa=%llx, base in axi=%d, base+length=%d, page_cross=%d, device_type=%d\n",
								 src_maptr->paddr, in_axi_space_proc(proc, src_maptr->paddr),
								 in_axi_space_proc(proc, src_maptr->paddr + length),
								 (((src + length) & src_page_mask) != (src & src_page_mask)), src_maptr->xlate_info.memtype.device);

			register_error_exception(thread, PRECISE_CAUSE_COPROC_LDST,
															 dst /*badva0*/,  src /*badva1*/,
															 1 /* bvs */, 1 /* v0 */, 1 /* v1 */,
															 0x2 /* slotmask */);
			src_exception = 1;
		}
	}

	/* Slot 0 */
	mem_init_access_unaligned(thread, dst_slot, dst, dst_aligned, width, access_type_memcpy_store, TYPE_STORE);
	if (thread->einfo.bv0) {  // This is ugly but EXCEPTION_DETECTED might have already been set before the
		                        // second mem_init_access_unaligned(). Need to know if slot 0 excepted.
		dst_exception = 1;
		/* FIXME: these should have accessors */
		dst_tlbmiss = (TLBMISSRW_CAUSE_WRITE == thread->einfo.cause ? 1 : 0);
		thread->einfo.badva1 = src;  // hack alert. RTL expects badva1 to have src on slot 0 exception
	}

	dst_maptr = &thread->mem_access[dst_slot];
	dst_page_mask = (~0) << dst_maptr->xlate_info.size;
	dst_maptr->stride_in_bytes = length;

	if (!dst_tlbmiss) {
		if (!in_vtcm_space_proc(proc, dst_maptr->paddr) ||
				!in_vtcm_space_proc(proc, dst_maptr->paddr + length) ||         // dst not in VTCM?
				(((dst + length) & dst_page_mask) != (dst & dst_page_mask)) ||  // page cross
				dst_maptr->xlate_info.memtype.device                            // device type
				) {
			SYSVERWARN("memcpy exception on dst addr pa=%llx, base in axi=%d, base+length=%d, page_cross=%d, device_type=%d\n",
								 dst_maptr->paddr, in_vtcm_space_proc(proc, dst_maptr->paddr),
								 in_vtcm_space_proc(proc, dst_maptr->paddr + length),
								 (((dst + length) & dst_page_mask) != (dst & dst_page_mask)), dst_maptr->xlate_info.memtype.device);

			register_error_exception(thread, PRECISE_CAUSE_COPROC_LDST,
															 dst /*badva0*/,  src /*badva1*/,
															 0 /* bvs */, 1 /* v0 */, 1 /* v1 */,
															 0x1 /* slotmask */);
			dst_exception = 1;
		}
	}

	if (src_exception || dst_exception) {
		return;
	}

	thread->last_pkt->pkt_has_vtcm_access = 1;

	//ETM
	LOG_MEM_LOAD(src_etm,src_maptr->paddr,0,0,src_slot);
	LOG_MEM_STORE(dst_etm,dst_maptr->paddr,0,0,dst_slot);


	src_save = src_maptr->paddr;
	dst_save = dst_maptr->paddr;

	warn("memcpy transfer from %llx to %llx total size=%d align_bytes=%d" , src_maptr->paddr, dst_maptr->paddr, bytes, align_bytes );

	while (bytes) {
		if (8 <= bytes) {
			width = 8;
		} else if (4 <= bytes) {
			width = 4;
		} else if (2 <= bytes) {
			width = 2;
		} else {
			width = 1;
		}

		src_maptr->width = width;
		dst_maptr->width = width;

		dst_maptr->stdata = memread(thread, src_maptr, 1);
		memwrite(thread, dst_maptr);
		SYSVERWARN("memcpy transfer from %llx to %llx width=%d data=%llx" , src_maptr->paddr, dst_maptr->paddr, width, dst_maptr->stdata );
		src_maptr->paddr += width;
		dst_maptr->paddr += width;
		bytes -= width;
	}

	/* verlib, etc need this to point to the beginning of buffer */
	src_maptr->paddr = src_save;
	dst_maptr->paddr = dst_save;
	src_maptr->width = 1;
	dst_maptr->width = 1;

#ifdef VERIFICATION
	if (thread->processor_ptr->options->testgen_mode) {
		MEMTRACE_LD(thread, thread->Regs[REG_PC], src_aligned, src_maptr->paddr, bytes_save, DREAD, 0xfeedfacedeadbeefULL);
		MEMTRACE_ST(thread, thread->Regs[REG_PC], dst_aligned, dst_maptr->paddr, bytes_save, DWRITE, 0x0);
	}
#endif
}

void sys_dcinva(thread_t * thread, size4u_t vaddr)
{

	FATAL_REPLAY;
	fatal("should never get here, no more dcinva");

    mem_init_access(thread, 0, vaddr, 1, access_type_dcinva, TYPE_STORE);

    if (EXCEPTION_DETECTED) return;

    mem_access_info_t *maptr = &thread->mem_access[0];

	maptr->index = -1;
	maptr->way = -1;
	maptr->op = INVALIDATE;
	maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;

}

static inline int sys_dcaddrop_get_type(thread_t *thread, size4u_t vaddr, int slot)
{
	exception_info read_einfo;
	exception_info write_einfo;
	xlate_info_t xinfo;
	int read_ret;
	int write_ret;
	if ((read_ret = sys_xlate(thread,vaddr,TYPE_LOAD,access_type_load,slot,0,&xinfo,&read_einfo)) != 0) {
		return TYPE_LOAD;
	}
	if ((write_ret = sys_xlate(thread,vaddr,TYPE_STORE,access_type_store,slot,0,&xinfo,&write_einfo)) != 0) {
		return TYPE_STORE;
	}
	/* OK, neither read nor writes will succeed.  But we might have R or W permission but not U... */
	if ((read_einfo.cause == PRECISE_CAUSE_PRIV_NO_READ)
		&& (write_einfo.cause == PRECISE_CAUSE_PRIV_NO_UWRITE)) return TYPE_STORE;
	if ((read_einfo.cause == PRECISE_CAUSE_PRIV_NO_UREAD)
		&& (write_einfo.cause == PRECISE_CAUSE_PRIV_NO_WRITE)) return TYPE_LOAD;
	/* They're both going to fail, so prefer the LOAD exception */
	return TYPE_LOAD;
}

static inline void sys_dcaddrop_fixup_type(thread_t *thread)
{
	/* EJP: what an ugly kludge.  But when you have "read OR write permission is OK" and the
	 * spec says "use the read permission" and you have our RWX and U permissions you get
	 * something like this sometimes...
	 * In practice I think this result is OK.  Just really strange to get a UREAD cause when no R.
	 */
	if (thread->einfo.cause == PRECISE_CAUSE_PRIV_NO_UWRITE) {
		thread->einfo.cause = PRECISE_CAUSE_PRIV_NO_UREAD;
	}
}

void sys_dccleana(thread_t * thread, size4u_t vaddr){

	int access_type;
	FATAL_REPLAY;
	access_type = sys_dcaddrop_get_type(thread,vaddr,0);

    mem_init_access(thread, 0, vaddr, 1, access_type_dccleana, access_type);

    if (EXCEPTION_DETECTED) sys_dcaddrop_fixup_type(thread);
	if (EXCEPTION_DETECTED) return;

    mem_access_info_t *maptr = &thread->mem_access[0];

	maptr->index = -1;
	maptr->way = -1;
	maptr->op = CLEAN;
	maptr->bp = GET_SSR_FIELD(SSR_BP);
	maptr->valid = 1;
    if (!thread->bq_on) {
        //MEMTRACE_LD(thread, thread->Regs[REG_PC], vaddr, maptr->paddr, 0, DREAD, 0);

		LOG_MEM_LOAD(vaddr,maptr->paddr,0,0,0);

    }

}

void sys_dccleaninva(thread_t * thread, size4u_t vaddr, int slot)
{
	int access_type;
	FATAL_REPLAY;
	access_type = sys_dcaddrop_get_type(thread,vaddr,0);

    mem_init_access(thread, 0, vaddr, 1, access_type_dccleaninva, access_type);



    if (EXCEPTION_DETECTED) sys_dcaddrop_fixup_type(thread);
	if (EXCEPTION_DETECTED) return;

    mem_access_info_t *maptr = &thread->mem_access[0];


	maptr->index = -1;
	maptr->way = -1;
	maptr->op = CLEANINVALIDATE;
	maptr->bp = GET_SSR_FIELD(SSR_BP);
	maptr->valid = 1;

    if (!thread->bq_on) {
       // MEMTRACE_LD(thread, thread->Regs[REG_PC], vaddr, maptr->paddr, 0, DREAD, 0);

	   LOG_MEM_LOAD(vaddr,maptr->paddr,0,0,0);

    }

}

void sys_dczeroa(thread_t * thread, size4u_t vaddr)
{
	paddr_t paddr;
	mem_access_info_t *maptr = &thread->mem_access[0];

	FATAL_REPLAY;

	/* Do address translation to get PA. It is always safe to go ahead
	   and do this. If a previous insn had a problem, exec_step will have
	   aborted early and we wouldn't get here.  */
	paddr = mem_init_access(thread, 0, vaddr, 32, access_type_dczeroa, TYPE_STORE);

	if (EXCEPTION_DETECTED) return;					/* Bail on exception */

	maptr->stdata = 0;
	thread->store_pending[0] = 1;	// slot0only
	LOG_MEM_STORE(vaddr,paddr,32,0,0);
}



void sys_dccleanidx(thread_t * thread, size4u_t data)
{
  //	processor_t *proc = thread->processor_ptr;
    int slot = 0;
	mem_access_info_t *maptr = &thread->mem_access[slot];

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
	maptr->vaddr = data;
    maptr->index = (data >> 5);
    maptr->way = (data & 0x1f);
    maptr->op = CLEAN;
    maptr->type = access_type_dccleanidx;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->tnum = thread->threadId;
    maptr->slot = slot;
    maptr->valid = 1;

}


void sys_dccleaninvidx(thread_t * thread, size4u_t data)
{
    int slot = 0;
	mem_access_info_t *maptr = &thread->mem_access[slot];

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
	maptr->vaddr = data;
    maptr->index = (data >> 5);
    maptr->way = (data & 0x1f);
    maptr->op = CLEANINVALIDATE;
    maptr->type = access_type_dccleaninvidx;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->tnum = thread->threadId;
    maptr->slot = slot;
    maptr->valid = 1;

}


void sys_l2cleaninvidx(thread_t * thread, size4u_t data)
{
	int slot = 0;
	mem_access_info_t *maptr = &thread->mem_access[slot];

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
	maptr->vaddr = data;
    maptr->slot = slot;
    maptr->type = access_type_l2cleaninvidx;
    maptr->index = data >> 8;
    maptr->way = data & 0x7; //[0-2]
    maptr->granule = (data & 0x8) >> 3; //[3]
    maptr->width = 0;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->tnum = thread->threadId;
    maptr->valid = 1;

}

void sys_l2cleanidx(thread_t * thread, size4u_t data)
{
	int slot = 0;
    mem_access_info_t *maptr = &thread->mem_access[slot];

	FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
	maptr->vaddr = data;
    maptr->type = access_type_l2cleanidx;
    maptr->index = data >> 8;
    maptr->way = data & 0x7; //[0-2]
    maptr->granule = (data & 0x8) >> 3; //[3]
    maptr->width = 0;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->tnum = thread->threadId;
    maptr->slot = slot;
    maptr->valid = 1;

}

void sys_l2invidx(thread_t * thread, size4u_t data)
{
	thread->mem_access[0].vaddr = data;
	sys_l2cleaninvidx(thread, data);
}


void sys_l2gcleaninv(thread_t * thread)
{
    int slot = 0;
    mem_access_info_t *maptr = &thread->mem_access[slot];

	FATAL_REPLAY;

    memset(&thread->mem_access[slot], 0, sizeof(thread->mem_access[slot]));
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->type = access_type_l2gcleaninv;
    maptr->width = 0;
    maptr->slot = slot;
    maptr->tnum = thread->threadId;
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
	maptr->log_as_tag = 1;
	maptr->sys_reg_update_callback = sys_l2_global_tagop_update_state;

    return;
}

void sys_l2gclean(thread_t * thread)
{
    int slot = 0;
    mem_access_info_t *maptr = &thread->mem_access[slot];

	FATAL_REPLAY;
    memset(maptr, 0, sizeof(mem_access_info_t));

    maptr->pc_va = thread->Regs[REG_PC];
    maptr->type = access_type_l2gclean;
    maptr->width = 0;
    maptr->slot = slot;
    maptr->tnum = thread->threadId;
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
	maptr->log_as_tag = 1;
	maptr->sys_reg_update_callback = sys_l2_global_tagop_update_state;

    return;
}



void sys_l2gcleaninv_pa(thread_t * thread, size8u_t data)
{
#if 0
    int slot = 0;

	FATAL_REPLAY;


	if (thread->timing_on) {
		memset(&thread->mem_access[slot], 0, sizeof(struct mem_access_info));
		thread->mem_access[slot].pc_va = thread->Regs[REG_PC];
		thread->mem_access[slot].type = access_type_l2gcleaninv;
		thread->mem_access[slot].width = 0;
        thread->mem_access[slot].slot = slot;
        thread->mem_access[slot].tnum = thread->threadId;
		thread->processor_ptr->uarch.uarch_th_be_log_tag_access_ptr(thread->processor_ptr, thread->threadId, &thread->mem_access[slot], sys_l2_global_tagop_update_state, 1);
	}
#endif

    return;
}

void sys_l2gclean_pa(thread_t * thread, size8u_t data)
{
#if 0
    int slot = 0;

	FATAL_REPLAY;


	if (thread->timing_on) {
		memset(&thread->mem_access[slot], 0, sizeof(struct mem_access_info));
		thread->mem_access[slot].pc_va = thread->Regs[REG_PC];
		thread->mem_access[slot].type = access_type_l2gclean;
		thread->mem_access[slot].width = 0;
        thread->mem_access[slot].slot = slot;
        thread->mem_access[slot].tnum = thread->threadId;
		thread->processor_ptr->uarch.uarch_th_be_log_tag_access_ptr(thread->processor_ptr, thread->threadId, &thread->mem_access[slot], sys_l2_global_tagop_update_state, 1);
	}
#endif

    return;
}


void
sys_update_gpr(processor_t *proc, int tnum, int type, int reg, int field, int val)
{
    thread_t *thread = proc->thread[tnum];
    thread->Regs[reg] = val;
}

void
sys_update_preg(processor_t *proc, int tnum, int type, int reg, int field, int val)
{
    thread_t *thread = proc->thread[tnum];
    size1u_t *pptr;

    pptr = (size1u_t *) & (thread->Regs[REG_PQ]);
    pptr[reg] = val & 0xff;
}

size4u_t
sys_l2locka(thread_t * thread, vaddr_t vaddr, int slot, int pregno)
{
	paddr_t paddr;
	size4u_t data = 0xff;

	FATAL_REPLAY;

	/* Do a va to pa translation, and check for exceptions. */
	paddr = mem_init_access(thread, slot, vaddr, 1, access_type_l2locka, TYPE_STORE);
	/* If exception, record slot done, and return 0 */
	if (EXCEPTION_DETECTED) return 0;


    thread->mem_access[slot].regno = pregno;
	thread->mem_access[slot].log_as_tag = 1;
	thread->mem_access[slot].sys_reg_update_callback = sys_l2_global_tagop_update_state;

#ifdef VERIFICATION
	if (!thread->bq_on) {
		warn("l2locka using hint: %d",thread->l2locka_hint_val);
		data = thread->shadow_data[slot] = thread->l2locka_hint_val;
		// HW-Issue 3528
		thread->has_l2locka |= (1<<slot);
	}
#endif

	data = (data == 0) ? 0 : 0xff;
	return data;
}


void
sys_l2unlocka(thread_t * thread, vaddr_t vaddr, int slot)
{
	paddr_t paddr;

    FATAL_REPLAY;

	paddr = mem_init_access(thread,slot,vaddr,1,access_type_l2unlocka,TYPE_STORE);
	if (EXCEPTION_DETECTED) return;

}

void sys_l2gunlock(thread_t * thread)
{
	int slot = 0;
    mem_access_info_t *maptr = &thread->mem_access[slot];

	FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->type = access_type_l2gunlock;
    maptr->slot = slot;
    maptr->tnum = thread->threadId;
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
	maptr->log_as_tag = 1;
	maptr->sys_reg_update_callback = sys_l2_global_tagop_update_state;
	maptr->no_deriveumaptr = 1;
}


void sys_dcinvidx(thread_t * thread, size4u_t data)
{
    int slot = 0;
	mem_access_info_t *maptr = &thread->mem_access[slot];

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
	maptr->vaddr = data;
    maptr->index = (data >> 5);
    maptr->way = (data & 0x1f);
    maptr->op = INVALIDATE;
    maptr->type = access_type_dcinvidx;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->tnum = thread->threadId;
    maptr->valid = 1;

}

void sys_dcfetch(thread_t *thread, size4u_t vaddr, int slot)
{
	paddr_t paddr;
    processor_t * proc = thread->processor_ptr;

	if (proc->arch_proc_options->dcfetch_disable) {
		return;
	}

	/* If the CCR[SDF]=0, turn it into NOP */
	if (GET_FIELD(CCR_SFD, READ_RREG(REG_CCR)) == 0) {
		return;
	}

	FATAL_REPLAY;

	/* Means, this instruction has already been executed */
	if (thread->shadow_valid[slot]) {
		return;
	}

	/* Do address translation and bail if we are not able to translate */
	paddr = mem_init_access_silent(thread, slot, vaddr, 1, access_type_dcfetch, TYPE_LOAD, 0);

#ifdef VERIFICATION
	thread->mem_access[slot].width = 0;
    thread->mem_access[slot].pc_va = thread->Regs[REG_PC];
	thread->mem_access[slot].bp = GET_SSR_FIELD(SSR_BP);
	MEMTRACE_LD(thread, thread->Regs[REG_PC], vaddr, paddr, 0, DPREFETCH, 0);
	//LOG_MEM_LOAD(vaddr,paddr,32,0,0);
#endif


	/* Remember this is done in the shadow in case this packet gets restarted due to another slot */
	thread->shadow_valid[slot] = 1;
    if (!thread->bq_on) {
        INC_TSTAT(tpkt_dcfetch);
    }
}

void
sys_l2prefetch_update_state(processor_t * proc, int tnum, int reg,
							int field, int val)
{
	thread_t *thread = proc->thread[tnum];
	fWRITE_REG_FIELD(USR, USR_PFA, (val & 0x1));
	SET_USR_FIELD(USR_PFA, (val & 0x1));
	return;
}

void
sys_l2fetch(thread_t *thread, size4u_t vaddr,
			size4u_t height, size4u_t width, size4u_t stride_in_bytes,
			size4u_t flags, int slot)
{
    mem_access_info_t *maptr = &thread->mem_access[slot];
	FATAL_REPLAY;
	/* Means, this instruction has already been executed */
	if (thread->shadow_valid[slot]) {
		return;
	}

	memset(maptr, 0, sizeof(mem_access_info_t));

	/* If the CCR[SDF]=0, turn it into NOP */
	if (GET_FIELD(CCR_SFD, READ_RREG(REG_CCR)) == 0) {
		return;
	}

	paddr_t paddr = mem_init_access(thread, 0, vaddr & 0xffffffff, 1, access_type_l2fetch, TYPE_LOAD);

	if (EXCEPTION_DETECTED) return;

	if (thread->processor_ptr->arch_proc_options->l2fetch_disable) {
		return;
	}


    maptr->pc_va = thread->Regs[REG_PC];
	maptr->paddr = paddr;
    maptr->vaddr = vaddr;
    maptr->width = width;
    maptr->height = height;
    maptr->stride_in_bytes = stride_in_bytes;
    maptr->flags = flags;
    maptr->valid = 1;

	if (thread->timing_on) {
		warn("L2fetch, width=%d, height=%d, stride_in_bytes=%d", width, height, stride_in_bytes);
	}
	thread->shadow_valid[slot] = 1;
	if((width != 0) && (height != 0) &&  (!thread->bq_on)) {
		INC_TSTAT(tpkt_l2fetch);
	}

	return;
}

void sys_icinva(thread_t * thread, size4u_t vaddr, int slot)
{
	xlate_info_t xinfo;
	exception_info einfo;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

    FATAL_REPLAY;

#ifdef VERIFICATION
	ver_pick_itlb(thread->processor_ptr, thread->threadId, thread->Regs[REG_PC], vaddr, 1);
#endif

    if(sys_xlate(thread,vaddr,TYPE_ICINVA,access_type_icinva,slot,0,&xinfo,&einfo) == 0) {
		register_einfo(thread,&einfo);
	}

	if (EXCEPTION_DETECTED) {
		return;
	}

    maptr->paddr = xinfo.pa;
    maptr->bad_vaddr = maptr->vaddr = vaddr;
    maptr->type = access_type_icinva;
    maptr->tnum = thread->threadId;
    maptr->cancelled = 0;
    maptr->slot = slot;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->op = INVALIDATE;
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;


	thread->processor_ptr->icinva_backoff = 16; // Not too big, not too small
	return;
}

size8u_t sys_cfgtbl_read(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width) {

	switch(width) {
	case 1:
		return memmap_cluster_read1(proc, memmapca_id_to_memmap_cluster(proc, proc->cfg_table_memmap_id), paddr);
	case 2:
		return memmap_cluster_read2(proc, memmapca_id_to_memmap_cluster(proc, proc->cfg_table_memmap_id), paddr);
	case 4:
		return memmap_cluster_read4(proc, memmapca_id_to_memmap_cluster(proc, proc->cfg_table_memmap_id), paddr);
	case 8:
		return memmap_cluster_read8(proc, memmapca_id_to_memmap_cluster(proc, proc->cfg_table_memmap_id), paddr);
	default:
		assert(0);
	}

	return 0;
}


void sys_cfgtbl_write(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width, size8u_t val) {

	pwarn(proc, "Write to cfg_table ignored");
}

void sys_cfgtbl_init(processor_t *proc) {
	memmap_cluster_t *mc = memmapca_id_to_memmap_cluster(proc, proc->cfg_table_memmap_id);

	assert (NULL != mc);
	#define DEF_CFGTBL_ENTRY(NAME, OFFSET, DESCR, BEH) \
		*((size4u_t *)(mc->data + OFFSET)) = BEH;
	#include "cfgtable.def"
	#undef DEF_CFGTBL_ENTRY

}


void sys_l2cfg_init(processor_t *proc) {
	memmap_cluster_t *mc = memmapca_id_to_memmap_cluster(proc, proc->l2cfg_memmap_id);
	size4u_t val;

	assert (NULL != mc);

	// Syndrome1-4
	*((size4u_t *)(mc->data + 0x0)) = 0;
	*((size4u_t *)(mc->data + 0x4)) = 0;
	*((size4u_t *)(mc->data + 0x8)) = 0;
	*((size4u_t *)(mc->data + 0xc)) = 0;
	// Bus Latency Counting
	*((size4u_t *)(mc->data + 0x10)) = 0;
	*((size4u_t *)(mc->data + 0x14)) = 0;
	*((size4u_t *)(mc->data + 0x18)) = 0;
	*((size4u_t *)(mc->data + 0x1c)) = 0;
	*((size4u_t *)(mc->data + 0x20)) = 0;
	*((size4u_t *)(mc->data + 0x24)) = 0;
	// Coproc Energy
	*((size4u_t *)(mc->data + 0x28)) = 0;
	*((size4u_t *)(mc->data + 0x2c)) = 0;
	// SFC Control
	*((size4u_t *)(mc->data + 0x30)) = 0;
	// Event Register Low/High
	*((size4u_t *)(mc->data + 0x34)) = 0;
	*((size4u_t *)(mc->data + 0x38)) = 0;
	//ASCid mapping
	*((size4u_t *)(mc->data + 0x40)) = 0;
	*((size4u_t *)(mc->data + 0x44)) = 0;
	*((size4u_t *)(mc->data + 0x48)) = 0;
	*((size4u_t *)(mc->data + 0x4c)) = 0;
	// CCCC
	*((size4u_t *)(mc->data + 0x50)) = 0;
	*((size4u_t *)(mc->data + 0x54)) = 0;
	*((size4u_t *)(mc->data + 0x58)) = 0;
	*((size4u_t *)(mc->data + 0x5c)) = 0;

	// QOS MODE
	val  = (proc->arch_proc_options->qos_mode_normal_mux & 0x3) << 0;
	val |= (proc->arch_proc_options->qos_mode_danger_mux & 0x3) << 2;
	val |= (proc->arch_proc_options->qos_mode_extreme_danger_mux & 0x3) << 4;
	val |= (proc->arch_proc_options->qos_normal_lo_watermark > 0 ) << 7;
	val |= (proc->arch_proc_options->qos_hi_watermark > 0 ) << 8;
	val |= (proc->arch_proc_options->qos_l2fetch_watermark > 0 ) << 9;
	val |= (proc->arch_proc_options->qos_extreme_danger_issue_rate > 0 ) << 10;
	val |= (proc->arch_proc_options->qos_danger_lo_watermark > 0 ) << 11;
	val |= (proc->arch_proc_options->qos_danger_l2fetch_stall & 0x1 ) << 12;
	val |= (proc->arch_proc_options->qos_stall_l2fetch_on_bus_conditions & 0x1) << 15;
	val |= (proc->arch_proc_options->qos_max_rd > 0) << 17;
	*((size4u_t *)(mc->data + 0x100)) = val;

	// QOS MAX TRANS
	val  = (proc->arch_proc_options->qos_max_rd 	& 0xFF) << 0;
	val |= (proc->arch_proc_options->qos_max_lo_rd	& 0xFF) << 8;
	val |= (proc->arch_proc_options->qos_max_wr 	& 0xFF) << 16;
	val |= (proc->arch_proc_options->qos_max_lo_wr 	& 0xFF) << 24;
	*((size4u_t *)(mc->data + 0x104)) = val;

	// QOS_ISSUE_RATE
	val  = (proc->arch_proc_options->qos_rd_issue_rate 		 & 0xFF) << 0;
	val |= (proc->arch_proc_options->qos_wr_issue_rate		 & 0xFF) << 8;
	val |= (proc->arch_proc_options->qos_danger_lo_watermark & 0xFF) << 16;
	*((size4u_t *)(mc->data + 0x108)) = val;

	// QOS DANGER ISSUE RATE
	val  = (proc->arch_proc_options->qos_danger_issue_rate 		   & 0xFF) << 0;
	val |= (proc->arch_proc_options->qos_extreme_danger_issue_rate & 0xFF) << 8;
	*((size4u_t *)(mc->data + 0x10c)) = val;

	// QOS_SCOREBOARD_WATERMARK
	val  = (proc->arch_proc_options->qos_l2fetch_watermark 		 & 0xFF) << 0;
	size4u_t watermark = (proc->arch_proc_options->qos_danger_lo_watermark > 0 ) ? proc->arch_proc_options->qos_danger_lo_watermark : proc->arch_proc_options->qos_normal_lo_watermark;
	val |= ( watermark & 0xFF) << 8;
	val |= (proc->arch_proc_options->qos_hi_watermark & 0xFF) << 16;
	*((size4u_t *)(mc->data + 0x110)) = val;

	// QOS_SYS_PRI
	val  = (proc->arch_proc_options->qos_areq_lo & 0x7) << 0;
	val |= (proc->arch_proc_options->qos_areq_hi & 0x7) << 16;
	*((size4u_t *)(mc->data + 0x114)) = val;

	// QOS_SLAVE
	*((size4u_t *)(mc->data + 0x118)) = 0;

	// AQOS_CNRL
	val   = (!proc->arch_proc_options->aqos_enable 		 & 0x1) << 0;
	val  |= (proc->arch_proc_options->aqos_mode 		 & 0x1) << 1;
	val  |= (proc->arch_proc_options->aqos_sb_full_count & 0x7) << 8;
	val  |= (proc->arch_proc_options->aqos_time_period 	 & 0x7) << 16;
	*((size4u_t *)(mc->data + 0x11c)) = val;


	// Distributed Syndrome Registers
	*((size4u_t *)(mc->data + 0x200)) = 0;
	*((size4u_t *)(mc->data + 0x300)) = 0;
	*((size4u_t *)(mc->data + 0x304)) = 0;
	*((size4u_t *)(mc->data + 0x308)) = 0;
	*((size4u_t *)(mc->data + 0x30C)) = 0;
	*((size4u_t *)(mc->data + 0x310)) = 0;
	*((size4u_t *)(mc->data + 0x314)) = 0;
	*((size4u_t *)(mc->data + 0x318)) = 0;
	*((size4u_t *)(mc->data + 0x31C)) = 0;
	*((size4u_t *)(mc->data + 0x320)) = 0;
	*((size4u_t *)(mc->data + 0x400)) = 0;
	*((size4u_t *)(mc->data + 0x404)) = 0;
	*((size4u_t *)(mc->data + 0x408)) = 0;
	*((size4u_t *)(mc->data + 0x40C)) = 0;
	*((size4u_t *)(mc->data + 0x410)) = 0;
	*((size4u_t *)(mc->data + 0x414)) = 0;
	*((size4u_t *)(mc->data + 0x500)) = 0;
	*((size4u_t *)(mc->data + 0x504)) = 0;
	*((size4u_t *)(mc->data + 0x508)) = 0;
	*((size4u_t *)(mc->data + 0x50C)) = 0;
	*((size4u_t *)(mc->data + 0x510)) = 0;
	*((size4u_t *)(mc->data + 0x514)) = 0;

	// QOS Danger
	*((size4u_t *)(mc->data + 0x1000)) = 0;
	*((size4u_t *)(mc->data + 0x2000)) = 0;
	*((size4u_t *)(mc->data + 0x3000)) = 0;
	*((size4u_t *)(mc->data + 0x4000)) = 0;


}

size8u_t sys_l2cfg_read(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width) {

	size8u_t val = 0;
	switch(width) {
	case 1:
		val = memmap_cluster_read1(proc, memmapca_id_to_memmap_cluster(proc, proc->l2cfg_memmap_id), paddr);
		break;
	case 2:
		val = memmap_cluster_read2(proc, memmapca_id_to_memmap_cluster(proc, proc->l2cfg_memmap_id), paddr);
		break;
	case 4:
		val = memmap_cluster_read4(proc, memmapca_id_to_memmap_cluster(proc, proc->l2cfg_memmap_id), paddr);
		break;
	case 8:
		val = memmap_cluster_read8(proc, memmapca_id_to_memmap_cluster(proc, proc->l2cfg_memmap_id), paddr);
		break;
	default:
		assert(0);
	}
	//printf("read l2cfg paddr: %llx Val=%llx\n", paddr, val);
	pwarn(proc, "Reading L2 CFG Register PA=%llx Val=%llx", paddr, val);
	return val;
}



/* Initialize 32-registers for ECC for a memory */
void sys_ecc_block_init(processor_t *proc, int mem_offset, size4u_t granule_size, paddr_t pa_base, int num_regions) {
	memmap_cluster_t *mc = memmapca_id_to_memmap_cluster(proc, proc->ecc_memmap_id);
	size4u_t val;

	assert (NULL != mc);

	// Clear all First
	for (int i = 0; i < 36*4; i+=4) {
		*((size4u_t *)(mc->data + mem_offset + i)) = 0;
	}

	val = 0x5 << 0;		// Protection Enable
	val |= 0x5 << 8;	// Writeback Enable
	val |= 0x5 << 12;	// Interrupt Enable
	*((size4u_t *)(mc->data + mem_offset + 0)) = val;


	val = 0x5 << 0;		// Corrected Interrupt Enable
	*((size4u_t *)(mc->data + mem_offset + 10*4)) = val;


	val = 0x5 << 0;		// Clear Region Logging
	*((size4u_t *)(mc->data + mem_offset + 12*4)) = val;

	for(int i = 0; i < num_regions; i++) {
		// TODO: Figure out correct base mapping
		val = 0; 	// BASE
		*((size4u_t *)(mc->data + mem_offset + (16 + i)*4)) = val;
		val = 0x1 << 9;		// Error Type: 1-corrected 2-uncorrected
		*((size4u_t *)(mc->data + mem_offset + (24 + i)*4)) = val;
	}

}

void sys_ecc_reg_init(processor_t *proc) {
	sys_ecc_block_init(proc, ECC_IUDATA_REGS_START, proc->features->QDSP6_IUDATA_ERR_GRANULE_BITS, 0, 1);
	sys_ecc_block_init(proc, ECC_DUDATA_REGS_START, proc->features->QDSP6_DUDATA_ERR_GRANULE_BITS, 0, 1);
	sys_ecc_block_init(proc, ECC_L2DATA_REGS_START, proc->features->QDSP6_L2DATA_ERR_GRANULE_BITS, 0, 1);
	sys_ecc_block_init(proc, ECC_VTCM_REGS_START,   proc->features->QDSP6_VTCM_ERR_GRANULE_BITS,   0, 8);
	sys_ecc_block_init(proc, ECC_L2TAG_REGS_START,  proc->features->QDSP6_L2TAG_ERR_GRANULE_BITS,  0, 1);
}
void sys_ecc_reg_write(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width, size8u_t val) {
	memmap_cluster_t *mc = memmapca_id_to_memmap_cluster(proc, proc->ecc_memmap_id);
	assert(mc != NULL);
	if (width == 4) {
		memmap_cluster_write4(proc, mc, paddr, val);
	} else {
		pwarn(proc, "Write to ecc memory mapped registers only support word sizes");
	}
}
size8u_t sys_ecc_reg_read(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width) {

	size8u_t val = 0;
	if(width == 4) {
		val = memmap_cluster_read4(proc, memmapca_id_to_memmap_cluster(proc, proc->ecc_memmap_id), paddr);
		pwarn(proc, "Reading ECC Register PA=%llx Val=%llx", paddr, val);
		return val;
	}
	pwarn(proc, "Non-word sized access to memory mapped reg is undefined.");
	return val;
}

void sys_l2cfg_write(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width, size8u_t val) {
	memmap_cluster_t *mc = memmapca_id_to_memmap_cluster(proc, proc->l2cfg_memmap_id);

	assert(mc != NULL);
	//size8u_t old_val = memmap_cluster_read4(proc, memmapca_id_to_memmap_cluster(proc, proc->l2cfg_memmap_id), paddr);
	if (width == 4) {
		memmap_cluster_write4(proc, mc, paddr, val);
	} else {
		pwarn(proc, "Write to l2cfg memory mapped registers only support word sizes");
		return;
	}
	paddr_t offset = paddr - mc->base_addr;

	//printf ("Write to L2CFG offset: %llx val: %llx\n", offset, val);

	switch (offset) {
        case 0x28: //Energy resource weights
                proc->arch_proc_options->mmvec_power_resource_weights = val;
                break;

        case 0x2c: //Energy configrations
               proc->arch_proc_options->mmvec_power_config = val;
               break;

	case 0x100: // QOS MODE

#if 0
		proc->arch_proc_options->qos_mode_normal_mux = (val >>  0) & 0x3;
		proc->arch_proc_options->qos_mode_danger_mux = (val >>  2) & 0x3;
		proc->arch_proc_options->qos_mode_extreme_danger_mux = (val >>  4) & 0x3;
		proc->arch_proc_options->qos_danger_l2fetch_stall = (val >> 12) & 0x1;
		proc->arch_proc_options->qos_stall_l2fetch_on_bus_conditions = (val >> 15) & 0x1;
#endif

		break;

	case 0x104: // QOS MAX TRANS
		proc->arch_proc_options->qos_max_rd    = (val >>  0) & 0xFF;
		proc->arch_proc_options->qos_max_lo_rd = (val >>  8) & 0xFF;
		proc->arch_proc_options->qos_max_wr    = (val >> 16) & 0xFF;
		proc->arch_proc_options->qos_max_lo_wr = (val >> 24) & 0xFF;
		break;

	case 0x108: // QOS_ISSUE_RATE
		proc->arch_proc_options->qos_rd_issue_rate = (val >> 0) & 0xFF;
		proc->arch_proc_options->qos_wr_issue_rate = (val >> 8) & 0xFF;
		proc->arch_proc_options->qos_danger_lo_watermark = (val >> 16) & 0xFF;
		break;

	case 0x10c: // QOS DANGER ISSUE RATE
		proc->arch_proc_options->qos_danger_issue_rate = (val >> 0) & 0xFF;
		proc->arch_proc_options->qos_extreme_danger_issue_rate = (val >> 8) & 0xFF;
		break;

	case 0x110: // QOS_SCOREBOARD_WATERMARK


#if 0
		proc->arch_proc_options->qos_l2fetch_watermark = (val >> 0) & 0xFF;
		if (proc->arch_proc_options->qos_danger_lo_watermark > 0 ) {
			proc->arch_proc_options->qos_danger_lo_watermark = (val >> 8) & 0xFF;
		} else {
			proc->arch_proc_options->qos_normal_lo_watermark = (val >> 8) & 0xFF;
		}
		proc->arch_proc_options->qos_hi_watermark = (val >> 16) & 0xFF;
#endif
		break;

	case 0x114: // QOS_SYS_PRI
		proc->arch_proc_options->qos_areq_lo = (val >>  0) & 0x7;
		proc->arch_proc_options->qos_areq_hi = (val >> 16) & 0x7;
		break;
	}

}


void sys_cfgtbl_dump(FILE * fp, processor_t *proc) {

	int i;

	for (i = 0; i <= 0x5c; i += 4) {
		fprintf(fp, "system cfg_table_%02x = 0x%08x\n", i, (unsigned int)sys_cfgtbl_read(NULL, proc, proc->cfg_table_memmap_id, 0, (proc->global_regs[REG_CFGBASE] << 16) + i, 4));
	}
}


size4s_t mem_load_phys(thread_t * thread, size4u_t src1, size4u_t src2, insn_t *insn)
{
    int slot = 0;
	mem_access_info_t *maptr = &thread->mem_access[slot];
	size8u_t data;


	FATAL_REPLAY;

	/* Fill in the basic stuff */
	maptr->paddr = ((size8u_t) src1 & 0x7ff) | (((size8u_t) src2) << 11);
	maptr->vaddr = 0xdeadbeef;	/* Seems close to 'physaddr', no? */
	maptr->width = 4;
	maptr->type = access_type_load_phys;
	maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->tnum = thread->threadId;
    maptr->cancelled = 0;
    maptr->valid = 1;
	//maptr->iswrite = 0;

	/* Attributes of insn/packet needed by the uarch */
	//maptr->page_mask = 0xffffffff;
	//maptr->double_access = 0;
	//maptr->dcfetch_and_access = 0;
	//maptr->dual_store = 0;
	//maptr->possible_pgxing = 0;

	/* Configure attributes to UC. The cache will always lookup L1, L2
	   anyway   */
	/* FIXME - removed page_attribs from here, but we probably need some
	 * tracking of ARM attributes, right? - dmv */
#if 0
	maptr->page_attribs =
		(C_DEVICE(0) |
		 C_L1DC(0) | C_L1WB(0) | C_L1WT(0) |
		 C_L1IC(0) | C_L2AUX(0) | C_L2C(0) | C_L2WB(0) | C_L2WT(0));

	SET_CBITS(maptr->page_attribs, 0x6);	/* True UC */
#endif

#ifdef VERIFICATION
        if (! (fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_DCEN)||
               fREAD_GLOBAL_REG_FIELD(SYSCONF,SYSCFG_MMUEN)) )
          thread->has_devtype_ld |= 1;
#endif

	/* Force alignment and mask away bits greater than 36 */
	maptr->paddr &= 0x0000000FFFFFFFFCLL;

	/* See if this is a CFGTBL load. This will bypass the cache and memwrap_read */



	/* if ((maptr->paddr >> 16) == */
	/* 	thread->processor_ptr->global_regs[REG_CFGBASE]) { */
	/* 	return (sys_cfgtbl_read(thread->processor_ptr, maptr->paddr)); */
	/* } */

	/* We've succesfully completed the load. Call the wrapmem function which
	   either gets it from the cache in case of DBC, or from sim_mem_read otherwise
	 */
	data = memread(thread, &thread->mem_access[slot], 1);
	LOG_MEM_LOAD(maptr->vaddr,maptr->paddr,maptr->width,data,slot);

	/* Remember this result in the shadow in case this packet gets restarted due
	 * to the other slot */
	thread->shadow_valid[slot] = 1;
	thread->shadow_data[slot] = data;

	warn("mem_load_phys: thread %d paddr %llx CFG_BASE=%x data=%x", thread->threadId,maptr->paddr,thread->processor_ptr->global_regs[REG_CFGBASE], data);
	return data;
}

size4u_t sys_icdatar(thread_t * thread, size4u_t indexway, int slot)
{
	processor_t *proc = thread->processor_ptr;
	int tnum = thread->threadId;

    mem_access_info_t *maptr = &thread->mem_access[slot];

    uarch_mem_access_info_t uma = { 0 };
	size4u_t data = 0;

    if (REPLAY_DETECTED) return 0;
	memset(maptr, 0, sizeof(mem_access_info_t));

    maptr->pc_va = thread->Regs[REG_PC];
	maptr->vaddr = indexway;
	maptr->type = access_type_icdatar;
	maptr->bp = GET_SSR_FIELD(SSR_BP);

    if (thread->timing_on) {
		/* For now, packet ids and xacts only work in timing mode */
		uma.packet_id = packet_getid(proc, PACKET_CLASS_UNDEF);
		uma.commit_packet_number = thread->tstats[tpkt];
		uarch_derive_umaptr_from_maptr(proc, tnum, &uma, maptr);
		proc->uarch.cache_icache_access_ptr(proc, tnum, maptr, &uma);
		if (REPLAY_DETECTED) return 0;
	}
	thread->shadow_valid[slot] = 1;
	data = memwrap_icdatar(thread, indexway);
	thread->shadow_data[slot] = data;
	return data;
}

void
sys_ictagw(thread_t * thread, size4u_t indexway, size4u_t tagstate, int slot)
{

	int tnum = thread->threadId;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
    maptr->asid = GET_FIELD(SSR_ASID,READ_RREG(REG_SSR));
	maptr->vaddr = indexway;
	maptr->stdata = tagstate;
	maptr->type = access_type_ictagw;
    maptr->paddr = 0xdeadbeef;
    maptr->width = 4;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->cancelled = 0;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
	maptr->log_as_tag = 1;
	maptr->no_deriveumaptr = 1;
	memwrap_ictagw(thread, indexway, tagstate);

	return;
}



size4u_t sys_ictagr(thread_t * thread, size4u_t indexway, int slot, int regno)
{
  //	processor_t *proc = thread->processor_ptr;
	int tnum = thread->threadId;
	size4u_t data = 0xdeadbeef;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

    FATAL_REPLAY;

    memset(&thread->mem_access[slot], 0, sizeof(thread->mem_access[slot]));
    maptr->asid = GET_FIELD(SSR_ASID,READ_RREG(REG_SSR));
	maptr->vaddr = indexway;
	maptr->type = access_type_ictagr;
    maptr->paddr = 0xdeadbeef;
    maptr->width = 4;
    maptr->regno = regno;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->cancelled = 0;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
 	maptr->log_as_tag = 1;
	maptr->sys_reg_update_callback = sys_ictagr_update_state;
	maptr->no_deriveumaptr = 1;

	thread->shadow_valid[slot] = 1;
    data = memwrap_ictagr(thread, indexway);
	thread->shadow_data[slot] = data;
	return data;
}

/* Callbacks for tag op hints */
void
sys_ictagr_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val)
{
	thread_t *thread = proc->thread[tnum];
	thread->ictagr_hint_val = val;
	return;
}
void
sys_dctagr_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val)
{
	thread_t *thread = proc->thread[tnum];
	thread->dctagr_hint_val = val;
	return;
}
void
sys_l2tagr_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val)
{
	thread_t *thread = proc->thread[tnum];
	thread->l2tagr_hint_val = val;
	return;
}
void
sys_icdatar_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val)
{
	thread_t *thread = proc->thread[tnum];
	thread->icdatar_hint_val = val;
	return;
}
void
sys_l2_global_tagop_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val)
{
	thread_t *thread = proc->thread[tnum];
	fWRITE_GLOBAL_REG_FIELD(SYSCONF, SYSCFG_L2GCA, (val & 0x1));
	return;
}


void
sys_l2tagw(thread_t * thread, size4u_t indexway, size4u_t data, int slot)
{
	int tnum = thread->threadId;
	mem_access_info_t *maptr = &(thread->mem_access[slot]);

	FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
	maptr->vaddr = indexway;
	maptr->stdata = data;
	maptr->type = access_type_l2tagw;
	maptr->paddr = 0xdeadbeef;
	maptr->width = 4;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->cancelled = 0;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
 	maptr->log_as_tag = 1;
	maptr->no_deriveumaptr = 1;
	return;
}

size4u_t sys_l2tagr(thread_t * thread, size4u_t indexway, int slot, int regno)
{
	int tnum = thread->threadId;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);
	size4u_t data = 0xdeadbeef;
    processor_t *proc = thread->processor_ptr;

	if (REPLAY_DETECTED) {
        pfatal(proc, "This is not supposed to happen\n");
    }

    memset(maptr, 0, sizeof(mem_access_info_t));
	maptr->vaddr = indexway;
	maptr->type = access_type_l2tagr;
	maptr->paddr = 0xdeadbeef;
	maptr->width = 4;
    maptr->regno = regno;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->cancelled = 0;
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;

    /* Dummy data returned now. Real data updated later */
	data = memwrap_l2tagr(thread, indexway);

	thread->shadow_valid[slot] = 1;
	thread->shadow_data[slot] = data;

	maptr->log_as_tag = 1;
	maptr->sys_reg_update_callback = sys_l2tagr_update_state;
	maptr->no_deriveumaptr = 1;

	return data;
}

size4u_t
sys_dctagr(thread_t * thread, size4u_t indexway, int slot, int regno)
{
	int tnum = thread->threadId;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

#ifdef VERIFICATION
	return thread->dctagr_hint_val;
#endif

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
    maptr->vaddr = indexway;
    maptr->type = access_type_dctagr;
    maptr->tnum = tnum;
    maptr->cancelled = 0;
    maptr->slot = slot;
    maptr->regno = regno;
    maptr->pc_va = thread->Regs[REG_PC];
	maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
    maptr->log_as_tag = 1;
	maptr->sys_reg_update_callback = sys_dctagr_update_state;

    return memwrap_dctagr(thread, indexway);
}

void
sys_dctagw(thread_t * thread, size4u_t indexway, size4u_t tagstate,
		   int slot)
{
	int tnum = thread->threadId;
	mem_access_info_t *maptr = &(thread->mem_access[slot]);

	FATAL_REPLAY;
    memset(maptr, 0, sizeof(mem_access_info_t));

    maptr->pc_va = thread->Regs[REG_PC];
    maptr->vaddr = indexway;
    maptr->stdata = tagstate;
    maptr->type = access_type_dctagw;
    maptr->tnum = tnum;
    maptr->cancelled = 0;
    maptr->slot = slot;
    maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
	memwrap_dctagw(thread, indexway, tagstate);
	return;
}


int
sys_pause(thread_t *thread,  int slot, size4u_t val)
{
    int tnum = thread->threadId;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->type = access_type_pause;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->stdata = val;
	maptr->bp = GET_SSR_FIELD(SSR_BP);
    maptr->valid = 1;
	maptr->no_deriveumaptr = 1;
    return 1;
}


void
sys_speculate_branch_stall(thread_t * thread, int slot, size1u_t jump_cond,
										   size1u_t bimodal,
										   size1u_t dotnewval,
										   size1u_t hintbitnum,
										   size1u_t strbitnum,
										   int duration,
										   int pkt_has_dual_jump,
										   int insn_is_2nd_jump,
										   paddr_t jump_PA)
{
	branch_info_t * binfo_ptr = &thread->branch_info[slot];
	FATAL_REPLAY;

	if (thread->bq_on)
	{
		binfo_ptr->pc_va = thread->Regs[REG_PC];
		binfo_ptr->speculate = 1;
		binfo_ptr->valid = 1;
		binfo_ptr->slot = slot;
		binfo_ptr->tnum = thread->threadId;
		binfo_ptr->jump_cond = jump_cond;
		binfo_ptr->dotnewval = dotnewval;
		binfo_ptr->hintbitnum = hintbitnum;
		binfo_ptr->bimodal = bimodal;
		binfo_ptr->strbitnum = strbitnum;
		binfo_ptr->duration = duration;
		binfo_ptr->pkt_has_dual_jump = pkt_has_dual_jump;
		binfo_ptr->insn_is_2nd_jump = insn_is_2nd_jump;
		binfo_ptr->jump_PA = jump_PA;
	}
}

void
sys_branch_call(thread_t * thread, int slot, vaddr_t target, vaddr_t retaddr)
{

	branch_info_t * binfo_ptr = &thread->branch_info[slot];
	FATAL_REPLAY;

	if (thread->bq_on)
	{
		binfo_ptr->pc_va = thread->Regs[REG_PC];
		binfo_ptr->branch_op = branch_op_call;
		binfo_ptr->crl_valid = 1;
		binfo_ptr->slot = slot;
		binfo_ptr->tnum = thread->threadId;

		binfo_ptr->target = target;
		binfo_ptr->retaddr = retaddr;
	}
}

void
sys_branch_return(thread_t * thread, int slot, vaddr_t target, int regno)
{

	branch_info_t * binfo_ptr = &thread->branch_info[slot];
	FATAL_REPLAY;

	if (thread->bq_on)
	{
		binfo_ptr->pc_va = thread->Regs[REG_PC];
		binfo_ptr->branch_op = branch_op_return;
		binfo_ptr->crl_valid = 1;
		binfo_ptr->slot = slot;
		binfo_ptr->tnum = thread->threadId;
		binfo_ptr->target = target;
		binfo_ptr->regno = regno;
	}
}


/*
 * **********************************************************
 * Everything here down should get moved out or axed, I think
 * ***********************************************************
 */



/* Implements the staller for the wait instruction */
int staller_twait(thread_t * thread)
{
	if (!fGET_WAIT_MODE(thread->threadId)) {
		thread->status &= ~EXEC_STATUS_REPLAY;
		return 0;
	}

	return 1;
}


int staller_tfloat(thread_t * thread)
{
	int trotation;
	thread->stall_time--;

    assert(0 && "who is calling whate here, oh no");
	if (thread->stall_time == 0) {
		//fe_fix_scheduler_after_stall(thread->processor_ptr, thread->threadId);
		thread->status &= ~EXEC_STATUS_REPLAY;
		trotation = uarch_threads_in_rotation(thread->processor_ptr);
		if ((thread->processor_ptr->uarch.fe.dmt) && (trotation != 3) && (trotation != 4)) {
			DEC_TSTALLN(tfloat, 2);
		}
		return 1;
	}

	return 0;
}

void sys_flatency(thread_t * thread, int latency)
{
	int stall_time;
	int trotation;

	/* Check if we are already stalled */
	if (REPLAY_DETECTED) {
		return;
	}

	/* Check if we have already stalled this instruction */
	if (thread->float_icount == thread->tstats[tinsns]) {
		return;
	}
	thread->float_icount = thread->tstats[tinsns];

	stall_time = ((latency + 1) * 2);
	trotation = uarch_threads_in_rotation(thread->processor_ptr);
	if ((thread->processor_ptr->uarch.fe.dmt == 0) || (trotation == 3) || (trotation == 4)) {
		stall_time = (latency) * 4;
	}

	if (stall_time) {
		STALL_SET_TIMED(tfloat, stall_time);
	}
}

/* This function does a va->pa translation. Does not check for exeception,
   or do any such magic. Just simply looks up the tlb and returns a
   pa and tlb index */
paddr_t
xlate_va_to_pa_silent(processor_t * proc, int tnum, vaddr_t vaddr,
					  int *tlbidx)
{
	thread_t *thread = proc->thread[tnum];
	int i;
	size8u_t entry, entry_PA, paddr;
	size4u_t entry_PGSIZE;
	*tlbidx = -1;
	/* Do a full search, dont just break when found */
	for (i = 0; i < NUM_TLB_REGS(proc); i++) {
		if (tlb_match_idx(proc, i, GET_FIELD(SSR_ASID, READ_RREG(REG_SSR)), vaddr)) {
			*tlbidx = i;
		}
	}
	if (*tlbidx == -1) {
		/* No tlb entry. Assume VA = PA, and return -1 for tlbidx */
		*tlbidx = -1;
		return vaddr;
	}
	/* Found a valid tlb entry. Now translate */
	entry = proc->TLB_regs[*tlbidx];
	entry_PA = get_ppn(GET_PPD(entry));
	entry_PGSIZE = get_pgsize(GET_PPD(entry));
	paddr = ((entry_PA << 12) & ~(encmask_2_mask[entry_PGSIZE & 0x7])) |
		(vaddr & encmask_2_mask [entry_PGSIZE & 0x7]);
	return paddr;
}

int staller_nop_stall(thread_t * thread)
{
	if (thread->stall_time) {
		thread->stall_time--;
	}
	if (thread->stall_time == 0) {
		thread->status &= ~EXEC_STATUS_REPLAY;
		return 1;
	}
	return 0;
}

void sys_nop_executed(thread_t * thread)
{
//	processor_t *proc = thread->processor_ptr;
	if (thread->timing_on && thread->processor_ptr->uarch.cachedrive) {
		cachedrive_go(thread);
	}
}

int
sys_isync(thread_t * thread, int slot)
{
    int tnum = thread->threadId;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

    FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->type = access_type_isync;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->valid = 1;
    maptr->no_deriveumaptr = 1;
    return 1;
}

int
sys_barrier(thread_t *thread, int slot)
{
    int tnum = thread->threadId;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

	FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->type = access_type_barrier;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->valid = 1;
	maptr->no_deriveumaptr = 1;
    return 1;
}

int sys_sync(thread_t * thread, int slot)
{
    int tnum = thread->threadId;
    mem_access_info_t *maptr = &(thread->mem_access[slot]);

	FATAL_REPLAY;

    memset(maptr, 0, sizeof(mem_access_info_t));
    maptr->pc_va = thread->Regs[REG_PC];
    maptr->type = access_type_synch;
    maptr->slot = slot;
    maptr->tnum = tnum;
    maptr->valid = 1;
    maptr->no_deriveumaptr = 1;
	return 1;
}

int
sys_get_mem_size(thread_t *thread, insn_t *insn)
{
    if(GET_ATTRIB(insn->opcode, A_MEMSIZE_1B)) {
        return 1;
    } else if(GET_ATTRIB(insn->opcode, A_MEMSIZE_2B)) {
        return 2;
    } else if(GET_ATTRIB(insn->opcode, A_MEMSIZE_4B)) {
        return 4;
    } else if(GET_ATTRIB(insn->opcode, A_MEMSIZE_8B)) {
        return 8;
    }

    return 32;
}

paddr_t
mem_init_access_cancelled(thread_t * thread, int slot, size4u_t vaddr,
                          int width, enum mem_access_types mtype,
                          int type_for_xlate)
{
    paddr_t paddr;

	paddr = mem_init_access_silent(thread, slot, vaddr, 1, mtype, type_for_xlate, 1);
    thread->mem_access[slot].cancelled = 1;


    return paddr;
}

void
mem_general_load_cancelled(thread_t * thread, size4u_t vaddr, insn_t *insn)
{
    paddr_t paddr;
    int width = sys_get_mem_size(thread, insn);
    int slot = insn->slot;

#if 0
    if (REPLAY_DETECTED) {
        printf("Replay not allowed in %s:tnum=%d:PC=%x:stall=%d\n",
              __FUNCTION__, thread->threadId, thread->Regs[REG_PC], thread->stall_type);
    }
#endif
	FATAL_REPLAY;

    /* This was already cancelled. No need to log it again. */
	if (thread->shadow_valid[slot]) {
		return;
	}

	paddr = mem_init_access_cancelled(thread, slot, vaddr, width, access_type_load, TYPE_LOAD);

    if(paddr == 0x0) {
        if (!thread->bq_on) {
            INC_TSTAT(tinsn_cancelled_new_ld_exception);
        }
        return;
    }

	thread->shadow_valid[slot] = 1;

    if(thread->timing_on) {
        thread->mem_access[slot].lddata = 0xdeadbeef;
    }

    return;
}

void
mem_general_store_cancelled(thread_t * thread, size4u_t vaddr, insn_t *insn)
{
	paddr_t paddr;
    int slot = insn->slot;
    int width = sys_get_mem_size(thread, insn);

	FATAL_REPLAY;

	paddr = mem_init_access_cancelled(thread, slot, vaddr, width, access_type_store, TYPE_STORE);

    if(paddr == 0x0) {
        if (!thread->bq_on) {
            INC_TSTAT(tinsn_cancelled_new_st_exception);
        }
        return;
    }

    thread->mem_access[slot].stdata = 0xdeadbeef;
	thread->store_pending[slot] = 1;
	LOG_MEM_STORE(vaddr,paddr,width,0xdeadbeef,slot);

    return;
}

bool
can_dispatch_without_cracking(processor_t *proc, packet_t *packet, size1u_t bigcore_slots_pending, size1u_t tinycore_slots_avail, int* slotmap) {
    size2u_t opcode;
    size1u_t sitype;
    int bigcore_slot;
    int tinycore_slot;
    size1u_t bigcore_insn_index;

    if (bigcore_slots_pending == 0) return true; // no more bigcore slot pending mapping
    if (tinycore_slots_avail == 0) return false; // no more tinycore slot still available

    bigcore_slot = SLOTS_MAX-1;
    while (!(fEXTRACTU_BITS(bigcore_slots_pending, 1, bigcore_slot)))
        bigcore_slot--;

    for (bigcore_insn_index = 0; bigcore_insn_index < packet->num_insns; bigcore_insn_index++) {
        opcode = packet->insn[bigcore_insn_index].opcode;
        // ignore some inst
        if (opcode == A2_nop) continue;
        if (opcode == J2_endloop01) continue;
        if (opcode == J2_endloop0) continue;
        if (opcode == J2_endloop1) continue;
        if (packet->insn[bigcore_insn_index].part1) continue;
        if (packet->insn[bigcore_insn_index].slot == bigcore_slot) break;
    }

    CACHE_ASSERT(proc, bigcore_insn_index != packet->num_insns);
    opcode = packet->insn[bigcore_insn_index].opcode;
    sitype = insn_sitype[opcode];

    if (proc->arch_proc_options->pkt_crack_insn_order_matters) {
        // in case of last unmapping bigcore slot, we first try to map it to tinycore slot 0
        if (!fEXTRACTU_BITS(bigcore_slots_pending, bigcore_slot, 0) // last CPA
                && sitype_allowed_uslot[sitype][0] // bigcore slot is allowed on tinycore slot 0
                && fEXTRACTU_BITS(tinycore_slots_avail, 1, 0)) // tinycore slot 0 is still available
            tinycore_slot = 0;
        else {
            // Wow, the bigcore_slot is not the last CPA, or it cannot be mapped to slot0
            // Then we start the regular serach, try to find the first available tinycore slot that the bigcore slot can map to
            tinycore_slot = SLOTS_MAX-1;
            while (!sitype_allowed_uslot[sitype][tinycore_slot] || !fEXTRACTU_BITS(tinycore_slots_avail, 1, tinycore_slot)) {
                if (tinycore_slot == 0)
                    return false; // didn't find a legal mapping
                else
                    tinycore_slot--;
            }
        }
        // Wow, we find a tinycore slot for the first unmapping bigcore slot
        fINSERT_BITS(bigcore_slots_pending, 1, bigcore_slot, 0);
        fINSERT_BITS(tinycore_slots_avail, SLOTS_MAX-tinycore_slot, tinycore_slot, 0);
        slotmap[bigcore_slot] = tinycore_slot;
        return can_dispatch_without_cracking(proc, packet, bigcore_slots_pending, tinycore_slots_avail, slotmap);
    }
    else {
        for (tinycore_slot = SLOTS_MAX-1; tinycore_slot >= 0; tinycore_slot--) {
            if (sitype_allowed_uslot[sitype][tinycore_slot] && fEXTRACTU_BITS(tinycore_slots_avail, 1, tinycore_slot)) {
                fINSERT_BITS(bigcore_slots_pending, 1, bigcore_slot, 0);
                fINSERT_BITS(tinycore_slots_avail, 1, tinycore_slot, 0);
                slotmap[bigcore_slot] = tinycore_slot;
                if (can_dispatch_without_cracking(proc, packet, bigcore_slots_pending, tinycore_slots_avail, slotmap))
                    return true;
                fINSERT_BITS(bigcore_slots_pending, 1, bigcore_slot, 1);
                fINSERT_BITS(tinycore_slots_avail, 1, tinycore_slot, 1);
            }
        }
        return false; // didn't find any legal mapping
    }
}

bool
is_native_tinycore_packet (thread_t* thread, packet_t* packet) {
    size2u_t opcode;
	processor_t *proc = thread->processor_ptr;
    size1u_t bigcore_slots_pending = 0;
    for (int i = 0; i < packet->num_insns; i++) {
        opcode = packet->insn[i].opcode;
        if (opcode == A2_nop) continue;
        if (opcode == J2_endloop01) continue;
        if (opcode == J2_endloop0) continue;
        if (opcode == J2_endloop1) continue;
        if (packet->insn[i].part1) continue;
        CACHE_ASSERT(proc, !(fEXTRACTU_BITS(bigcore_slots_pending, 1,  packet->insn[i].slot)));
        fINSERT_BITS(bigcore_slots_pending, 1,  packet->insn[i].slot, 1);
    }
    int slotmap[] = {-1, -1, -1, -1};
    return can_dispatch_without_cracking(proc, packet, bigcore_slots_pending, CORE_SLOTMASK, slotmap);
}

#define IS_EARLY_DISPATCH(slot) (fEXTRACTU_BITS(subpkt_slotmask[0], 1, slot))
#define IS_LATE_DISPATCH(slot) (fEXTRACTU_BITS(subpkt_slotmask[1], 1, slot))

#define SET_LATE_DISPATCH(slot) \
    fINSERT_BITS(subpkt_slotmask[0], 1, slot, 0); \
    fINSERT_BITS(subpkt_slotmask[1], 1, slot, 1);

#define SET_EARLY_DISPATCH(slot) \
    fINSERT_BITS(subpkt_slotmask[0], 1, slot, 1); \
    fINSERT_BITS(subpkt_slotmask[1], 1, slot, 0);

size8u_t get_subinsn_class(size4u_t opcode) {
    /* convert subinsn classes to best fit iclass */
    switch (opcode_encodings[opcode].enc_class) {
        case SUBINSN_A:
            return ICLASS_ALU32_2op;
        case SUBINSN_L1:
        case SUBINSN_L2:
            return ICLASS_LD;
        case SUBINSN_S1:
        case SUBINSN_S2:
            return ICLASS_ST;
        default:
            return 0;
    }
    return 0;
}

size4u_t get_iclass(insn_t *insn) {
    size8u_t iclass = insn->iclass;
    /* convert subinsn classes to best fit iclass */
    if ( GET_ATTRIB(insn->opcode, A_SUBINSN) && (insn->slot==0 || insn->slot==1) ) {
        iclass = get_subinsn_class(insn->opcode);
    }
    return iclass;
}
#endif

int
sys_xlate_dma(thread_t *thread, uintptr_t va, int access_type, int maptr_type,int slot, size4u_t align_mask,
  xlate_info_t *xinfo, exception_info *einfo)
{
  int ret = 1;

  memset(einfo,0,sizeof(*einfo));
  memset(xinfo,0,sizeof(*xinfo));
  xinfo->pte_u = 1;
  xinfo->pte_w = 1;
  xinfo->pte_r = 1;
  xinfo->pte_x = 1;
  xinfo->pa = (uint64_t)va;

  return ret;
}

