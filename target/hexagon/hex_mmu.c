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
#include "cpu.h"
#include "internal.h"
#include "exec/exec-all.h"
#include "hex_arch_types.h"
#include "hex_mmu.h"
#include "macros.h"
#include "reg_fields.h"

#define GET_TLB_FIELD(ENTRY, FIELD) \
    fEXTRACTU_BITS(ENTRY, reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)

/*
 * PPD (physical page descriptor) is formed by putting the PTE_PA35 field
 * in the MSB of the PPD
 */
#define GET_PPD(ENTRY) \
    ((GET_TLB_FIELD((ENTRY), PTE_PPD) | \
     (GET_TLB_FIELD((ENTRY), PTE_PA35) << reg_field_info[PTE_PPD].width)))

#define NO_ASID      (1 << 8)

typedef enum {
    PGSIZE_4K,
    PGSIZE_16K,
    PGSIZE_64K,
    PGSIZE_256K,
    PGSIZE_1M,
    PGSIZE_4M,
    PGSIZE_16M
} tlb_pgsize_t;

static const char *pgsize_str[] = {
    "4K",
    "16K",
    "64K",
    "256K",
    "1M",
    "4M",
    "16M"
};

/* The lower 6 bits of PPD determine the pgsize */
static const tlb_pgsize_t ct1_6bit[64] = {
    PGSIZE_16M,                         /* 000 000 */
    PGSIZE_4K,                          /* 000 001 */
    PGSIZE_16K,                         /* 000 010 */
    PGSIZE_4K,                          /* 000 011 */
    PGSIZE_64K,                         /* 000 100 */
    PGSIZE_4K,                          /* 000 101 */
    PGSIZE_16K,                         /* 000 110 */
    PGSIZE_4K,                          /* 000 111 */
    PGSIZE_256K,                        /* 001 000 */
    PGSIZE_4K,                          /* 001 001 */
    PGSIZE_16K,                         /* 001 010 */
    PGSIZE_4K,                          /* 001 011 */
    PGSIZE_64K,                         /* 001 100 */
    PGSIZE_4K,                          /* 001 101 */
    PGSIZE_16K,                         /* 001 110 */
    PGSIZE_4K,                          /* 001 111 */
    PGSIZE_1M,                          /* 010 000 */
    PGSIZE_4K,                          /* 010 001 */
    PGSIZE_16K,                         /* 010 010 */
    PGSIZE_4K,                          /* 010 011 */
    PGSIZE_64K,                         /* 010 100 */
    PGSIZE_4K,                          /* 010 101 */
    PGSIZE_16K,                         /* 010 110 */
    PGSIZE_4K,                          /* 010 111 */
    PGSIZE_256K,                        /* 011 000 */
    PGSIZE_4K,                          /* 011 001 */
    PGSIZE_16K,                         /* 011 010 */
    PGSIZE_4K,                          /* 011 011 */
    PGSIZE_64K,                         /* 011 100 */
    PGSIZE_4K,                          /* 011 101 */
    PGSIZE_16K,                         /* 011 110 */
    PGSIZE_4K,                          /* 011 111 */
    PGSIZE_4M,                          /* 100 000 */
    PGSIZE_4K,                          /* 100 001 */
    PGSIZE_16K,                         /* 100 010 */
    PGSIZE_4K,                          /* 100 011 */
    PGSIZE_64K,                         /* 100 100 */
    PGSIZE_4K,                          /* 100 101 */
    PGSIZE_16K,                         /* 100 110 */
    PGSIZE_4K,                          /* 100 111 */
    PGSIZE_256K,                        /* 101 000 */
    PGSIZE_4K,                          /* 101 001 */
    PGSIZE_16K,                         /* 101 010 */
    PGSIZE_4K,                          /* 101 011 */
    PGSIZE_64K,                         /* 101 100 */
    PGSIZE_4K,                          /* 101 101 */
    PGSIZE_16K,                         /* 101 110 */
    PGSIZE_4K,                          /* 101 111 */
    PGSIZE_1M,                          /* 110 000 */
    PGSIZE_4K,                          /* 110 001 */
    PGSIZE_16K,                         /* 110 010 */
    PGSIZE_4K,                          /* 110 011 */
    PGSIZE_64K,                         /* 110 100 */
    PGSIZE_4K,                          /* 110 101 */
    PGSIZE_16K,                         /* 110 110 */
    PGSIZE_4K,                          /* 110 111 */
    PGSIZE_256K,                        /* 111 000 */
    PGSIZE_4K,                          /* 111 001 */
    PGSIZE_16K,                         /* 111 010 */
    PGSIZE_4K,                          /* 111 011 */
    PGSIZE_64K,                         /* 111 100 */
    PGSIZE_4K,                          /* 111 101 */
    PGSIZE_16K,                         /* 111 110 */
    PGSIZE_4K,                          /* 111 111 */
};

