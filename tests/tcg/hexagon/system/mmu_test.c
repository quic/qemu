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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hexagon_standalone.h"

#define TARGET_PAGE_BITS            12
#define TLB_NOT_FOUND               (1 << 31)
#define ONE_MB                      (1 << 20)

#define fZXTN(N, M, VAL) ((VAL) & ((1LL << (N)) - 1))
#define fEXTRACTU_BITS(INREG, WIDTH, OFFSET) \
    (fZXTN(WIDTH, 32, (INREG >> OFFSET)))

#define fCONSTLL(A) A##LL
#define fINSERT_BITS(REG, WIDTH, OFFSET, INVAL) \
    do { \
        REG = ((REG) & ~(((fCONSTLL(1) << (WIDTH)) - 1) << (OFFSET))) | \
           (((INVAL) & ((fCONSTLL(1) << (WIDTH)) - 1)) << (OFFSET)); \
    } while (0)

#define GET_FIELD(ENTRY, FIELD) \
    fEXTRACTU_BITS(ENTRY, reg_field_info[FIELD].width, \
                   reg_field_info[FIELD].offset)
#define SET_FIELD(ENTRY, FIELD, VAL) \
    fINSERT_BITS(ENTRY, reg_field_info[FIELD].width, \
                 reg_field_info[FIELD].offset, (VAL))

typedef struct {
    const char *name;
    int offset;
    int width;
    const char *description;
} reg_field_t;

enum reg_fields_enum {
#define DEF_REG_FIELD(TAG, NAME, START, WIDTH, DESCRIPTION) \
    TAG,
#include "reg_fields_def.h"
    NUM_REG_FIELDS
#undef DEF_REG_FIELD
};

const reg_field_t reg_field_info[] = {
#define DEF_REG_FIELD(TAG, NAME, START, WIDTH, DESCRIPTION)    \
      {NAME, START, WIDTH, DESCRIPTION},

#include "reg_fields_def.h"

      {NULL, 0, 0}
#undef DEF_REG_FIELD
};

/*
 * PPD (physical page descriptor) is formed by putting the PTE_PA35 field
 * in the MSB of the PPD
 */
#define GET_PPD(ENTRY) \
    ((GET_FIELD((ENTRY), PTE_PPD) | \
     (GET_FIELD((ENTRY), PTE_PA35) << reg_field_info[PTE_PPD].width)))

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

