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
MY_EVENT_HANDLE(my_event_handle_nmi,              my_event_handle_nmi_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_error,       HANDLE_ERROR_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)

void test_multi_tlb(void)
{
    uint32_t addr = (uint32_t)&data;
    uint32_t page = page_start(addr, TARGET_PAGE_BITS);
    uint32_t offset = ONE_MB;
    uint32_t new_addr = addr + offset;
    uint32_t new_page = page + offset;
    uint64_t entry =
        create_mmu_entry(0, 0, 0, 1, new_page, 1, 1, 1, 0, 7, page, PGSIZE_4K);
    exception_vector expected_exceptions;

    install_my_event_vectors();

    /*
     * Write the entry at index 1 and 2
     * The second tlbp should raise an exception
     */
    clear_exception_vector(my_exceptions);
    clear_exception_vector(expected_exceptions);
    set_exception_vector_bit(expected_exceptions,
                             HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH);

    set_asid(1);
    tlbinvasid(1);
    tlbw(entry, 1);
    check32(tlboc(entry), 1);
    check32(tlbp(1, new_addr), 1);
    tlbw(entry, 2);
    check32(tlboc(entry), 0xffffffff);
    check32(tlbp(1, new_addr), 1);
    check_exception_vector(my_exceptions, expected_exceptions);

    /* Clear the TLB entries */
    remove_trans(1);
    remove_trans(2);
}

int main()
{
    puts("Hexagon MMU multi TLB test");

    test_multi_tlb();

    printf("%s : %s\n", ((err) ? "FAIL" : "PASS"), __FILENAME__);
    return err;
}
