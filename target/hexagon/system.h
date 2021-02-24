/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#ifndef _SYSTEM_H
#define _SYSTEM_H

#undef LOG_MEM_STORE
#define LOG_MEM_STORE(...)

/* FIXME: Use the mem_access_types instead */
#define TYPE_LOAD 'L'
#define TYPE_STORE 'S'
#define TYPE_FETCH 'F'
#define TYPE_ICINVA 'I'
#define TYPE_DMA_FETCH 'D'

//#include "thread.h"
#include "macros.h"
#include "xlate_info.h"
#include "iclass.h"
//#include "memwrap.h"

#define thread_t CPUHexagonState

#define TCM_UPPER_BOUND 0x400000
#define L2VIC_UPPER_BOUND 0x10000
#define TCM_RANGE(PROC, PADDR)                                  \
    (((PADDR) >= (PROC)->options->l2tcm_base) &&                \
     ((PADDR) < (PROC)->options->l2tcm_base + TCM_UPPER_BOUND))

#define L2VIC_RANGE(PROC, PADDR)                                  \
	((0 != (PROC)->arch_proc_options->fastl2vic_base) &&											\
	 ((PADDR) >= (PROC)->arch_proc_options->fastl2vic_base) &&								\
	 ((PADDR) < (PROC)->arch_proc_options->fastl2vic_base + L2VIC_UPPER_BOUND))


#ifdef CLADE
#define CLADE_RANGE(PROC, PADDR, WIDTH)																\
	 (clade_isclade((PROC), (PADDR), (WIDTH)))

#define CLADE_REG_RANGE(PROC, PADDR, WIDTH)														\
	 (clade_iscladereg((PROC), (PADDR), (WIDTH)))
#endif

#ifdef CLADE2
#define CLADE2_RANGE(PROC, PADDR, WIDTH)																\
	 (clade2_isclade2((PROC), (PADDR), (WIDTH)))

#define CLADE2_REG_RANGE(PROC, PADDR, WIDTH)														\
	 (clade2_isclade2reg((PROC), (PADDR), (WIDTH)))
#endif

#define VERIFY_EXTENSIVELY(PROC, A)              \
    if(!((PROC)->options->dont_verify_extensively)) { A; }

#define CFG_TABLE_RANGE(PROC, PADDR)																	\
	(((PADDR) >= (PROC)->global_regs[REG_CFGBASE] << 16) &&							\
	 ((PADDR) < ((PROC)->global_regs[REG_CFGBASE] << 16) + (0x1 << 16)))

#define CORE_SLOTMASK (thread->processor_ptr->features->QDSP6_TINY_CORE ? 13 : 15)

#ifdef FIXME
void k0lock_queue_init(processor_t *proc);
void sys_check_privs(thread_t * thread);
void sys_check_framelimit_slowpath(thread_t *thread, size4u_t vaddr, size4u_t limit, size4u_t badva);
static inline void sys_check_framelimit(thread_t *thread, size4u_t vaddr, size4u_t badva)
{
	size4u_t limit = thread->Regs[REG_FRAMELIMIT];
	if (LIKELY(vaddr >= limit)) return;
	sys_check_framelimit_slowpath(thread,vaddr,limit, badva);
}
void sys_illegal(thread_t * thread);
void sys_check_guest(thread_t * thread);
void sys_recalc_num_running_threads(processor_t * proc);
void sys_utlb_invalidate(processor_t * proc, thread_t *th);

void sys_dcfetch(thread_t * thread, size4u_t vaddr, int slot);
void sys_l2fetch(thread_t * thread,  size4u_t vaddr,
		size4u_t height, size4u_t width, size4u_t stride_in_bytes,
				 size4u_t flags, int slot);
void sys_tlb_write(thread_t * thread, int index, size8u_t data);
//void sys_tlb_refresh_attributes(processor_t * proc, size4u_t syscfg);
int sys_check_overlap(thread_t * thread, size8u_t data);