static const uint8_t size_bits[] = {
    0x01,    /*    PGSIZE_4K     */
    0x02,    /*    PGSIZE_16K    */
    0x04,    /*    PGSIZE_64K    */
    0x08,    /*    PGSIZE_256K   */
    0x10,    /*    PGSIZE_1M     */
    0x20,    /*    PGSIZE_4M     */
    0x40,    /*    PGSIZE_16M    */
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

uint64_t encmask_2_mask[] = {
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
    return 1 << (TARGET_PAGE_BITS + 2 * hex_tlb_pgsize(entry));
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
    return GET_FIELD(entry, PTE_VPN) << TARGET_PAGE_BITS;
}

static void hex_dump_mmu_entry(uint64_t entry)
{
    printf("0x%016llx: ", entry);
    if (GET_FIELD(entry, PTE_V)) {
        uint64_t PA = hex_tlb_phys_addr(entry);
        uint64_t VA = hex_tlb_virt_addr(entry);
        printf("V:%lld G:%lld A1:%lld A0:%lld",
                 GET_FIELD(entry, PTE_V),
                 GET_FIELD(entry, PTE_G),
                 GET_FIELD(entry, PTE_ATR1),
                 GET_FIELD(entry, PTE_ATR0));
        printf(" ASID:0x%02llx VA:0x%08llx",
                GET_FIELD(entry, PTE_ASID),
                VA);
        printf(" X:%lld W:%lld R:%lld U:%lld C:%lld",
                GET_FIELD(entry, PTE_X),
                GET_FIELD(entry, PTE_W),
                GET_FIELD(entry, PTE_R),
                GET_FIELD(entry, PTE_U),
                GET_FIELD(entry, PTE_C));
        printf(" PA:0x%09llx SZ:%s (0x%lx)",
                PA,
                pgsize_str[hex_tlb_pgsize(entry)],
                hex_tlb_page_size(entry));
        printf("\n");
    } else {
        printf("invalid\n");
    }
}

uint64_t create_mmu_entry(uint8_t G, uint8_t A0, uint8_t A1, uint8_t ASID,
                          uint32_t VA, uint8_t X, int8_t W, uint8_t R,
                          uint8_t C, uint64_t PA, tlb_pgsize_t SZ)
{
    uint64_t entry = 0;
    SET_FIELD(entry, PTE_V, 1);
    SET_FIELD(entry, PTE_G, G);
    SET_FIELD(entry, PTE_ATR0, A0);
    SET_FIELD(entry, PTE_ATR1, A1);
    SET_FIELD(entry, PTE_ASID, ASID);
    SET_FIELD(entry, PTE_VPN, VA >> TARGET_PAGE_BITS);
    SET_FIELD(entry, PTE_X, X);
    SET_FIELD(entry, PTE_W, W);
    SET_FIELD(entry, PTE_R, R);
    SET_FIELD(entry, PTE_C, C);
    SET_FIELD(entry, PTE_PA35, (PA >> (TARGET_PAGE_BITS + 35)) & 1);
    SET_FIELD(entry, PTE_PPD, ((PA >> (TARGET_PAGE_BITS - 1))));
    entry |= size_bits[SZ];
    return entry;
}

uint64_t tlbr(uint32_t i)
{
    uint64_t ret;
    asm volatile ("%0 = tlbr(%1)\n\t" : "=r"(ret) : "r"(i));
    return ret;
}

uint32_t tlbp(uint32_t asid, uint32_t VA)
{
    uint32_t x = ((asid & 0x7f) << 20) | ((VA >> 12) & 0xfffff);
    uint32_t ret;
    asm volatile ("%0 = tlbp(%1)\n\t" : "=r"(ret) : "r"(x));
    return ret;
}

uint32_t ctlbw(uint64_t entry, uint32_t idx)
{
    uint32_t ret;
    asm volatile ("%0 = ctlbw(%1, %2)\n\t" : "=r"(ret) : "r"(entry), "r"(idx));
    return ret;
}

void tlbw(uint64_t entry, uint32_t idx)
{
    asm volatile ("tlbw(%0, %1)\n\t" :: "r"(entry), "r"(idx));
}

uint32_t tlboc(uint64_t entry)
{
    uint32_t ret;
    asm volatile ("%0 = tlboc(%1)\n\t" : "=r"(ret) : "r"(entry));
    return ret;
}

void tlbinvasid(uint32_t entry_hi)
{
    asm volatile ("tlbinvasid(%0)\n\t" :: "r"(entry_hi));
}

void enter_user_mode(void)
{
    asm volatile ("r0 = ssr\n\t"
                  "r0 = clrbit(r0, #17) // EX\n\t"
                  "r0 = setbit(r0, #16) // UM\n\t"
                  "r0 = clrbit(r0, #19) // GM\n\t"
                  "ssr = r0\n\t" : : : "r0");
}

void do_coredump(void)
{
    asm volatile("r0 = #2\n\t"
                 "stid = r0\n\t"
                 "jump __coredump\n\t" : : : "r0");
}

uint32_t get_ssr(void)
{
    uint32_t ret;
    asm volatile ("%0 = ssr\n\t" : "=r"(ret));
    return ret;
}

void set_ssr(uint32_t new_ssr)
{
    asm volatile ("ssr = %0\n\t" :: "r"(new_ssr));
}

void set_asid(uint32_t asid)
{
    uint32_t ssr = get_ssr();
    SET_FIELD(ssr, SSR_ASID, asid);
    set_ssr(ssr);
}

int err;
uint64_t my_exceptions;

/* volatile because it is written through different MMU mappings */
typedef volatile int mmu_variable;
mmu_variable data = 0xdeadbeef;

#define check(N, EXPECT) \
    if (N != EXPECT) { \
        printf("ERROR at %d: 0x%08lx != 0x%08lx\n", __LINE__, \
               (uint32_t)(N), (uint32_t)(EXPECT)); \
        err++; \
    }

#define check64(N, EXPECT) \
    if (N != EXPECT) { \
        printf("ERROR at %d: 0x%016llx != 0x%016llx\n", __LINE__, \
               (N), (EXPECT)); \
        err++; \
    }

#define check_not(N, EXPECT) \
    if (N == EXPECT) { \
        printf("ERROR: 0x%08x == 0x%08x\n", N, EXPECT); \
        err++; \
    }

typedef int (*func_t)(void);
/* volatile because it will be invoked via different MMU mappings */
typedef volatile func_t mmu_func_t;
int foo(void)
{
    int ret;
    asm volatile ("%0 = pc\n\t" : "=r"(ret));
    return ret;
}

enum {
    TLB_U = (1 << 0),
    TLB_R = (1 << 1),
    TLB_W = (1 << 2),
    TLB_X = (1 << 3),
};

uint32_t add_trans_pgsize(uint32_t page_size_bits)
{
    switch (page_size_bits) {
    case 12:                    /* 4KB   */
        return 1;
    case 14:                    /* 16KB  */
        return 2;
    case 16:                    /* 64KB  */
        return 4;
    case 18:                    /* 256KB */
        return 8;
    case 20:                    /* 1MB   */
        return 16;
    case 22:                    /* 4MB   */
        return 32;
    case 24:                    /* 16MB  */
        return 64;
    default:
        return 1;
    }
}

void do_add_trans(int index, void *va, uint64_t pa, unsigned int page_size_bits,
                  unsigned int xwru, unsigned int cccc, unsigned int asid,
                  unsigned int aa, unsigned int vg)
{
    if (add_translation_extended(index, va, pa,
                                 add_trans_pgsize(page_size_bits),
                                 xwru, cccc, asid, aa, vg)) {
        puts("ERROR: add_translation_extended failed");
        err++;
    }
}


void test_page_size(const char *name, uint32_t page_size_bits)
{
#ifdef DEBUG
    printf("Testing %s page size\n", name);
#endif
    data = 0xdeadbeef;

    uint32_t page_size = 1 << page_size_bits;
    uint32_t page_align = ~(page_size - 1);
    uint32_t addr = (uint32_t)&data;
    uint32_t page = addr & page_align;
    uint32_t offset = page_size <= ONE_MB ? 5 * ONE_MB : page_size;
    uint32_t new_page = page + offset;
    uint32_t new_addr = addr + offset;
    do_add_trans(1, (void *)new_page, page, page_size_bits,
                 TLB_X | TLB_W | TLB_R | TLB_U,
                 0, 0, 0, 0x3);
#ifdef DEBUG
    uint64_t entry = tlbr(1);
    printf("new_addr: 0x%lx\n", new_addr);
    printf("----> ");
    hex_dump_mmu_entry(entry);
    printf("tlbp(0x%08lx) = 0x%lx\n", addr, tlbp(0, addr));
    printf("tlbp(0x%08lx) = 0x%lx\n", new_addr, tlbp(0, new_addr));
#endif
    check(tlbp(0, new_addr), 1);

    /* Load through the new VA */
    check(*(mmu_variable *)new_addr, 0xdeadbeef);

    /* Store through the new VA */
    *(mmu_variable *)new_addr = 0xcafebabe;
    check(data, 0xcafebabe);

    /* Clear out this entry */
    do_add_trans(1, (void *)new_page, page, page_size_bits,
                 TLB_X | TLB_W | TLB_R | TLB_U,
                 0, 0, 0, 0x1);
    check(tlbp(0, new_addr), TLB_NOT_FOUND);

    mmu_func_t f = foo;
    addr = (uint32_t)f;
    page = addr & page_align;
    offset = page_size <= ONE_MB ? 6 * ONE_MB : page_size;
    new_page = page + offset;
    new_addr = addr + offset;
    do_add_trans(2, (void *)new_page, page, page_size_bits,
                 TLB_X | TLB_W | TLB_R | TLB_U,
                 0, 0, 0, 0x3);
#ifdef DEBUG
    entry = tlbr(2);
    printf("new_addr: 0x%lx\n", new_addr);
    printf("====> ");
    hex_dump_mmu_entry(entry);
    printf("tlbp(0x%08lx) = 0x%lx\n", new_addr, tlbp(0, new_addr));
#endif
    check(tlbp(0, new_addr), 2);

    mmu_func_t new_f = (mmu_func_t)new_addr;
    check((new_f()), (int)new_addr);

    /* Clear out this entry */
    do_add_trans(2, (void *)new_page, page, page_size_bits,
                 TLB_X | TLB_W | TLB_R | TLB_U,
                 0, 0, 0, 0x1);
    check(tlbp(0, new_addr), TLB_NOT_FOUND);
}

#define HEX_CAUSE_FETCH_NO_XPAGE                  0x011
#define HEX_CAUSE_FETCH_NO_UPAGE                  0x012
#define HEX_CAUSE_PRIV_NO_READ                    0x022
#define HEX_CAUSE_PRIV_NO_WRITE                   0x023
#define HEX_CAUSE_PRIV_NO_UREAD                   0x024
#define HEX_CAUSE_PRIV_NO_UWRITE                  0x025
#define HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH       0x44

#define READ_TLB                                 1
#define READ_USER_TLB                            2
#define WRITE_TLB                                3
#define WRITE_USER_TLB                           4
#define EXEC_TLB                                 5

void my_event_handler_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);
    uint64_t entry;

    if (cause < 64) {
        my_exceptions |= 1LL << cause;
    } else {
        my_exceptions = cause;
    }

    switch (cause) {
    case HEX_CAUSE_FETCH_NO_XPAGE:
        entry = tlbr(EXEC_TLB);
        SET_FIELD(entry, PTE_X, 1);
        tlbw(entry, EXEC_TLB);
        break;
    case HEX_CAUSE_FETCH_NO_UPAGE:
        entry = tlbr(EXEC_TLB);
        SET_FIELD(entry, PTE_U, 1);
        tlbw(entry, EXEC_TLB);
        break;
    case HEX_CAUSE_PRIV_NO_READ:
        entry = tlbr(READ_TLB);
        SET_FIELD(entry, PTE_R, 1);
        tlbw(entry, READ_TLB);
        break;
    case HEX_CAUSE_PRIV_NO_WRITE:
        entry = tlbr(WRITE_TLB);
        SET_FIELD(entry, PTE_W, 1);
        tlbw(entry, WRITE_TLB);
        break;
    case HEX_CAUSE_PRIV_NO_UREAD:
        entry = tlbr(READ_USER_TLB);
        SET_FIELD(entry, PTE_U, 1);
        tlbw(entry, READ_USER_TLB);
        break;
    case HEX_CAUSE_PRIV_NO_UWRITE:
        entry = tlbr(WRITE_USER_TLB);
        SET_FIELD(entry, PTE_U, 1);
        tlbw(entry, WRITE_USER_TLB);
        break;
    case HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH:
        break;
    default:
        do_coredump();
        break;
    }
}

