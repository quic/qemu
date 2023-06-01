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

    .text
    .p2align 2
    .global test_pass
    .type test_pass, @function

/* r0 = c4 in      */
/* r1 = c4 out     */
/* r2 = combine in */
/* r3 = combine in */
/* r4 = r4 in/out  */
/* r5 = r5 in/out  */

test_pass:

    r6 = r4
    r7 = r5
    r8 = #0xffffffff
    c4 = r8
    {
    c4 = r0
    if (!p2) r7:6 = combine(r2, r3)
    }

    r9 = c4
    r8 = #0x0
    c4 = r8

    {
    p0 = cmp.eq(r9, r1)
    p1 = cmp.eq(r6, r4)
    p2 = cmp.eq(r7, r5)
    }
    r0 = c4
    r0 = cmp.eq(r0, #0x00FFFFFF)
    jumpr r31
    .size test_pass, .-test_pass