void sys_write_global_creg(thread_t * thread, int rnum, size4u_t data);
void sys_write_local_creg(thread_t * thread, int rnum, size4u_t data);
size4u_t sys_read_creg(processor_t * proc, int rnum); /* badly named... rename to sys_read_global_reg or something */
size4u_t sys_read_ucreg(const thread_t * thread, int rnum);
size4u_t sys_read_sreg(const thread_t * thread, int rnum);
size4u_t sys_read_greg(const thread_t * thread, unsigned int rnum);

int can_accept_interrupt(processor_t * proc, int tnum, int *intnum);
void sys_take_interrupt(processor_t * proc, int tnum, int intnum);

void sys_reschedule_interrupt(processor_t * proc);


void register_einfo(thread_t *thread,exception_info *einfo);
void register_error_exception(thread_t * thread, size4u_t error_code,
							  size4u_t badva0, size4u_t badva1,
							  size4u_t bvs, size4u_t bv0, size4u_t bv1,
							  size4u_t slotmask);
void register_trap_exception(thread_t * thread, size4u_t elr, int trapno, int imm);
void register_guest_trap_exception(thread_t * thread, size4u_t elr, int trapno, int imm);
void register_reset_interrupt(thread_t * thread, int tnum);
void register_nmi_interrupt(thread_t * thread);
void register_debug_exception(thread_t * thread, size4u_t error_code);
void register_fp_exception(thread_t * thread, size4u_t error_code);
void register_coproc_exception(thread_t * thread);

void register_dma_error_exception(thread_t * thread, exception_info *einfo, size4u_t va);

void register_coproc2_exception(thread_t * thread);
#endif /* FIXME */
void register_coproc_ldst_exception(thread_t * thread, int slot, size4u_t badva);
#ifdef FIXME
void commit_exception_info(thread_t * thread);
void rewind_exception_info(thread_t * thread);

size4u_t tlb_lookup(thread_t * thread, size4u_t ssr, size4u_t VA,
					int is_tlbp);
size4u_t tlb_lookup_by_asid(thread_t * thread, size4u_t asid, size4u_t VA,
					int is_tlbp);
int tlb_match_idx(processor_t * P, int i, size4u_t ssr, size4u_t VA);
void sys_waiting_for_tlb_lock(thread_t * thread);
void sys_waiting_for_k0_lock(thread_t * thread);
void sys_initiate_clear_k0_lock(thread_t * thread);
int sys_k0lock_queue_empty(thread_t *thread);
int sys_k0lock_queue_ready(thread_t *thread);


void recalculate_level_ipend(processor_t * proc);

size8u_t mem_general_load(thread_t * thread, size4u_t vaddr, int width, insn_t *insn);
void mem_general_load_cancelled(thread_t * thread, size4u_t vaddr, insn_t *insn);
paddr_t mem_init_access_direct(thread_t * thread, int slot, size4u_t vaddr,
	   int width, enum mem_access_types mtype, int type_for_xlate);
paddr_t mem_init_access_silent(thread_t * thread, int slot, size4u_t vaddr,
	   int width, enum mem_access_types mtype, int type_for_xlate, int cancel);
#endif
paddr_t mem_init_access(thread_t * thread, int slot, size4u_t vaddr, int width,
		enum mem_access_types mtype, int type_for_xlate);

paddr_t mem_init_access_unaligned(thread_t * thread, int slot, size4u_t vaddr, size4u_t realvaddr, int size,
		enum mem_access_types mtype, int type_for_xlate);

void mem_dmalink_store(thread_t * thread, size4u_t vaddr, int width, size8u_t data, int slot);

#ifdef FIXME

int check_release(thread_t * thread, size4u_t vaddr, paddr_t paddr, size1u_t data, insn_t *insn);
void mem_general_store(thread_t * thread, size4u_t vaddr, int width, size8u_t data, insn_t *insn);
void mem_general_store_cancelled(thread_t * thread, size4u_t vaddr, insn_t *insn);