void my_event_handler(void)
{
    asm volatile("crswap(sp, sgp0)\n\t"
                 "memd(sp++#8) = r1:0\n\t"
                 "memd(sp++#8) = r3:2\n\t"
                 "memd(sp++#8) = r5:4\n\t"
                 "memd(sp++#8) = r7:6\n\t"
                 "memd(sp++#8) = r9:8\n\t"
                 "memd(sp++#8) = r11:10\n\t"
                 "memd(sp++#8) = r13:12\n\t"
                 "memd(sp++#8) = r15:14\n\t"
                 "memd(sp++#8) = r17:16\n\t"
                 "memd(sp++#8) = r19:18\n\t"
                 "memd(sp++#8) = r21:20\n\t"
                 "memd(sp++#8) = r23:22\n\t"
                 "memd(sp++#8) = r25:24\n\t"
                 "memd(sp++#8) = r27:26\n\t"
                 "memd(sp++#8) = r31:30\n\t"
                 "r0 = ssr\n\t"
                 "call my_event_handler_helper\n\t"
                 "sp = add(sp, #-8)\n\t"
                 "r31:30 = memd(sp++#-8)\n\t"
                 "r27:26 = memd(sp++#-8)\n\t"
                 "r25:24 = memd(sp++#-8)\n\t"
                 "r23:22 = memd(sp++#-8)\n\t"
                 "r21:20 = memd(sp++#-8)\n\t"
                 "r19:18 = memd(sp++#-8)\n\t"
                 "r17:16 = memd(sp++#-8)\n\t"
                 "r15:14 = memd(sp++#-8)\n\t"
                 "r13:12 = memd(sp++#-8)\n\t"
                 "r11:10 = memd(sp++#-8)\n\t"
                 "r9:8 = memd(sp++#-8)\n\t"
                 "r7:6 = memd(sp++#-8)\n\t"
                 "r5:4 = memd(sp++#-8)\n\t"
                 "r3:2 = memd(sp++#-8)\n\t"
                 "r1:0 = memd(sp)\n\t"
                 "crswap(sp, sgp0);\n\t"
                 "rte\n\t");
}

