/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

/*
 *  Check the instruction counters in qemu
 */
#include <stdio.h>
#ifdef __HEXMSGABI_3_SUPPORTED__

int err;

static void check(const char *name, int val, int expect)
{
    if (val != expect) {
        printf("ERROR: %s %d != %d\n", name, val, expect);
        err++;
    }
}

int main()
{
    int pkt_old, pkt_new;
    int insn_old, insn_new;
    int hvx_old, hvx_new;

    /* Test reading from individual control regs */
    asm volatile("%[pkt_old] = c20\n\t"
                 "%[insn_old] = c21\n\t"
                 "%[hvx_old] = c22\n\t"
                 "r2 = #7\n\t"
                 "loop0(1f, #3)\n\t"
                 "1:\n\t"
                 "    v0.b = vadd(v0.b, v0.b)\n\t"
                 "    { p0 = cmp.eq(r2,#5); if (p0.new) jump:nt 2f }\n\t"
                 "    { r0 = r1; r1 = r0 }:endloop0\n\t"
                 "2:\n\t"
                 "%[pkt_new] = c20\n\t"
                 "%[insn_new] = c21\n\t"
                 "%[hvx_new] = c22\n\t"
                 : [pkt_old] "=r"(pkt_old),
                   [insn_old] "=r"(insn_old),
                   [hvx_old] "=r"(hvx_old),
                   [pkt_new] "=r"(pkt_new),
                   [insn_new] "=r"(insn_new),
                   [hvx_new] "=r"(hvx_new)
                 : : "r0", "r1", "r2", "sa0", "lc0", "v0", "p0");

    check("Packet", pkt_new - pkt_old, 14);
    check("Instruction", insn_new - insn_old, 17);
    check("HVX", hvx_new - hvx_old, 3);

    /* Test reading from control reg pairs */
    asm volatile("r1:0 = c21:20\n\t"
                 "%[pkt_old] = r0\n\t"
                 "%[insn_old] = r1\n\t"
                 "r1:0 = c23:22\n\t"
                 "%[hvx_old] = r0\n\t"
                 "r2 = #7\n\t"
                 "loop0(1f, #5)\n\t"
                 "1:\n\t"
                 "    v0.b = vadd(v0.b, v0.b)\n\t"
                 "    { p0 = cmp.eq(r2,#5); if (p0.new) jump:nt 2f }\n\t"
                 "    { r0 = r1; r1 = r0 }:endloop0\n\t"
                 "2:\n\t"
                 "r1:0 = c21:20\n\t"
                 "%[pkt_new] = r0\n\t"
                 "%[insn_new] = r1\n\t"
                 "r1:0 = c23:22\n\t"
                 "%[hvx_new] = r0\n\t"
                 : [pkt_old] "=r"(pkt_old),
                   [insn_old] "=r"(insn_old),
                   [hvx_old] "=r"(hvx_old),
                   [pkt_new] "=r"(pkt_new),
                   [insn_new] "=r"(insn_new),
                   [hvx_new] "=r"(hvx_new)
                 : : "r0", "r1", "r2", "sa0", "lc0", "v0", "p0");

    check("Packet", pkt_new - pkt_old, 22);
    check("Instruction", insn_new - insn_old, 27);
    check("HVX", hvx_new - hvx_old, 5);

    /* Test writing to individual control regs */
    asm volatile("r2 = #0\n\t"
                 "c20 = r2\n\t"
                 "c21 = r2\n\t"
                 "c22 = r2\n\t"
                 "r2 = #7\n\t"
                 "loop0(1f, #3)\n\t"
                 "1:\n\t"
                 "    v0.b = vadd(v0.b, v0.b)\n\t"
                 "    { p0 = cmp.eq(r2,#5); if (p0.new) jump:nt 2f }\n\t"
                 "    { r0 = r1; r1 = r0 }:endloop0\n\t"
                 "2:\n\t"
                 "%[pkt_new] = c20\n\t"
                 "%[insn_new] = c21\n\t"
                 "%[hvx_new] = c22\n\t"
                 : [pkt_new] "=r"(pkt_new),
                   [insn_new] "=r"(insn_new),
                   [hvx_new] "=r"(hvx_new)
                 : : "r0", "r1", "r2", "sa0", "lc0", "v0", "p0");

    check("Packet", pkt_new, 14);
    check("Instruction", insn_new, 17);
    check("HVX", hvx_new, 3);

    /* Test writing to control reg pairs */
    asm volatile("r0 = #0\n\t"
                 "r1 = #0\n\t"
                 "c21:20 = r1:0\n\t"
                 "c23:22 = r1:0\n\t"
                 "r2 = #7\n\t"
                 "loop0(1f, #3)\n\t"
                 "1:\n\t"
                 "    v0.b = vadd(v0.b, v0.b)\n\t"
                 "    { p0 = cmp.eq(r2,#5); if (p0.new) jump:nt 2f }\n\t"
                 "    { r0 = r1; r1 = r0 }:endloop0\n\t"
                 "2:\n\t"
                 "%[pkt_new] = c20\n\t"
                 "%[insn_new] = c21\n\t"
                 "%[hvx_new] = c22\n\t"
                 : [pkt_new] "=r"(pkt_new),
                   [insn_new] "=r"(insn_new),
                   [hvx_new] "=r"(hvx_new)
                 : : "r0", "r1", "r2", "sa0", "lc0", "v0", "p0");

    check("Packet", pkt_new, 13);
    check("Instruction", insn_new, 17);
    check("HVX", hvx_new, 3);

    puts(err ? "FAIL" : "PASS");
    return err;
}
#else
int main () {puts ("NOT RUN"); return 0;}
#endif
