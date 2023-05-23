#!/usr/bin/env python3

##
##  Copyright(c) 2019-2023 rev.ng Labs Srl. All Rights Reserved.
##
##  This program is free software; you can redistribute it and/or modify
##  it under the terms of the GNU General Public License as published by
##  the Free Software Foundation; either version 2 of the License, or
##  (at your option) any later version.
##
##  This program is distributed in the hope that it will be useful,
##  but WITHOUT ANY WARRANTY; without even the implied warranty of
##  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
##  GNU General Public License for more details.
##
##  You should have received a copy of the GNU General Public License
##  along with this program; if not, see <http://www.gnu.org/licenses/>.
##

import sys
import re
import string
from io import StringIO

import hex_common


##
## Generate code to be fed to the idef_parser
##
## Consider A2_add:
##
##     Rd32=add(Rs32,Rt32), { RdV=RsV+RtV;}
##
## We produce:
##
##     A2_add(RdV, in RsV, in RtV) {
##       { RdV=RsV+RtV;}
##     }
##
## A2_add represents the instruction tag. Then we have a list of TCGv
## that the code generated by the parser can expect in input. Some of
## them are inputs ("in" prefix), while some others are outputs.
##
def main():
    hex_common.read_semantics_file(sys.argv[1])
    hex_common.read_attribs_file(sys.argv[2])
    hex_common.calculate_attribs()
    tagregs = hex_common.get_tagregs()
    tagimms = hex_common.get_tagimms()

    with open(sys.argv[3], "w") as f:
        f.write('#include "macros.inc"\n\n')

        for tag in hex_common.tags:
            if hex_common.tag_ignore(tag):
                continue
            if "A_HMX" in hex_common.attribdict[tag]:
                continue
            if "A_MEMSIZE_0B" in hex_common.attribdict[tag]:
                continue
            ## Skip the priv instructions
            if "A_PRIV" in hex_common.attribdict[tag]:
                continue
            ## Skip the guest instructions
            if "A_GUEST" in hex_common.attribdict[tag]:
                continue
            ## Skip instructions that saturate in a ternary expression
            if tag in {"S2_asr_r_r_sat", "S2_asl_r_r_sat"}:
                continue
            ## Skip instructions using switch
            if tag in {"S4_vrcrotate_acc", "S4_vrcrotate"}:
                continue
            ## Skip trap instructions
            if tag in {"J2_trap0", "J2_trap1"}:
                continue
            ## Skip 128-bit instructions
            if tag in {"A7_croundd_ri", "A7_croundd_rr"}:
                continue
            if tag in {
                "M7_wcmpyrw",
                "M7_wcmpyrwc",
                "M7_wcmpyiw",
                "M7_wcmpyiwc",
                "M7_wcmpyrw_rnd",
                "M7_wcmpyrwc_rnd",
                "M7_wcmpyiw_rnd",
                "M7_wcmpyiwc_rnd",
            }:
                continue
            ## Skip interleave/deinterleave instructions
            if tag in {"S2_interleave", "S2_deinterleave"}:
                continue
            ## Skip instructions using bit reverse
            if tag in {
                "S2_brev",
                "S2_brevp",
                "S2_ct0",
                "S2_ct1",
                "S2_ct0p",
                "S2_ct1p",
                "A4_tlbmatch",
            }:
                continue
            ## Skip other unsupported instructions
            if tag == "S2_cabacdecbin" or tag == "A5_ACS":
                continue
            if tag.startswith("Y"):
                continue
            if tag.startswith("V6_"):
                continue
            if ( tag.startswith("F") and
                 tag not in {
                     "F2_sfimm_p",
                     "F2_sfimm_n",
                     "F2_dfimm_p",
                     "F2_dfimm_n",
                     "F2_dfmpyll",
                     "F2_dfmpylh"
                 }):
                continue
            if tag.endswith("_locked"):
                continue
            if "A_COF" in hex_common.attribdict[tag]:
                continue
            if ( tag.startswith('R6_release_') ):
                continue
            ## Skip instructions that are incompatible with short-circuit
            ## packet register writes
            if ( tag == 'S2_insert' or
                 tag == 'S2_insert_rp' or
                 tag == 'S2_asr_r_svw_trun' or
                 tag == 'A2_swiz' ):
                continue

            regs = tagregs[tag]
            imms = tagimms[tag]

            arguments = []
            for regtype, regid in regs:
                prefix = "in " if hex_common.is_read(regid) else ""

                is_pair = hex_common.is_pair(regid)
                is_single_old = hex_common.is_single(regid) and hex_common.is_old_val(
                    regtype, regid, tag
                )
                is_single_new = hex_common.is_single(regid) and hex_common.is_new_val(
                    regtype, regid, tag
                )

                if is_pair or is_single_old:
                    arguments.append(f"{prefix}{regtype}{regid}V")
                elif is_single_new:
                    arguments.append(f"{prefix}{regtype}{regid}N")
                else:
                    hex_common.bad_register(regtype, regid)

            for immlett, bits, immshift in imms:
                arguments.append(hex_common.imm_name(immlett))

            f.write(f"{tag}({', '.join(arguments)}) {{\n")
            f.write("    ")
            if hex_common.need_ea(tag):
                f.write("size4u_t EA; ")
            f.write(f"{hex_common.semdict[tag]}\n")
            f.write("}\n\n")


if __name__ == "__main__":
    main()