void goto_my_event_handler(void)
{
    asm volatile("r0 = ##my_event_handler\n\t"
                 "jumpr r0\n\t"
                 : : : "r0");
}

void print_exceptions(uint64_t excp)
{
    int i;
    printf("exceptions (0x%llx):", excp);
    for (i = 0; i <= 64; i++) {
        if (excp & (1LL << i)) {
            printf(" 0x%x", i);
        }
    }
    printf("\n");
}

void test_permissions(void)
{
    my_exceptions = 0;

    uint32_t page_size_bits = 12;
    uint32_t page_size = 1 << page_size_bits;
    uint32_t page_align = ~(page_size - 1);
    uint32_t data_offset;
    uint32_t new_data_page;
    unsigned int data_perm;

    data = 0xdeadbeef;

    uint32_t data_addr = (uint32_t)&data;
    uint32_t data_page = data_addr & page_align;


    data_offset = READ_TLB * ONE_MB;
    new_data_page = data_page + data_offset;
    uint32_t read_data_addr = data_addr + data_offset;
    data_perm = TLB_X | TLB_W | TLB_U;
    do_add_trans(READ_TLB, (void *)new_data_page, data_page,
                 page_size_bits, data_perm, 0, 0, 0, 0x3);
    check(tlbp(0, read_data_addr), READ_TLB);

    data_offset = READ_USER_TLB * ONE_MB;
    new_data_page = data_page + data_offset;
    uint32_t read_user_data_addr = data_addr + data_offset;
    data_perm = TLB_X | TLB_W | TLB_R;
    do_add_trans(READ_USER_TLB, (void *)new_data_page, data_page,
                 page_size_bits, data_perm, 0, 0, 0, 0x3);
    check(tlbp(0, read_user_data_addr), READ_USER_TLB);

    data_offset = WRITE_TLB * ONE_MB;
    new_data_page = data_page + data_offset;
    uint32_t write_data_addr = data_addr + data_offset;
    data_perm = TLB_X | TLB_R | TLB_U;
    do_add_trans(WRITE_TLB, (void *)new_data_page, data_page,
                 page_size_bits, data_perm, 0, 0, 0, 0x3);
    check(tlbp(0, write_data_addr), WRITE_TLB);

    data_offset = WRITE_USER_TLB * ONE_MB;
    new_data_page = data_page + data_offset;
    uint32_t write_user_data_addr = data_addr + data_offset;
    data_perm = TLB_X | TLB_R | TLB_W;
    do_add_trans(WRITE_USER_TLB, (void *)new_data_page, data_page,
                 page_size_bits, data_perm, 0, 0, 0, 0x3);
    check(tlbp(0, write_user_data_addr), WRITE_USER_TLB);


    mmu_func_t f = foo;
    uint32_t func_addr = (uint32_t)f;
    uint32_t func_page = func_addr & page_align;
    uint32_t func_offset = EXEC_TLB * ONE_MB;
    uint32_t new_func_page = func_page + func_offset;
    uint32_t exec_addr = func_addr + func_offset;

    unsigned int func_perm = TLB_W | TLB_R;
    do_add_trans(EXEC_TLB, (void *)new_func_page, func_page,
                 page_size_bits, func_perm, 0, 0, 0, 0x3);
    check(tlbp(0, exec_addr), EXEC_TLB);

    enter_user_mode();

    /* Load through the new VA */
    check(*(mmu_variable *)read_data_addr, 0xdeadbeef);
    check(*(mmu_variable *)read_user_data_addr, 0xdeadbeef);

    /* Store through the new VA */
    /* Make sure the replay happens before anything else */
    mmu_variable *p = (mmu_variable *)write_data_addr;
    asm volatile("memw(%0++#4) = %1\n\t"
                 : "+r"(p) : "r"(0xc0ffee) : "memory");
    check(data, 0xc0ffee);
    check((uint32_t)p, (uint32_t)write_data_addr + 4);

    *(mmu_variable *)write_user_data_addr = 0xc0ffee;
    check(data, 0xc0ffee);

    mmu_func_t new_f = (mmu_func_t)exec_addr;
    check((new_f()), (int)exec_addr);

    uint64_t expected_exceptions = 0;
    expected_exceptions |= (1LL << HEX_CAUSE_FETCH_NO_XPAGE);
    expected_exceptions |= (1LL << HEX_CAUSE_FETCH_NO_UPAGE);
    expected_exceptions |= (1LL << HEX_CAUSE_PRIV_NO_READ);
    expected_exceptions |= (1LL << HEX_CAUSE_PRIV_NO_WRITE);
    expected_exceptions |= (1LL << HEX_CAUSE_PRIV_NO_UREAD);
    expected_exceptions |= (1LL << HEX_CAUSE_PRIV_NO_UWRITE);

#if DEBUG
    print_exceptions(my_exceptions);
    print_exceptions(expected_exceptions);
#endif
    check64(my_exceptions, expected_exceptions);
}

