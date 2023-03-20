/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#ifndef _HMX_SYSTEM_H
#define _HMX_SYSTEM_H

#undef LOG_MEM_STORE
#define LOG_MEM_STORE(...)

#define VERIFY_EXTENSIVELY(PROC, A)              \
    if(!((PROC)->options->dont_verify_extensively)) { A; }
#define CORE_SLOTMASK (thread->processor_ptr->arch_proc_options->QDSP6_TINY_CORE ? 13 : 15)

#define MEM_READ(SIZE) \
static inline size##SIZE##u_t hmx_mem_read##SIZE(system_t *system, paddr_t addr) \
{ \
    paddr_t offset = addr - system->vtcm_base; \
    return *(size##SIZE##u_t *)((uintptr_t)(system->vtcm_haddr) + offset); \
}
MEM_READ(1)
MEM_READ(2)
MEM_READ(4)
MEM_READ(8)

#define MEM_WRITE(SIZE) \
static inline void hmx_mem_write##SIZE(system_t *system, paddr_t addr, size##SIZE##u_t val) \
{ \
    paddr_t offset = addr - system->vtcm_base; \
    *(size##SIZE##u_t *)((uintptr_t)(system->vtcm_haddr) + offset) = val; \
}
MEM_WRITE(1)
MEM_WRITE(2)
MEM_WRITE(4)
MEM_WRITE(8)

/* HMX depth size in log2 */
static inline int get_hmx_channel_size(const processor_t *proc)
{
    /* return ctz32(proc->arch_proc_options->QDSP6_MX_CHANNELS); */
    return log2((float)proc->arch_proc_options->QDSP6_MX_CHANNELS);
}

static inline int get_hmx_block_bit(processor_t *proc)
{
    return get_hmx_channel_size(proc) + proc->arch_proc_options->
                                        hmx_spatial_size;
}

#define register_coproc_ldst_exception(...)

paddr_t hmx_mem_init_access(thread_t * thread, int slot, size4u_t vaddr, int width,
		enum mem_access_types mtype, int type_for_xlate, int page_size);

paddr_t hmx_mem_init_access_unaligned(thread_t * thread, int slot, size4u_t vaddr, size4u_t realvaddr, int size,
		enum mem_access_types mtype, int type_for_xlate, int page_size);

size4u_t count_leading_ones_1(size1u_t src);
size4u_t count_leading_ones_8(size8u_t src);
#endif