void mem_vtcm_memcpy(thread_t *thread, insn_t *insn, vaddr_t dst, vaddr_t src, size4u_t length);


void sys_dcinva(thread_t * thread, size4u_t va);
void sys_dcinvidx(thread_t * thread, size4u_t data);
void sys_dccleana(thread_t * thread, size4u_t va);
void sys_dccleaninva(thread_t * thread, size4u_t va, int slot);
void sys_dccleanidx(thread_t * thread, size4u_t data);
void sys_dccleaninvidx(thread_t * thread, size4u_t data);
void sys_dczeroa(thread_t * thread, size4u_t va);
void sys_icinva(thread_t * thread, size4u_t va, int slot);
void sys_l2cleaninvidx(thread_t * thread, size4u_t data);
void sys_l2invidx(thread_t * thread, size4u_t data);
void sys_l2cleanidx(thread_t * thread, size4u_t data);
void sys_l2gunlock(thread_t * thread);
void sys_l2gclean(thread_t * thread);
void sys_l2gcleaninv(thread_t * thread);
void sys_l2gclean_pa(thread_t * thread, size8u_t data);
void sys_l2gcleaninv_pa(thread_t * thread, size8u_t data);
size4u_t sys_l2locka(thread_t * thread, vaddr_t va, int slot, int pregno);
void sys_l2unlocka(thread_t * thread, vaddr_t va, int slot);
int sys_sync(thread_t * thread, int slot);
int sys_barrier(thread_t *thread, int slot);
int sys_isync(thread_t * thread, int slot);
void sys_nop_executed(thread_t * thread);
void sys_ciad(thread_t * thread, size4u_t val);
void sys_siad(thread_t * thread, size4u_t val);

size8s_t mem_load_locked(thread_t * thread, size4u_t vaddr, int width, insn_t *insn);
int mem_store_conditional(thread_t * thread, size4u_t vaddr, size8u_t value, int width, insn_t *insn);

size8u_t sys_cfgtbl_read(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width);
void sys_cfgtbl_write(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width, size8u_t val);
void sys_cfgtbl_init(processor_t *proc);

size8u_t sys_l2cfg_read(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width);
void sys_l2cfg_write(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width, size8u_t val);
void sys_l2cfg_init(processor_t *proc);

size8u_t sys_ecc_reg_read(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width);
void sys_ecc_block_init(processor_t *proc, int mem_offset, size4u_t granule_size, paddr_t pa_base, int num_regions);
void sys_ecc_reg_write(system_t *sys, processor_t *proc, int memmap_cluster_id, int tnum, paddr_t paddr, int width, size8u_t val);
void sys_ecc_reg_init(processor_t *proc);


void sys_cfgtbl_dump(FILE *fp, processor_t *proc);
size4s_t mem_load_phys(thread_t * thread, size4u_t src1, size4u_t src2, insn_t *insn);

size4u_t imem_try_read4(thread_t * thread, size4u_t vaddr, size4u_t * word, xlate_info_t *xinfo, exception_info *einfo);

void mem_finished_slot_cleanup_timing(thread_t * thread, int slot);

void mem_itlb_update(thread_t * thread, size8u_t entry, int jtlbidx);

int sys_xlate(thread_t *thread, size4u_t va, int access_type, int maptr_type, int slot, size4u_t align_mask,
		   xlate_info_t *info, exception_info *einfo);
int sys_xlate_ssr(thread_t *thread, size4u_t ssr, size4u_t va, int access_type, int slot, size4u_t align_mask,
		   xlate_info_t *info, exception_info *einfo);
paddr_t xlate_va_to_pa_silent(processor_t * proc, int tnum, vaddr_t vaddr,
							  int *tlbidx);


size4u_t sys_decode_cccc(processor_t * proc, int cbits, size4u_t syscfg);