#define INVALID_MASK 0xffffffffLL

size8u_t encmask_2_mask[] = {
    0x0fffLL,                           /* 4k,   0000 */
    0x3fffLL,                           /* 16k,  0001 */
    0xffffLL,                           /* 64k,  0010 */
    0x3ffffLL,                          /* 256k, 0011 */
    0xfffffLL,                          /* 1m,   0100 */
    0x3fffffLL,                         /* 4m,   0101 */
    0xffffffLL,                         /* 16M,  0110 */
    INVALID_MASK,                       /* RSVD, 0111 */
};

static inline tlb_pgsize_t hex_tlb_pgsize(uint64_t entry)
{
    uint32_t PPD = GET_PPD(entry);
    return ct1_6bit[PPD & 0x3f];
}

static inline uint32_t hex_tlb_page_size(uint64_t entry)
{
    return 1 << (TARGET_PAGE_BITS + 2 * hex_tlb_pgsize(GET_PPD(entry)));
}

static inline uint64_t hex_tlb_phys_page_num(uint64_t entry)
{
    uint32_t ppd = GET_PPD(entry);
    return ppd >> 1;
}

static inline uint64_t hex_tlb_phys_addr(uint64_t entry)
{
    uint64_t pagemask = encmask_2_mask[hex_tlb_pgsize(entry)];
    uint64_t pagenum = hex_tlb_phys_page_num(entry);
    uint64_t PA = (pagenum << TARGET_PAGE_BITS) & (~pagemask);
    return PA;
}

static inline uint64_t hex_tlb_virt_addr(uint64_t entry)
{
    return GET_TLB_FIELD(entry, PTE_VPN) << TARGET_PAGE_BITS;
}

static bool hex_dump_mmu_entry(FILE *f, uint64_t entry)
{
    if (GET_TLB_FIELD(entry, PTE_V)) {
        fprintf(f, "0x%016lx: ", entry);
        uint64_t PA = hex_tlb_phys_addr(entry);
        uint64_t VA = hex_tlb_virt_addr(entry);
        fprintf(f, "V:%lld G:%lld A1:%lld A0:%lld",
                 GET_TLB_FIELD(entry, PTE_V),
                 GET_TLB_FIELD(entry, PTE_G),
                 GET_TLB_FIELD(entry, PTE_ATR1),
                 GET_TLB_FIELD(entry, PTE_ATR0));
        fprintf(f, " ASID:0x%02llx VA:0x%08lx",
                GET_TLB_FIELD(entry, PTE_ASID),
                VA);
        fprintf(f, " X:%lld W:%lld R:%lld U:%lld C:%lld",
                GET_TLB_FIELD(entry, PTE_X),
                GET_TLB_FIELD(entry, PTE_W),
                GET_TLB_FIELD(entry, PTE_R),
                GET_TLB_FIELD(entry, PTE_U),
                GET_TLB_FIELD(entry, PTE_C));
        fprintf(f, " PA:0x%09lx SZ:%s (0x%x)",
                PA,
                pgsize_str[hex_tlb_pgsize(entry)],
                hex_tlb_page_size(entry));
        fprintf(f, "\n");
        return true;
    }

    /* Not valid */
    return false;
}

#if HEX_DEBUG
static void hex_dump_mmu(CPUHexagonState *env, FILE *f)
{
    bool valid_found = false;
    int i;
    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        valid_found |= hex_dump_mmu_entry(f, env->hex_tlb->entries[i]);
    }
    if (!valid_found) {
        fprintf(f, "TLB is empty :(\n");
    }
}
#endif

static inline void hex_log_tlbw(uint32_t index, uint64_t entry)
{
    if (qemu_loglevel_mask(CPU_LOG_MMU)) {
        QemuLogFile *logfile;
        if (qemu_log_enabled()) {
            rcu_read_lock();
            logfile = atomic_rcu_read(&qemu_logfile);
            if (logfile) {
                fprintf(logfile->fd, "tlbw[%03d]: ", index);
                if (!hex_dump_mmu_entry(logfile->fd, entry)) {
                    fprintf(logfile->fd, "invalid\n");
                }
            }
        }
        rcu_read_unlock();
    }
}

