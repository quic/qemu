#!/usr/bin/env python3

##
##  Copyright(c) 2019-2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
import hex_common


def _line():
    f = sys._getframe().f_back
    return "[{f.f_code.co_filename}:{f.f_lineno}]"


##
## Generate the TCG code to call the helper
##     For A2_add: Rd32=add(Rs32,Rt32), { RdV=RsV+RtV;}
##     We produce:
##    static void generate_A2_add(DisasContext *ctx)
##    {
##        Insn *insn G_GNUC_UNUSED = ctx->insn;
##        const int RdN = insn->regno[0];
##        TCGv RdV = get_result_gpr(ctx, RdN);
##        TCGv RsV = hex_gpr[insn->regno[1]];
##        TCGv RtV = hex_gpr[insn->regno[2]];
##        <GEN>
##        gen_log_reg_write(ctx, RdN, RdV);
##    }
##
##       where <GEN> depends on hex_common.skip_qemu_helper(tag)
##       if hex_common.skip_qemu_helper(tag) is True
##       <GEN>  is fGEN_TCG_A2_add({ RdV=RsV+RtV;});
##       if hex_common.skip_qemu_helper(tag) is False
##       <GEN>  is gen_helper_A2_add(RdV, tcg_env, RsV, RtV);
##
def gen_tcg_func(f, tag, regs, imms):
    f.write(f"static void generate_{tag}(DisasContext *ctx)\n")
    f.write("{\n")

    f.write("    Insn *insn G_GNUC_UNUSED = ctx->insn;\n")

    if "A_PRIV" in hex_common.attribdict[tag]:
        f.write("#ifdef CONFIG_USER_ONLY\n")
        f.write("    gen_exception_end_tb(ctx, " "HEX_CAUSE_PRIV_USER_NO_SINSN);\n")
        f.write("#else\n")
    if "A_GUEST" in hex_common.attribdict[tag]:
        f.write("#ifdef CONFIG_USER_ONLY\n")
        f.write("    gen_exception_end_tb(ctx, HEX_CAUSE_PRIV_USER_NO_GINSN);\n")
        f.write("#else\n")
    if hex_common.need_ea(tag):
        f.write("    TCGv EA G_GNUC_UNUSED = tcg_temp_new();\n")

    ## Declare all the operands (regs and immediates)
    i = 0
    for regtype, regid, _, _, subtype in regs:
        reg = hex_common.get_register(tag, regtype, regid, subtype)
        reg.decl_tcg(f, tag, i)
        i += 1
    for immlett, bits, immshift in imms:
        i = 1 if immlett.isupper() else 0
        f.write(f"    int {hex_common.imm_name(immlett)} = insn->immed[{i}];\n")

    if hex_common.is_idef_parser_enabled(tag):
        declared = []
        ## Handle registers
        for regtype, regid, *_ in regs:
            reg = hex_common.get_register(tag, regtype, regid)
            reg.idef_arg(declared)
        ## Handle immediates
        for immlett, bits, immshift in imms:
            declared.append(hex_common.imm_name(immlett))

        arguments = ", ".join(["ctx", "ctx->insn", "ctx->pkt"] + declared)
        f.write(f"    emit_{tag}({arguments});\n")

    elif hex_common.skip_qemu_helper(tag):
        f.write(f"    fGEN_TCG_{tag}({hex_common.semdict[tag]});\n")
    else:
        ## Generate the call to the helper
        declared = []
        ret_type = hex_common.helper_ret_type(tag, regs).call_arg
        if ret_type != "void":
            declared.append(ret_type)

        for arg in hex_common.helper_args(tag, regs, imms):
            declared.append(arg.call_arg)

        arguments = ", ".join(declared)
        f.write(f"    gen_helper_{tag}({arguments});\n")

    ## Write all the outputs
    for regtype, regid, *_ in regs:
        reg = hex_common.get_register(tag, regtype, regid)
        if reg.is_written():
            reg.log_write(f, tag)

    if (
        "A_PRIV" in hex_common.attribdict[tag]
        or "A_GUEST" in hex_common.attribdict[tag]
    ):
        f.write("#endif   /* CONFIG_USER_ONLY */\n")
    f.write("}\n\n")


def gen_def_tcg_func(f, tag, tagregs, tagimms):
    regs = tagregs[tag]
    imms = tagimms[tag]

    gen_tcg_func(f, tag, regs, imms)


def main():
    hex_common.read_semantics_file(sys.argv[1])
    hex_common.read_attribs_file(sys.argv[2])
    hex_common.read_overrides_file(sys.argv[3])
    hex_common.read_overrides_file(sys.argv[4])
    hex_common.read_overrides_file(sys.argv[5])
    hex_common.calculate_attribs()
    hex_common.init_registers()
    ## Whether or not idef-parser is enabled is
    ## determined by the number of arguments to
    ## this script:
    ##
    ##   6 args. -> not enabled,
    ##   7 args. -> idef-parser enabled.
    ##
    ## The 7:th arg. then holds a list of the successfully
    ## parsed instructions.
    is_idef_parser_enabled = len(sys.argv) > 7
    if is_idef_parser_enabled:
        hex_common.read_idef_parser_enabled_file(sys.argv[6])
    tagregs = hex_common.get_tagregs(full=True)
    tagimms = hex_common.get_tagimms()

    output_file = sys.argv[-1]
    with open(output_file, "w") as f:
        f.write("#ifndef HEXAGON_TCG_FUNCS_H\n")
        f.write("#define HEXAGON_TCG_FUNCS_H\n\n")
        if is_idef_parser_enabled:
            f.write('#include "idef-generated-emitter.h.inc"\n\n')

        for tag in hex_common.tags:
            if hex_common.tag_ignore(tag):
                continue

            gen_def_tcg_func(f, tag, tagregs, tagimms)

        f.write("#endif    /* HEXAGON_TCG_FUNCS_H */\n")


if __name__ == "__main__":
    main()