size4u_t sys_ictagr(thread_t * thread, size4u_t data, int slot, int regno);
void sys_ictagw(thread_t * thread, size4u_t data, size4u_t tagstate,
				int slot);
size4u_t sys_icdatar(thread_t * thread, size4u_t indexway, int slot);
void sys_dctagw(thread_t * thread, size4u_t data, size4u_t tagstate,
				int slot);
size4u_t sys_l2tagr(thread_t * thread, size4u_t indexway, int slot, int regno);
void sys_l2tagw(thread_t * thread, size4u_t indexway, size4u_t data,
				int slot);
size4u_t sys_dctagr(thread_t * thread, size4u_t indexway, int slot, int regno);


void sys_ictagr_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val);
void sys_dctagr_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val);
void sys_l2tagr_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val);
void sys_icdatar_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val);
void sys_l2_global_tagop_update_state(processor_t * proc, int tnum, int type, int reg, int field, int val);

void
sys_speculate_branch_stall(thread_t * thread, int slot, size1u_t jump_cond,
										   size1u_t bimodal,
										   size1u_t dotnewval,
										   size1u_t hintbitnum,
										   size1u_t strbitnum,
										   int duration,
										   int pkt_has_dual_jump,
										   int insn_is_2nd_jump,
										   paddr_t jump_PA);

void
sys_branch_call(thread_t * thread, int slot, vaddr_t target, vaddr_t retaddr);


void
sys_branch_return(thread_t * thread, int slot, vaddr_t target, int regno);

enum { PGSIZE_4K, PGSIZE_16K, PGSIZE_64K, PGSIZE_256K, PGSIZE_1M,
	PGSIZE_4M,
	PGSIZE_16M
};

static const char ct1_6bit[64] = {
	PGSIZE_16M,					/* 000 000 */
	PGSIZE_4K,					/* 000 001 */
	PGSIZE_16K,					/* 000 010 */
	PGSIZE_4K,					/* 000 011 */
	PGSIZE_64K,					/* 000 100 */
	PGSIZE_4K,					/* 000 101 */
	PGSIZE_16K,					/* 000 110 */
	PGSIZE_4K,					/* 000 111 */
	PGSIZE_256K,				/* 001 000 */
	PGSIZE_4K,					/* 001 001 */
	PGSIZE_16K,					/* 001 010 */
	PGSIZE_4K,					/* 001 011 */
	PGSIZE_64K,					/* 001 100 */
	PGSIZE_4K,					/* 001 101 */
	PGSIZE_16K,					/* 001 110 */
	PGSIZE_4K,					/* 001 111 */
	PGSIZE_1M,					/* 010 000 */
	PGSIZE_4K,					/* 010 001 */
	PGSIZE_16K,					/* 010 010 */
	PGSIZE_4K,					/* 010 011 */
	PGSIZE_64K,					/* 010 100 */
	PGSIZE_4K,					/* 010 101 */
	PGSIZE_16K,					/* 010 110 */
	PGSIZE_4K,					/* 010 111 */
	PGSIZE_256K,				/* 011 000 */
	PGSIZE_4K,					/* 011 001 */
	PGSIZE_16K,					/* 011 010 */
	PGSIZE_4K,					/* 011 011 */
	PGSIZE_64K,					/* 011 100 */
	PGSIZE_4K,					/* 011 101 */
	PGSIZE_16K,					/* 011 110 */
	PGSIZE_4K,					/* 011 111 */
	PGSIZE_4M,					/* 100 000 */
	PGSIZE_4K,					/* 100 001 */
	PGSIZE_16K,					/* 100 010 */
	PGSIZE_4K,					/* 100 011 */
	PGSIZE_64K,					/* 100 100 */
	PGSIZE_4K,					/* 100 101 */
	PGSIZE_16K,					/* 100 110 */
	PGSIZE_4K,					/* 100 111 */
	PGSIZE_256K,				/* 101 000 */
	PGSIZE_4K,					/* 101 001 */
	PGSIZE_16K,					/* 101 010 */
	PGSIZE_4K,					/* 101 011 */
	PGSIZE_64K,					/* 101 100 */
	PGSIZE_4K,					/* 101 101 */
	PGSIZE_16K,					/* 101 110 */
	PGSIZE_4K,					/* 101 111 */
	PGSIZE_1M,					/* 110 000 */
	PGSIZE_4K,					/* 110 001 */
	PGSIZE_16K,					/* 110 010 */
	PGSIZE_4K,					/* 110 011 */
	PGSIZE_64K,					/* 110 100 */
	PGSIZE_4K,					/* 110 101 */
	PGSIZE_16K,					/* 110 110 */
	PGSIZE_4K,					/* 110 111 */
	PGSIZE_256K,				/* 111 000 */
	PGSIZE_4K,					/* 111 001 */
	PGSIZE_16K,					/* 111 010 */
	PGSIZE_4K,					/* 111 011 */
	PGSIZE_64K,					/* 111 100 */
	PGSIZE_4K,					/* 111 101 */
	PGSIZE_16K,					/* 111 110 */
	PGSIZE_4K,					/* 111 111 */
};