void hex_tlbw(CPUHexagonState *env, uint32_t index, uint64_t value)
{
    uint32_t myidx = fTLB_NONPOW2WRAP(fTLB_IDXMASK(index));
    bool old_entry_valid = GET_TLB_FIELD(env->hex_tlb->entries[myidx], PTE_V);
    if (old_entry_valid && hexagon_cpu_mmu_enabled(env)) {
        /* FIXME - Do we have to invalidate everything here? */
        CPUState *cs = CPU(hexagon_env_get_cpu(env));
        tlb_flush(cs);
    }
    env->hex_tlb->entries[myidx] = (value);
    hex_log_tlbw(myidx, value);
}

void hex_mmu_init(CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    env->hex_tlb = g_malloc0(sizeof(CPUHexagonTLBContext));
}

void hex_mmu_on(CPUHexagonState *env)
{
    CPUState *cs = CPU(hexagon_env_get_cpu(env));
    qemu_log_mask(CPU_LOG_MMU, "Hexagon MMU turned on!\n");
    tlb_flush(cs);

#if HEX_DEBUG
    hex_dump_mmu(env, stderr);
#endif
}

void hex_mmu_off(CPUHexagonState *env)
{
    CPUState *cs = CPU(hexagon_env_get_cpu(env));
    qemu_log_mask(CPU_LOG_MMU, "Hexagon MMU turned off!\n");
    tlb_flush(cs);
}

void hex_mmu_mode_change(CPUHexagonState *env)
{
    qemu_log_mask(CPU_LOG_MMU, "Hexagon mode change!\n");
    CPUState *cs = CPU(hexagon_env_get_cpu(env));
    tlb_flush(cs);
}

static inline bool hex_tlb_entry_match_noperm(uint64_t entry, uint32_t asid,
                                              target_ulong VA)
{
    if (GET_TLB_FIELD(entry, PTE_V)) {
        if (GET_TLB_FIELD(entry, PTE_G)) {
            /* Global entry - ingnore ASID */
        } else if (asid != NO_ASID) {
            uint32_t tlb_asid = GET_TLB_FIELD(entry, PTE_ASID);
            if (tlb_asid != asid) {
                return false;
            }
        }

        uint32_t page_start = hex_tlb_virt_addr(entry);
        uint32_t page_size = hex_tlb_page_size(entry);
        if (page_start <= VA && VA < page_start + page_size) {
            /* FIXME - Anything else we need to check? */
            return true;
        }
    }
    return false;
}

static inline void hex_tlb_entry_get_perm(uint64_t entry,
                                          MMUAccessType access_type,
                                          int mmu_idx, int *prot,
                                          int32_t *excp)
{
    bool perm_x = GET_TLB_FIELD(entry, PTE_X);
    bool perm_w = GET_TLB_FIELD(entry, PTE_W);
    bool perm_r = GET_TLB_FIELD(entry, PTE_R);
    bool perm_u = GET_TLB_FIELD(entry, PTE_U);
    bool user_idx = mmu_idx == MMU_USER_IDX;

    if (mmu_idx == MMU_KERNEL_IDX) {
        *prot = PAGE_VALID | PAGE_READ | PAGE_WRITE | PAGE_EXEC;
        return;
    }

    *prot = PAGE_VALID;
    switch (access_type) {
    case MMU_INST_FETCH:
        if (user_idx && !perm_u) {
            *excp = HEX_EXCP_FETCH_NO_UPAGE;
        } else if (!perm_x) {
            *excp = HEX_EXCP_FETCH_NO_XPAGE;
        }
        break;
    case MMU_DATA_LOAD:
        if (user_idx && !perm_u) {
            *excp = HEX_EXCP_PRIV_NO_UREAD;
        } else if (!perm_r) {
            *excp = HEX_EXCP_PRIV_NO_READ;
        }
        break;
    case MMU_DATA_STORE:
        if (user_idx && !perm_u) {
            *excp = HEX_EXCP_PRIV_NO_UWRITE;
        } else if (!perm_w) {
            *excp = HEX_EXCP_PRIV_NO_WRITE;
        }
        break;
    }

    if (!user_idx || perm_u) {
        if (perm_x) {
            *prot |= PAGE_EXEC;
        }
        if (perm_r) {
            *prot |= PAGE_READ;
        }
        if (perm_w) {
            *prot |= PAGE_WRITE;
        }
    }
}

