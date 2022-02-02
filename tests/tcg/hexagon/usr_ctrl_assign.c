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

#include <stdio.h>
#include <stdlib.h>

extern unsigned test_pass(
    unsigned c4_in, unsigned c4_out,
    unsigned in1, unsigned in2,
    unsigned out1,unsigned out2);

int basic_test()

{
    unsigned tmp = 0xffffffff;
    unsigned *tmp_ptr = &tmp;

    /* verify setting c4 does not change p3_0 until after packet commits */
    asm volatile ("{r1 = #0xffffffff\n\t"
                  " r7 = #0xffffffff\n\t"
                  " r0 = #0\n\t"
                  " c4 = r0}\n\t"
                  "{c4=r7\n\t"
                  "if (!p2) memw(%0) = #0}\n\t"
                  : : "r"(tmp_ptr) : "r1", "r7", "r0", "p3:0", "memory");
    return *tmp_ptr == 0x0;
}

int main()
{
    int rval;

    rval = basic_test();
    rval += test_pass(0xba73a5fd, 0xba73a5fd, 0x837ed653,
                      0x0000007f, 0x00000080, 0x0000001c);
    rval += test_pass(0x74e74bfb, 0x74e74bfb, 0x06fdaca7,
                      0x000000fe, 0x00000100, 0x00000038);
    rval += test_pass(0xe9ce97f6, 0xe9ce97f6, 0x0dfb594e,
                      0x000001fc, 0x00000200, 0x00000070);
    if (rval != 4) {
        printf("FAIL\n");
        exit(-1);
    }

    printf("PASS\n");
    exit(0);
}
