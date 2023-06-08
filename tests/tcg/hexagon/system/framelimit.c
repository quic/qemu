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

#include <stdio.h>
#include <string.h>

#define ELTS 512
struct bar {
    int el_0[ELTS];
};

int foo(int x, struct bar *B)
{
    B->el_0[x % ELTS] = x * 12;
    if (x > -1) {
        return foo((x + 1) * B->el_0[x % ELTS], B);
    }
    return (x + 1) * x;
}

unsigned int get_stack_ptr(void)
{
    unsigned int retval;
    asm("%0 = r29\n" : "=r"(retval));
    return retval;
}

void set_framelimit(unsigned int x)
{
    asm volatile("r10 = %0\n\t"
                 "framelimit = r10\n\t"
                 :: "r"(x) : "r10");
}

void enter_user_mode(void)
{
    asm volatile ("r0 = ssr\n\t"
                  "r0 = clrbit(r0, #17) // EX\n\t"
                  "r0 = setbit(r0, #16) // UM\n\t"
                  "r0 = clrbit(r0, #19) // GM\n\t"
                  "ssr = r0\n\t" : : : "r0");
}

int main(int argc, const char *argv[])
{
    struct bar B;
    memset(&B, 0x0f, sizeof(B));

    printf("Testing FRAMELIMIT\n");

    unsigned int sp = get_stack_ptr();
    printf("stack pointer = 0x%x\n", sp);
    set_framelimit(sp + 0x1000);
    enter_user_mode();

    return foo(2, &B);
}
