#!/usr/bin/env python3

##
##  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

from hex_common import *

##
## Helpers for gen_helper_function
##
def gen_decl_ea(f):
    f.write("uint32_t EA;\n")

def gen_helper_return_type(f,regtype,regid,regno):
    if regno > 1 : f.write(", ")
    f.write("int32_t")

def gen_helper_return_type_pair(f,regtype,regid,regno):
    if regno > 1 : f.write(", ")
    f.write("int64_t")

def gen_helper_arg(f,regtype,regid,regno):
    if regno > 0 : f.write(", " )
    f.write("int32_t %s%sV" % (regtype,regid))

def gen_helper_arg_new(f,regtype,regid,regno):
    if regno >= 0 : f.write(", " )
    f.write("int32_t %s%sN" % (regtype,regid))

def gen_helper_arg_pair(f,regtype,regid,regno):
    if regno >= 0 : f.write(", ")
    f.write("int64_t %s%sV" % (regtype,regid))

def gen_helper_arg_ext(f,regtype,regid,regno):
    if regno > 0 : f.write(", ")
    f.write("void *%s%sV_void" % (regtype,regid))

def gen_helper_arg_ext_pair(f,regtype,regid,regno):
    if regno > 0 : f.write(", ")
    f.write("void *%s%sV_void" % (regtype,regid))

def gen_helper_arg_opn(f,regtype,regid,i,tag):
    if (is_pair(regid)):
        if (is_hvx_reg(regtype)):
            gen_helper_arg_ext_pair(f,regtype,regid,i)
        else:
            gen_helper_arg_pair(f,regtype,regid,i)
    elif (is_single(regid)):
        if is_old_val(regtype, regid, tag):
            if (is_hvx_reg(regtype)):
                gen_helper_arg_ext(f,regtype,regid,i)
            else:
                gen_helper_arg(f,regtype,regid,i)
        elif is_new_val(regtype, regid, tag):
            gen_helper_arg_new(f,regtype,regid,i)
        else:
            print("Bad register parse: ",regtype,regid,toss,numregs)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

def gen_helper_arg_imm(f,immlett):
    f.write(", int32_t %s" % (imm_name(immlett)))

def gen_helper_dest_decl(f,regtype,regid,regno,subfield=""):
    f.write("int32_t %s%sV%s = 0;\n" % \
        (regtype,regid,subfield))

def gen_helper_dest_decl_pair(f,regtype,regid,regno,subfield=""):
    f.write("int64_t %s%sV%s = 0;\n" % \
        (regtype,regid,subfield))

def gen_helper_dest_decl_ext(f,regtype,regid):
    f.write("/* %s%sV is *(mmvector_t*)(%s%sV_void) */\n" % \
        (regtype,regid,regtype,regid))

def gen_helper_dest_decl_ext_pair(f,regtype,regid,regno):
    f.write("/* %s%sV is *(mmvector_pair_t*))%s%sV_void) */\n" % \
        (regtype,regid,regtype, regid))

def gen_helper_dest_decl_opn(f,regtype,regid,i):
    if (is_pair(regid)):
        if (is_hvx_reg(regtype)):
            gen_helper_dest_decl_ext_pair(f,regtype,regid, i)
        else:
            gen_helper_dest_decl_pair(f,regtype,regid,i)
    elif (is_single(regid)):
        if (is_hvx_reg(regtype)):
            gen_helper_dest_decl_ext(f,regtype,regid)
        else:
            gen_helper_dest_decl(f,regtype,regid,i)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

def gen_helper_src_var_ext(f,regtype,regid):
    f.write("/* %s%sV is *(mmvector_t*)(%s%sV_void) */\n" % \
        (regtype,regid,regtype,regid))

def gen_helper_src_var_ext_pair(f,regtype,regid,regno):
    f.write("/* %s%sV%s is *(mmvector_pair_t*)(%s%sV%s_void) */\n" % \
        (regtype,regid,regno,regtype,regid,regno))

def gen_helper_return(f,regtype,regid,regno):
    f.write("return %s%sV;\n" % (regtype,regid))

def gen_helper_return_pair(f,regtype,regid,regno):
    f.write("return %s%sV;\n" % (regtype,regid))

def gen_helper_dst_write_ext(f,regtype,regid):
    f.write("/* %s%sV is *(mmvector_t*)%s%sV_void */\n" % \
        (regtype,regid,regtype,regid))

def gen_helper_dst_write_ext_pair(f,regtype,regid):
    f.write("/* %s%sV is *(mmvector_pair_t*)%s%sV_void */\n" % \
        (regtype,regid, regtype,regid))

def gen_helper_return_opn(f, regtype, regid, i):
    if (is_pair(regid)):
        if (is_hvx_reg(regtype)):
            gen_helper_dst_write_ext_pair(f,regtype,regid)
        else:
            gen_helper_return_pair(f,regtype,regid,i)
    elif (is_single(regid)):
        if (is_hvx_reg(regtype)):
            gen_helper_dst_write_ext(f,regtype,regid)
        else:
            gen_helper_return(f,regtype,regid,i)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