static inline char get_pgsize(size4u_t PPD)
{
	/* HEXDEV-2782 all zeros coming in maps to 4K page to match RTL*/
	if ((PPD & 0x7f) == 0)
		return PGSIZE_4K;

	return ct1_6bit[PPD & 0x3f];
}
static inline size8u_t get_ppn(size8u_t PPD)
{
	return PPD >> 1;
}

int mem_dtlb_lookup(thread_t * thread, mem_access_info_t * maptr,
					size4u_t vaddr);

int sys_pause(thread_t *thread,  int slot, size4u_t val);
//int sys_set_wait(processor_t *proc, int tnum);
int staller_twait(thread_t * thread);
void sys_flatency(thread_t * thread, int latency);
void sys_l2prefetch_update_state(processor_t *proc, int tnum, int reg, int field, int val);


#define FLATENCY(LATENCY)                        \
    if(thread->timing_on) {                      \
        sys_flatency(thread, (LATENCY));         \
    }

#define FATAL_REPLAY if (REPLAY_DETECTED) fatal("Replay not allowed in %s:tnum=%d:PC=%x:stall=%d\n", \
                                                __FUNCTION__, thread->threadId, thread->Regs[REG_PC], thread->stall_type)

#define L1S_OFFSET 0x200000

#define MEM_LOAD1(THREAD,VA,INSN) ((size1u_t)mem_load1(THREAD,VA,INSN))
#define MEM_LOAD2(THREAD,VA,INSN) ((size2u_t)mem_load2(THREAD,VA,INSN))
#define MEM_LOAD4(THREAD,VA,INSN) ((size4u_t)mem_load4(THREAD,VA,INSN))
#define MEM_LOAD8(THREAD,VA,INSN) ((size8u_t)mem_load8(THREAD,VA,INSN))

//#define MEM_LOADVEC(THREAD,ES,VA,STRIDE,MODE,SLOT) ((size256b_t)mem_load_vector(THREAD,ES,VA,STRIDE,MODE,SLOT))

#define MEM_STORE0(THREAD,VA,DATA,INSN) mem_store0(THREAD,VA,DATA,INSN)
#define MEM_STORE1(THREAD,VA,DATA,INSN) mem_store1(THREAD,VA,DATA,INSN)
#define MEM_STORE2(THREAD,VA,DATA,INSN) mem_store2(THREAD,VA,DATA,INSN)
#define MEM_STORE4(THREAD,VA,DATA,INSN) mem_store4(THREAD,VA,DATA,INSN)
#define MEM_STORE8(THREAD,VA,DATA,INSN) mem_store8(THREAD,VA,DATA,INSN)

