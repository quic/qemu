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

void inc_elr(uint32_t inc)
{

    asm volatile ("r1 = %0\n\t"
                  "r2 = elr\n\t"
                  "r1 = add(r2, r1)\n\t"
                  "elr = r1\n\t"
                  :  : "r"(inc) : "r1");
}

void invalid_opcode(void)
{
    /* nops pads are a workaround for QTOOL-54399 */
    asm volatile ("nop");
    asm volatile (".word 0x6fffdffc\n\t");
    asm volatile ("nop");
}

void do_coredump(void)
{
    asm volatile("r0 = #2\n\t"
                 "stid = r0\n\t"
                 "jump __coredump\n\t" : : : "r0");
}

int err;
uint64_t my_exceptions;


#define check(N, EXPECT) \
    if (N != EXPECT) { \
        printf("ERROR at %d: 0x%08lx != 0x%08lx\n", __LINE__, \
               (uint32_t)(N), (uint32_t)(EXPECT)); \
        err++; \
    }

#define HEX_CAUSE_INVALID_OPCODE                  0x015

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
    case HEX_CAUSE_INVALID_OPCODE:
        /* We don't want to replay this instruction, just note the exception */
        inc_elr(4);
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

int main()
{
    /*
     * Install our own exception handlers
     * The normal behavior is to coredump
     * NOTE: Using the hard-coded addresses is a brutal hack,
     * but the symbol names aren't exported from the standalone
     * runtime.
     */
    memcpy((void *)0x1000, goto_my_event_handler, 12);

    puts("Hexagon invalid opcode test");

    my_exceptions = 0;
    invalid_opcode();
    check(my_exceptions, 1 << HEX_CAUSE_INVALID_OPCODE);

    puts(err ? "FAIL" : "PASS");
    return err;
}