##
## Generate the TCG code to call the helper
##     For A2_add: Rd32=add(Rs32,Rt32), { RdV=RsV+RtV;}
##     We produce:
##       #ifndef fGEN_TCG_A2_add
##       int32_t HELPER(A2_add)(CPUHexagonState *env, int32_t RsV, int32_t RtV)
##       {
##       uint32_t slot __attribute__(unused)) = 4;
##       int32_t RdV = 0;
##       { RdV=RsV+RtV;}
##       COUNT_HELPER(A2_add);
##       return RdV;
##       }
##       #endif
##
def gen_helper_function(f, tag, tagregs, tagimms):
    regs = tagregs[tag]
    imms = tagimms[tag]

    f.write('#ifndef fGEN_TCG_%s\n' % tag)
    numresults = 0
    numscalarresults = 0
    numscalarreadwrite = 0
    for regtype,regid,toss,numregs in regs:
        if (is_written(regid)):
            numresults += 1
            if (is_scalar_reg(regtype)):
                numscalarresults += 1
        if (is_readwrite(regid)):
            if (is_scalar_reg(regtype)):
                numscalarreadwrite += 1

    if (numscalarresults > 1):
        ## The helper is bogus when there is more than one result
        f.write("void HELPER(%s)(CPUHexagonState *env) { BOGUS_HELPER(%s); }\n"
                % (tag, tag))
    else:
        ## The return type of the function is the type of the destination
        ## register (if scalar)
        i=0
        for regtype,regid,toss,numregs in regs:
            if (is_written(regid)):
                if (is_pair(regid)):
                    if (is_hvx_reg(regtype)):
                        continue
                    else:
                        gen_helper_return_type_pair(f,regtype,regid,i)
                elif (is_single(regid)):
                    if (is_hvx_reg(regtype)):
                            continue
                    else:
                        gen_helper_return_type(f,regtype,regid,i)
                else:
                    print("Bad register parse: ",regtype,regid,toss,numregs)
            i += 1

        if (numscalarresults == 0):
            f.write("void")
        f.write(" HELPER(%s)(CPUHexagonState *env" % tag)

        ## Arguments include the vector destination operands
        i = 1
        for regtype,regid,toss,numregs in regs:
            if (is_written(regid)):
                if (is_pair(regid)):
                    if (is_hvx_reg(regtype)):
                        gen_helper_arg_ext_pair(f,regtype,regid,i)
                    else:
                        continue
                elif (is_single(regid)):
                    if (is_hvx_reg(regtype)):
                        gen_helper_arg_ext(f,regtype,regid,i)
                    else:
                        # This is the return value of the function
                        continue
                else:
                    print("Bad register parse: ",regtype,regid,toss,numregs)
                i += 1

        ## Arguments to the helper function are the source regs and immediates
        for regtype,regid,toss,numregs in regs:
            if (is_read(regid)):
                if (is_hvx_reg(regtype) and is_readwrite(regid)):
                    continue
                gen_helper_arg_opn(f,regtype,regid,i,tag)
                i += 1
        for immlett,bits,immshift in imms:
            gen_helper_arg_imm(f,immlett)
            i += 1
        if need_slot(tag):
            if i > 0: f.write(", ")
            f.write("uint32_t slot")
            i += 1
        if need_part1(tag):
            if i > 0: f.write(", ")
            f.write("uint32_t part1")
        f.write(")\n{\n")
        if (not need_slot(tag)): f.write("uint32_t slot __attribute__((unused)) = 4;\n" )
        if need_ea(tag): gen_decl_ea(f)
        ## Declare the return variable
        i=0
        for regtype,regid,toss,numregs in regs:
            if (is_writeonly(regid)):
                gen_helper_dest_decl_opn(f,regtype,regid,i)
            i += 1

        for regtype,regid,toss,numregs in regs:
            if (is_read(regid)):
                if (is_pair(regid)):
                    if (is_hvx_reg(regtype)):
                        gen_helper_src_var_ext_pair(f,regtype,regid,i)
                elif (is_single(regid)):
                    if (is_hvx_reg(regtype)):
                        gen_helper_src_var_ext(f,regtype,regid)
                else:
                    print("Bad register parse: ",regtype,regid,toss,numregs)

        if 'A_FPOP' in attribdict[tag]:
            f.write('fFPOP_START();\n');

        f.write(semdict[tag])
        f.write("\n")
        f.write("COUNT_HELPER(%s);\n" % tag )

        if 'A_FPOP' in attribdict[tag]:
            f.write('fFPOP_END();\n');

        ## Save/return the return variable
        for regtype,regid,toss,numregs in regs:
            if (is_written(regid)):
                gen_helper_return_opn(f, regtype, regid, i)
        f.write("}\n")
        ## End of the helper definition
    f.write('#endif\n')

def main():
    read_semantics_file(sys.argv[1])
    read_attribs_file(sys.argv[2])
    calculate_attribs()
    tagregs = get_tagregs()
    tagimms = get_tagimms()

    f = StringIO()

    for tag in tags:
        ## Skip the priv instructions
        if ( "A_PRIV" in attribdict[tag] ) :
            continue
        ## Skip the guest instructions
        if ( "A_GUEST" in attribdict[tag] ) :
            continue
        ## Skip the diag instructions
        if ( tag == "Y6_diag" ) :
            continue
        if ( tag == "Y6_diag0" ) :
            continue
        if ( tag == "Y6_diag1" ) :
            continue

        gen_helper_function(f, tag, tagregs, tagimms)


    realf = open('helper_funcs_generated.h','w')
    realf.write(f.getvalue())
    realf.close()
    f.close()

if __name__ == "__main__":
    main()