//#define MEM_STOREVEC(THREAD,ES,VA,STRIDE,MODE,DATA,SLOT) mem_store_vector(THREAD,ES,VA,STRIDE,MODE,DATA,SLOT)



/* The FAST MEMORY CACHE */

/*
   This is a direct mapped cache that holds 4K translations from VA to hostaddr.

   Direct-mapped fast-memory-cache, 4K pages.
   Holds pointers to pages on host for direct access
 */

/* FIXME NOW VERIFICATION: Verify this is working correctly instead of turning it off */
#if defined(VERIFICATION) || defined(ZEBU_CHECKSUM_TRACE)
#define FMC_ENABLED 0
#define FMC_ENABLED_ST 0
#else
#define FMC_ENABLED 1			/* enable this feature */
#define FMC_ENABLED_ST 1		/* do stores as well */
#endif

#define ISALIGN(VA,SIZE) (((VA)&(SIZE-1))==0)

#define FMC_HASH(VA) ((VA)>>12)

#define FMC_IDX(VA)  ((FMC_HASH(VA))&(FAST_MEM_CACHE_SIZE-1))

#define FMC_PAGE(VA,IDX) (thread->fast_mem_cache[IDX].hostptr)

#define FMC_HOSTPTR(VA) ((char*)FMC_PAGE((VA),FMC_IDX(VA)) + ((VA)&0xfff))

#define FMC_LD(VA,SIZE) (*((SIZE*)((char*)FMC_PAGE((VA),FMC_IDX(VA)) + ((VA)&0xfff))))

#define FMC_ST(VA,SIZE,DATA) (*((SIZE*)FMC_HOSTPTR(VA)) = DATA)

#if 0
#define FMC_ST_CHECKER(VA,SIZE,SZ,DATA) {\
		thread->save_st_hostptr = FMC_HOSTPTR(VA);						\
		thread->save_st_data = DATA;									\
		thread->save_st_va = VA;										\
		thread->save_st_width = SZ;}
#endif

#ifdef VERIFICATION
#define SYSVERWARN(...) warn(__VA_ARGS__);
#else
#define SYSVERWARN(...)
#endif

#define INC_PCYCLE_REGS(PROC)                                           \
    if(!(PROC)->all_wait) {                                             \
        (PROC)->global_regs[REG_PCYCLEHI] += ((PROC)->global_regs[REG_PCYCLELO] == 0xffffffff); \
        ((PROC)->global_regs[REG_PCYCLELO]) ++;                         \
    }

#define CLEAR_PCYCLE_REGS(PROC)                     \
    (PROC)->global_regs[REG_PCYCLEHI] = 0;          \
    (PROC)->global_regs[REG_PCYCLELO] = 0;

int fmc_insert(thread_t * thread, size4u_t va);


static inline int fmc_hit_load(thread_t * thread, size4u_t va)
{
	int idx = FMC_IDX(va);
	/* XXX: FIXME: generate non-timing functions */
	if (UNLIKELY(thread->timing_on)) return 0;
	if (UNLIKELY(thread->store_pending[1])) return 0;
	/* If we have a VA->host mapping and the counter hasn't bumped,
	   then we can access this memory directly */
	if (LIKELY((thread->fast_mem_cache[idx].use_counter ==
		 thread->fmc_use_counter) && (thread->fast_mem_cache[idx].hostptr)
		&& (thread->fast_mem_cache[idx].va_tag == (va & 0xfffff000)))) {
		return FMC_ENABLED;
	}
	/* It's not there now, but see if we can do VA->host directly */
	return (fmc_insert(thread, va));
}

