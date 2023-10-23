/*
 *  Copyright(c) 2022-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#define HEX_CAUSE_PRIV_USER_NO_SINS         0x1b
#define HEX_CAUSE_PRIV_USER_NO_GINS         0x1a

static bool priv_exception_found;
static bool guest_exception_found;

static inline void increment_elr(int x)
{
    asm volatile("r7 = elr\n\t"
                 "r7 = add(r7, %0)\n\t"
                 "elr = r7\n\t"
                 : : "r"(x) : "r7");
}

static inline void enter_kernel_mode(void)
{
    asm volatile ("r0 = ssr\n\t"
                  "r0 = setbit(r0, #17) // EX\n\t"
                  "r0 = clrbit(r0, #16) // UM\n\t"
                  "r0 = setbit(r0, #19) // GM\n\t"
                  "ssr = r0\n\t" : : : "r0");
}

void checkforpriv_event_handle_error_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    /*
     * Once we have handled the exceptions we are looking for,
     * go back to kernel mode.
     * We need this because some of the subsequent functions
     * rely on this.
     */
    if (priv_exception_found && guest_exception_found &&
        (cause == HEX_CAUSE_PRIV_USER_NO_SINS ||
         cause == HEX_CAUSE_PRIV_USER_NO_GINS)) {
        enter_kernel_mode();
        return;
    }

    if (cause == HEX_CAUSE_PRIV_USER_NO_SINS) {
        priv_exception_found = true;
        increment_elr(4);
    } else if (cause == HEX_CAUSE_PRIV_USER_NO_GINS) {
        guest_exception_found = true;
        increment_elr(4);
    } else {
        do_coredump();
    }
}

/* Set up the event handlers */
MY_EVENT_HANDLE(my_event_handle_error, checkforpriv_event_handle_error_helper)

DEFAULT_EVENT_HANDLE(my_event_handle_nmi,         HANDLE_NMI_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissrw,   HANDLE_TLBMISSRW_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_tlbmissx,    HANDLE_TLBMISSX_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_reset,       HANDLE_RESET_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_rsvd,        HANDLE_RSVD_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap0,       HANDLE_TRAP0_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_trap1,       HANDLE_TRAP1_OFFSET)
DEFAULT_EVENT_HANDLE(my_event_handle_int,         HANDLE_INT_OFFSET)


int main()
{
    puts("Hexagon supervisor/guest permissions test");

    install_my_event_vectors();
    enter_user_mode();

    asm volatile("rte\n\t");
    check32(priv_exception_found, true);

    asm volatile("gelr = r0\n\t");
    check32(guest_exception_found, true);

    printf("%s : %s\n", ((err) ? "FAIL" : "PASS"), __FILENAME__);
    return err;
}