void test_tlboc_ctlbw(void)
{
    uint32_t page_size_bits = 20;               /* 1MB page size */
    uint32_t page_size = 1 << page_size_bits;
    uint32_t page_align = ~(page_size - 1);

    data = 0xdeadbeef;

    uint32_t addr = (uint32_t)&data;
    uint32_t page = addr & page_align;
    uint32_t offset = page_size <= ONE_MB ? 5 * ONE_MB : page_size;
    uint32_t new_page = page + offset;
    uint32_t new_addr = addr + offset;
    unsigned int data_perm = TLB_X | TLB_W | TLB_R | TLB_U;
    do_add_trans(1, (void *)new_page, page, page_size_bits,
                 data_perm, 0, 0, 0, 0x3);
    check(tlbp(0, new_addr), 1);

    /* Check an entry that overlaps with the one we just created */
    uint64_t entry =
        create_mmu_entry(1, 0, 0, 0, new_page, 1, 1, 1, 7, page, PGSIZE_4K);
    check(tlboc(entry), 1);
    check(ctlbw(entry, 2), 0x1);

    /* Create an entry that does not overlap with the one we just created */
    entry = create_mmu_entry(1, 0, 0, 0, new_page + ONE_MB, 1, 1, 1, 7, page,
                             PGSIZE_4K);
    check(tlboc(entry), TLB_NOT_FOUND);
    check(ctlbw(entry, 2), TLB_NOT_FOUND);

    /* Clear the TLB entries */
    do_add_trans(1, (void *)new_page, page, page_size_bits,
                 TLB_X | TLB_W | TLB_R | TLB_U,
                 0, 0, 0, 0x1);
    check(tlbp(0, new_addr), TLB_NOT_FOUND);
    do_add_trans(2, (void *)(new_page + ONE_MB), page, page_size_bits,
                 TLB_X | TLB_W | TLB_R | TLB_U,
                 0, 0, 0, 0x1);
    check(tlbp(0, (new_addr + ONE_MB)), TLB_NOT_FOUND);
}