static inline int fmc_hit_store(thread_t * thread, size4u_t va)
{
	int idx = FMC_IDX(va);
	/* XXX: FIXME: generate non-timing functions */
	if (UNLIKELY(thread->timing_on)) return 0;
	/* We don't handle dual store - those must be posted.  */
	if (thread->last_pkt->dual_store) return 0;
	/* If we have a VA->host mapping and the counter hasn't bumped,
	   then we can access this memory directly */
	if (LIKELY((thread->fast_mem_cache[idx].use_counter ==
		 thread->fmc_use_counter) && (thread->fast_mem_cache[idx].hostptr)
		&& (thread->fast_mem_cache[idx].va_tag == (va & 0xfffff000)))) {
		return FMC_ENABLED;
	}
	/* It's not there now, but see if we can do VA->host directly */
	return (fmc_insert(thread, va));
}


static inline void mem_init_lean(thread_t *thread, size4u_t vaddr, insn_t *insn, enum mem_access_types mtype, int xlate_type, int width)
{
  	mem_access_info_t *maptr = &(thread->mem_access[insn->slot]);
    maptr->valid = 1;
    maptr->type = mtype;
    sys_xlate(thread, vaddr, xlate_type, mtype, insn->slot, width-1, &(maptr->xlate_info), &(thread->einfo));
    maptr->paddr = maptr->xlate_info.pa;
    maptr->pc_va = thread->Regs[REG_PC];
	maptr->cancelled = 0;
	if(insn->is_memop) {
  	  maptr->is_memop = 1;
	  maptr->type = access_type_load;
	}
}

