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

int main()
{
    unsigned int rev;
    asm volatile(
            "r0 = rev\n"
            "r1 = #255\n"
            "%0 = and(r0, r1)\n"
            : "=r"(rev)
            :
            : "r0", "r1");
    printf("0x%x\n", rev);
    return 0;
}
