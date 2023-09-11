/*
 *  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#include "filename.h"

#define DEBUG        0

#include "mmu.h"

/* Set up the event handlers */
MY_EVENT_HANDLE(my_event_handle_error,            my_event_handle_error_helper)
MY_EVENT_HANDLE(my_event_handle_nmi,              my_event_handle_nmi_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)

void test_page_size(tlb_pgsize_t pgsize, uint32_t page_size_bits)
{
#if DEBUG
    printf("Testing %s page size\n", pgsize_str[pgsize]);
#endif
    uint32_t page_size = 1 << page_size_bits;
    uint32_t addr = (uint32_t)&data;
    uint32_t page = page_start(addr, page_size_bits);
    uint32_t offset = page_size <= ONE_MB ? FIVE_MB : page_size;
    uint32_t new_page = page + offset;
    uint32_t new_addr = addr + offset;
    mmu_func_t f = func_return_pc;
    mmu_func_t new_f;

    data = 0xdeadbeef;

    add_trans(1, new_page, page, pgsize,
              TLB_X | TLB_W | TLB_R | TLB_U,
              0, 1, 1);
#if DEBUG
    uint64_t entry = tlbr(1);
    printf("new_addr: 0x%lx\n", new_addr);
    printf("----> ");
    hex_dump_mmu_entry(entry);
    printf("tlbp(0x%08lx) = 0x%lx\n", addr, tlbp(0, addr));
    printf("tlbp(0x%08lx) = 0x%lx\n", new_addr, tlbp(0, new_addr));
#endif
    check32(tlbp(0, new_addr), 1);

    /* Load through the new VA */
    check32(*(mmu_variable *)new_addr, 0xdeadbeef);

    /* Store through the new VA */
    *(mmu_variable *)new_addr = 0xcafebabe;
    check32(data, 0xcafebabe);

    /* Clear out this entry */
    remove_trans(1);
    check32(tlbp(0, new_addr), TLB_NOT_FOUND);

    /* Set up a mapping for function execution */
    addr = (uint32_t)f;
    page = page_start(addr, page_size_bits);
    offset = page_size <= ONE_MB ? FIVE_MB : page_size;
    new_page = page + offset;
    new_addr = addr + offset;
    add_trans(2, new_page, page, pgsize,
              TLB_X | TLB_W | TLB_R | TLB_U,
              0, 1, 1);
#if DEBUG
    entry = tlbr(2);
    printf("new_addr: 0x%lx\n", new_addr);
    printf("====> ");
    hex_dump_mmu_entry(entry);
    printf("tlbp(0x%08lx) = 0x%lx\n", new_addr, tlbp(0, new_addr));
#endif
    check32(tlbp(0, new_addr), 2);

    /*
     * Call the function at the new address
     * It will return it's PC, which should be the new address
     */
    new_f = (mmu_func_t)new_addr;
    check32((new_f()), (int)new_addr);

    /* Clear out this entry */
    remove_trans(2);
    check32(tlbp(0, new_addr), TLB_NOT_FOUND);
}

int main()
{
    puts("Hexagon MMU page size test");

    test_page_size(PGSIZE_4K,   12);
    test_page_size(PGSIZE_16K,  14);
    test_page_size(PGSIZE_64K,  16);
    test_page_size(PGSIZE_256K, 18);
    test_page_size(PGSIZE_1M,   20);
    test_page_size(PGSIZE_4M,   22);
    test_page_size(PGSIZE_16M,  24);
    test_page_size(PGSIZE_64M,  26);
    test_page_size(PGSIZE_256M, 28);
    test_page_size(PGSIZE_1G,   30);

    printf("%s : %s\n", ((err) ? "FAIL" : "PASS"), __FILENAME__);
    return err;
}