static inline size1u_t mem_load1(thread_t * thread, size4u_t vaddr, insn_t *insn)
{
	if (LIKELY(FMC_ENABLED && fmc_hit_load(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  mem_init_lean(thread, vaddr, insn, access_type_load, TYPE_LOAD, 1);
		}
		return (FMC_LD(vaddr, size1u_t));
	} else {
		return (mem_general_load(thread, vaddr, 1, insn));
	}
}

static inline size2u_t mem_load2(thread_t * thread, size4u_t vaddr, insn_t *insn)
{
	if (LIKELY(ISALIGN(vaddr, 2) && FMC_ENABLED && fmc_hit_load(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  mem_init_lean(thread, vaddr, insn, access_type_load, TYPE_LOAD, 2);
		}
		return (FMC_LD(vaddr, size2u_t));
	} else {
		return (mem_general_load(thread, vaddr, 2, insn));
	}
}


static inline size4u_t mem_load4(thread_t * thread, size4u_t vaddr, insn_t *insn)
{
	if (LIKELY(ISALIGN(vaddr, 4) && FMC_ENABLED && fmc_hit_load(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  mem_init_lean(thread, vaddr, insn, access_type_load, TYPE_LOAD, 4);
		}
		return (FMC_LD(vaddr, size4u_t));
	} else {
		return (mem_general_load(thread, vaddr, 4, insn));
	}
}


static inline size8u_t mem_load8(thread_t * thread, size4u_t vaddr, insn_t *insn)
{
	if (LIKELY(ISALIGN(vaddr, 8) && FMC_ENABLED && fmc_hit_load(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  mem_init_lean(thread, vaddr, insn, access_type_load, TYPE_LOAD, 8);
		}
		return (FMC_LD(vaddr, size8u_t));
	} else {
		return (mem_general_load(thread, vaddr, 8, insn));
	}
}

/* Store for release instructions */
static inline void mem_store0(thread_t * thread, size4u_t vaddr, size1u_t data, insn_t *insn)
{
	paddr_t paddr = mem_init_access(thread,insn->slot,vaddr,1,access_type_store,TYPE_STORE);
	if(check_release(thread,vaddr,paddr,data,insn)) {
		thread->mem_access[insn->slot].is_release = 1;
		LOG_MEM_STORE(vaddr,paddr,0,data,insn->slot);
	}
}

static inline void mem_store1(thread_t * thread, size4u_t vaddr, size1u_t data, insn_t *insn)
{
	/* See if we can store directly to the host */
	if (LIKELY(ISALIGN(vaddr, 1) && FMC_ENABLED_ST && fmc_hit_store(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  	mem_init_lean(thread, vaddr, insn, access_type_store, TYPE_STORE, 1);
		}
		FMC_ST(vaddr, size1u_t, data);
	} else {
		mem_general_store(thread, vaddr, 1, data, insn);
	}
}


static inline void mem_store2(thread_t * thread, size4u_t vaddr, size2u_t data, insn_t *insn)
{
	/* See if we can store directly to the host */
	if (LIKELY(ISALIGN(vaddr, 2) && FMC_ENABLED_ST && fmc_hit_store(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  mem_init_lean(thread, vaddr, insn, access_type_store, TYPE_STORE, 2);
		}
		FMC_ST(vaddr, size2u_t, data);
	} else {
		mem_general_store(thread, vaddr, 2, data, insn);
	}
}

static inline void mem_store4(thread_t * thread, size4u_t vaddr, size4u_t data, insn_t *insn)
{
	/* See if we can store directly to the host */
	if (LIKELY(ISALIGN(vaddr, 4) && FMC_ENABLED_ST && fmc_hit_store(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  mem_init_lean(thread, vaddr, insn, access_type_store, TYPE_STORE, 4);
		}
		FMC_ST(vaddr, size4u_t, data);
	} else {
		mem_general_store(thread, vaddr, 4, data, insn);
	}
}

static inline void mem_store8(thread_t * thread, size4u_t vaddr, size8u_t data, insn_t *insn)
{
	/* See if we can store directly to the host */
	if (LIKELY(ISALIGN(vaddr, 8) && FMC_ENABLED_ST && fmc_hit_store(thread, vaddr))) {
		if(thread->processor_ptr->uarch.cache_lean_ready) {
      	  mem_init_lean(thread, vaddr, insn, access_type_store, TYPE_STORE, 8);
		}
		FMC_ST(vaddr, size8u_t, data);
	} else {
		mem_general_store(thread, vaddr, 8, data, insn);
	}
}



static inline int sys_in_monitor_mode(thread_t * thread)
{
	if (!thread->debug_mode) {
		return ((GET_SSR_FIELD(SSR_UM) == 0)
				|| (GET_SSR_FIELD(SSR_EX) != 0));
	} else {
		return ((GET_SSR_FIELD(SSR_UM) == 0)
				|| (GET_SSR_FIELD(SSR_EX) != 0)
				|| ((fREAD_GLOBAL_REG_FIELD(ISDBCMD, ISDBCMD_PRIV) != 0) && (fREAD_GLOBAL_REG_FIELD(ISDBCMD, ISDBCMD_PRIV) != 1)));
	}
}

static inline int sys_in_guest_mode(thread_t * thread)
{
	if (sys_in_monitor_mode(thread)) return 0;

	if (!thread->debug_mode) {
		if (GET_SSR_FIELD(SSR_GM)) return 1;
		else return 0;
	} else {
		if ((GET_SSR_FIELD(SSR_GM) || fREAD_GLOBAL_REG_FIELD(ISDBCMD, ISDBCMD_PRIV) == 1) ) return 1;
		else return 0;
	}
}

static inline int sys_in_user_mode(thread_t * thread)
{
	if (sys_in_monitor_mode(thread)) return 0;
	if (GET_SSR_FIELD(SSR_GM)) return 0;
	else return 1;
}

void sys_stats_reset(processor_t * proc);

bool can_dispatch_without_cracking(processor_t *proc, packet_t *packet, size1u_t bigcore_slots_pending, size1u_t tinycore_slots_avail, int* slotmap);

size8u_t get_subinsn_class( size4u_t opcode);
size4u_t get_iclass(insn_t *insn);

bool is_native_tinycore_packet(thread_t* thread, packet_t* packet);

#endif  /* FIXME */
void iic_flush_cache(processor_t * proc);

extern int sys_xlate_dma(thread_t *thread, uintptr_t va, int access_type, int maptr_type,int slot, size4u_t align_mask,
  xlate_info_t *xinfo, exception_info *einfo);
#endif