void test_asids(void)
{
    uint32_t page_size_bits = 12;               /* 4KB page size */
    uint32_t page_size = 1 << page_size_bits;
    uint32_t page_align = ~(page_size - 1);

    uint32_t addr = (uint32_t)&data;
    uint32_t page = addr & page_align;
    uint32_t offset = 5 * ONE_MB;
    uint32_t new_addr = addr + offset;
    uint32_t new_page = page + offset;

    uint64_t entry =
        create_mmu_entry(0, 0, 0, 1, new_page, 1, 1, 1, 7, page, PGSIZE_4K);
    /*
     * Create a TLB entry for ASID=1
     * Write it at index 1
     * Check that it is present
     * Invalidate the ASID
     * Check that it is not found
     */
    tlbw(entry, 1);
    check(tlboc(entry), 1);
    tlbinvasid(entry >> 32);
    check(tlboc(entry), TLB_NOT_FOUND);

    /*
     * Re-install the entry
     * Put ourselves in ASID=1
     * Do a load and a store
     */
    data = 0xdeadbeef;
    tlbw(entry, 1);
    set_asid(1);
    check(*(mmu_variable *)new_addr, 0xdeadbeef);
    *(mmu_variable *)new_addr = 0xcafebabe;
    check(data, 0xcafebabe);

    /*
     * Make sure a load from ASID 2 gets a different value.
     * The standalone runtime will create a VA==PA entry on
     * a TLB miss, so the load will be reading from uninitialized
     * memory.
     */
    set_asid(2);
    data = 0xdeadbeef;
    check_not(*(mmu_variable *)new_addr, 0xdeadbeef);

    /*
     * Invalidate the ASID and make sure a loads from ASID 1
     * gets a different value.
     */
    tlbinvasid(entry >> 32);
    set_asid(1);
    data = 0xcafebabe;
    check_not(*(mmu_variable *)new_addr, 0xcafebabe);
}