static inline bool hex_tlb_entry_match(uint64_t entry, uint8_t asid, target_ulong VA,
                                       MMUAccessType access_type, hwaddr *PA,
                                       int *prot, int *size, int32_t *excp,
                                       int mmu_idx)
{
    if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
        hex_tlb_entry_get_perm(entry, access_type, mmu_idx, prot, excp);
        *PA = hex_tlb_phys_addr(entry);
        *size = hex_tlb_page_size(entry);
        return true;
    }
    return false;
}

bool hex_tlb_find_match(CPUHexagonState *env, target_ulong VA,
                        MMUAccessType access_type, hwaddr *PA, int *prot,
                        int *size, int32_t *excp, int mmu_idx)
{
    uint8_t asid = (env->sreg[HEX_SREG_SSR] & SSR_ASID) >> 8;
    int i;
    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        uint64_t entry = env->hex_tlb->entries[i];
        if (hex_tlb_entry_match(entry, asid, VA, access_type, PA, prot, size,
                                excp, mmu_idx)) {
            return true;
        }
    }
    return false;
}

static uint32_t hex_tlb_lookup_by_asid(CPUHexagonState *env, uint32_t asid,
                                       uint32_t VA, int is_tlbp)
{
    uint32_t not_found = 0x80000000;
    int i;

    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        uint64_t entry = env->hex_tlb->entries[i];
        if (hex_tlb_entry_match_noperm(entry, asid, VA)) {
            /* FIXME - Check for duplicate matches */
            qemu_log_mask(CPU_LOG_MMU, "%s: 0x%x, 0x%08x, %d => %d\n",
                          __func__, asid, VA, is_tlbp, i);
            return i;
        }
    }
    qemu_log_mask(CPU_LOG_MMU, "%s: 0x%x, 0x%08x, %d => NOT FOUND\n",
                  __func__, asid, VA, is_tlbp);
    return not_found;
}

uint32_t hex_tlb_lookup(CPUHexagonState *env, uint32_t ssr, uint32_t VA,
                        int is_tlbp)
{
    return hex_tlb_lookup_by_asid(env, (ssr & SSR_ASID) >> 8, VA, is_tlbp);
}

static bool hex_tlb_is_match(CPUHexagonState *env,
                             uint64_t entry1, uint64_t entry2,
                             bool consider_gbit)
{
    bool valid1 = GET_TLB_FIELD(entry1, PTE_V);
    bool valid2 = GET_TLB_FIELD(entry2, PTE_V);
    uint64_t vaddr1 = hex_tlb_virt_addr(entry1);
    uint64_t vaddr2 = hex_tlb_virt_addr(entry2);
    int size1 = hex_tlb_page_size(entry1);
    int size2 = hex_tlb_page_size(entry2);
    int asid1 = GET_TLB_FIELD(entry1, PTE_ASID);
    int asid2 = GET_TLB_FIELD(entry2, PTE_ASID);
    bool gbit1 = GET_TLB_FIELD(entry1, PTE_G);
    bool gbit2 = GET_TLB_FIELD(entry2, PTE_G);

    if (!valid1 || !valid2) {
        return false;
    }

    if (((vaddr1 <= vaddr2) && (vaddr2 < (vaddr1 + size1))) ||
        ((vaddr2 <= vaddr1) && (vaddr1 < (vaddr2 + size2)))) {
        if (asid1 == asid2) {
            return true;
        }
        if ((consider_gbit && gbit1) || gbit2) {
            return true;
        }
    }
    return false;
}

/*
 * Return codes:
 * 0 or positive             index of match
 * -1                        multiple matches
 * -2                        no match
 */
int hex_tlb_check_overlap(CPUHexagonState *env, uint64_t entry)
{
    int matches = 0;
    int last_match = 0;
    int i;

    for (i = 0; i < NUM_TLB_ENTRIES; i++) {
        if (hex_tlb_is_match(env, entry, env->hex_tlb->entries[i], false)) {
            matches++;
            last_match = i;
        }
    }

    if (matches == 1) {
        return last_match;
    }
    if (matches == 0) {
        return -2;
    }
    return 1;
}
