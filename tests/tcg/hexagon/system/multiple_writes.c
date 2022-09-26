/*
 *  Copyright(c) 2022 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
#define NO_DEFAULT_EVENT_HANDLES
#include "mmu.h"

#define HEX_CAUSE_REG_WRITE_CONFLICT 0x01d

void multiple_writes(void)
{
    asm volatile("p1 = cmp.eq(r1, r1)\n\t"
                 "p2 = p1\n\t"
                 "{\n\t"
                 "    if (p1) r3 = #1\n\t"
                 "    if (p2) r3 = #0\n\t"
                 "}\n\t"
                 "\n\t"
                 : : : "r3", "p1", "p2");
}

void my_err_handler_helper(uint32_t ssr)
{
    uint32_t cause = GET_FIELD(ssr, SSR_CAUSE);

    if (cause < 64) {
        *my_exceptions |= 1LL << cause;
    } else {
        *my_exceptions = cause;
    }

    switch (cause) {
    case HEX_CAUSE_REG_WRITE_CONFLICT:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(8);
        break;
    default:
        do_coredump();
        break;
    }
}

MAKE_ERR_HANDLER(my_err_handler, my_err_handler_helper)

int main()
{
    puts("Hexagon multiple writes to the same reg test");

    INSTALL_ERR_HANDLER(my_err_handler);
    multiple_writes();
    check(*my_exceptions, 1 << HEX_CAUSE_REG_WRITE_CONFLICT);

    puts(err ? "FAIL" : "PASS");
    return err;
}