void test_multi_tlb(void)
{
    uint32_t page_size_bits = 12;               /* 4KB page size */
    uint32_t page_size = 1 << page_size_bits;
    uint32_t page_align = ~(page_size - 1);

    uint32_t addr = (uint32_t)&data;
    uint32_t page = addr & page_align;
    uint32_t offset = 7 * ONE_MB;
    uint32_t new_addr = addr + offset;
    uint32_t new_page = page + offset;

    uint64_t entry =
        create_mmu_entry(0, 0, 0, 1, new_page, 1, 1, 1, 7, page, PGSIZE_4K);
    /*
     * Write the index at index 1 and 2
     * The second tlbp should raise an exception
     */
    my_exceptions = 0;
    set_asid(1);
    tlbinvasid(1);
    tlbw(entry, 1);
    check(tlboc(entry), 1);
    check(tlbp(1, new_addr), 1);
    tlbw(entry, 2);
    check(tlboc(entry), -1);
    check(tlbp(1, new_addr), 1);
    check64(my_exceptions, (uint64_t)HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH);

    /* Clear the TLB entries */
    SET_FIELD(entry, PTE_V, 0);
    tlbw(entry, 1);
    tlbw(entry, 2);
}

int main()
{
    /*
     * Install our own privelege and nmi exception handlers
     * The normal behavior is to coredump
     * NOTE: Using the hard-coded addresses is a brutal hack,
     * but the symbol names aren't exported from the standalone
     * runtime.
     */
#if (__clang_major__ == 10)
    memcpy((void *)0x0ffc, goto_my_event_handler, 12);
    memcpy((void *)0x0ff0, goto_my_event_handler, 12);
#else
    memcpy((void *)0x1000, goto_my_event_handler, 12);
    memcpy((void *)0x0ff4, goto_my_event_handler, 12);
#endif

    puts("Hexagon MMU test");

#ifdef DEBUG
    uint32_t VA;
    uint64_t PA;
    uint64_t entry;

    printf("Testing create_mmu_entry\n");
    VA = 0x00001000;
    PA = 0x00003000;
    entry = create_mmu_entry(1, 0, 0, 0, VA, 1, 1, 1, 7, PA, PGSIZE_4K);
    hex_dump_mmu_entry(entry);

    VA = 0x00004000;
    PA = 0x0000c000;
    entry = create_mmu_entry(1, 0, 0, 0, VA, 1, 1, 1, 7, PA, PGSIZE_16K);
    hex_dump_mmu_entry(entry);

    VA = 0x00010000;
    PA = 0x00030000;
    entry = create_mmu_entry(1, 0, 0, 0, VA, 1, 1, 1, 7, PA, PGSIZE_64K);
    hex_dump_mmu_entry(entry);

    VA = 0x00040000;
    PA = 0x000c0000;
    entry = create_mmu_entry(1, 0, 0, 0, VA, 1, 1, 1, 7, PA, PGSIZE_256K);
    hex_dump_mmu_entry(entry);

    VA = 0x00100000;
    PA = 0x00300000;
    entry = create_mmu_entry(1, 0, 0, 0, VA, 1, 1, 1, 7, PA, PGSIZE_1M);
    hex_dump_mmu_entry(entry);

    VA = 0x00400000;
    PA = 0x00c00000;
    entry = create_mmu_entry(1, 0, 0, 0, VA, 1, 1, 1, 7, PA, PGSIZE_4M);
    hex_dump_mmu_entry(entry);

    VA = 0x01000000;
    PA = 0x03000000;
    entry = create_mmu_entry(1, 0, 0, 0, VA, 1, 1, 1, 7, PA, PGSIZE_16M);
    hex_dump_mmu_entry(entry);
#endif

    test_page_size("4KB",   12);
    test_page_size("16KB",  14);
    test_page_size("64KB",  16);
    test_page_size("256KB", 18);
    test_page_size("1MB",   20);
    test_page_size("4MB",   22);
    test_page_size("16MB",  24);

    test_tlboc_ctlbw();
    test_asids();
    test_multi_tlb();

    /* This has to be last because it puts us in user mode */
    test_permissions();

    puts(err ? "FAIL" : "PASS");
    return err;
}
