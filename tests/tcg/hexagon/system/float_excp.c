
/*
 *  Copyright(c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

void set_usr_fp_exception_bits(void)
{
    asm volatile("r0 = usr\n\t"
                 "r0 = setbit(r0, #25)\n\t"
                 "r0 = setbit(r0, #26)\n\t"
                 "r0 = setbit(r0, #27)\n\t"
                 "r0 = setbit(r0, #28)\n\t"
                 "r0 = setbit(r0, #29)\n\t"
                 "usr = r0\n\t" : : : "r0");
}

void clear_usr_invalid(void)
{
    asm volatile("r0 = usr\n\t"
                 "r0 = clrbit(r0, #1)\n\t"
                 "usr = r0\n\t" : : : "r0");
}

void clear_usr_div_by_zero(void)
{
    asm volatile("r0 = usr\n\t"
                 "r0 = clrbit(r0, #2)\n\t"
                 "usr = r0\n\t" : : : "r0");
}

int get_usr(void)
{
    int ret;
    asm volatile("%0 = usr\n\t" : "=r"(ret));
    return ret;
}

int get_usr_invalid(void)
{
    return (get_usr() >> 1) & 1;
}

int get_usr_div_by_zero(void)
{
    return (get_usr() >> 2) & 1;
}

void gen_sfinvsqrta_exception(void)
{
    /* Force a invalid exception */
    float RsV = -1.0;
    asm volatile("R2,P0 = sfinvsqrta(%0)\n\t"
                 "R4 = sffixupd(R0, R1)\n\t"
                 : : "r"(RsV) : "r2", "p0", "r4");
}

void gen_sfrecipa_exception(void)
{
    /* Force a divide-by-zero exception */
    int RsV = 0x3f800000;
    int RtV = 0x00000000;
    asm volatile("R2,P0 = sfrecipa(%0, %1)\n\t"
                 "R4 = sffixupd(R0, R1)\n\t"
                 : : "r"(RsV), "r"(RtV) : "r2", "p0", "r4");
}

int main(int argc, char *argv[])
{
    clear_usr_invalid();
    if (get_usr_invalid()) {
        printf("ERROR: usr invalid bit not cleared\n");
        return 1;
    }

    gen_sfinvsqrta_exception();
    if (get_usr_invalid() == 0) {
        printf("ERROR: usr invalid bit not set\n");
        return 1;
    }

    clear_usr_div_by_zero();
    if (get_usr_div_by_zero()) {
        printf("ERROR: usr div-by-zero bit not cleared\n");
        return 1;
    }

    set_usr_fp_exception_bits();
    gen_sfrecipa_exception();

    return 0;
}
