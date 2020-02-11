#!/usr/bin/env python

from __future__ import print_function
##
##  Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
try:
    from StringIO import StringIO
except ImportError:
    from io import StringIO


import operator
from itertools import chain



behdict = {}          # tag ->behavior
semdict = {}          # tag -> semantics
extdict = {}          # tag -> What extension an instruction belongs to (or "")
extnames = {}         # ext name -> True
attribdict = {}       # tag -> attributes
macros = {}           # macro -> macro information...
attribinfo = {}       # Register information and misc
tags = []             # list of all tags

def get_macro(macname,ext=""):
    mackey = macname + ":" + ext
    if ext and mackey not in macros:
        return get_macro(macname,"")
    return macros[mackey]

# We should do this as a hash for performance,
# but to keep order let's keep it as a list.
def uniquify(seq):
    seen = set()
    seen_add = seen.add
    return [x for x in seq if x not in seen and not seen_add(x)]

regre = re.compile(
    r"((?<!DUP)[MNORCPQXSGVZA])([stuvwxyzdefg]+)([.]?[LlHh]?)(\d+S?)")
immre = re.compile(r"[#]([rRsSuUm])(\d+)(?:[:](\d+))?")
reg_or_immre = \
    re.compile(r"(((?<!DUP)[MNRCOPQXSGVZA])([stuvwxyzdefg]+)" + \
                "([.]?[LlHh]?)(\d+S?))|([#]([rRsSuUm])(\d+)[:]?(\d+)?)")
relimmre = re.compile(r"[#]([rR])(\d+)(?:[:](\d+))?")
absimmre = re.compile(r"[#]([sSuUm])(\d+)(?:[:](\d+))?")

finished_macros = set()

def expand_macro_attribs(macro,allmac_re):
    if macro.key not in finished_macros:
        # Get a list of all things that might be macros
        l = allmac_re.findall(macro.beh)
        for submacro in l:
            if not submacro: continue
            if not get_macro(submacro,macro.ext):
                raise Exception("Couldn't find macro: <%s>" % l)
            macro.attribs |= expand_macro_attribs(
                get_macro(submacro,macro.ext), allmac_re)
            finished_macros.add(macro.key)
    return macro.attribs

immextre = re.compile(r'f(MUST_)?IMMEXT[(]([UuSsRr])')
def calculate_attribs():
    # Recurse down macros, find attributes from sub-macros
    macroValues = list(macros.values())
    allmacros_restr = "|".join(set([ m.re.pattern for m in macroValues ]))
    allmacros_re = re.compile(allmacros_restr)
    for macro in macroValues:
        expand_macro_attribs(macro,allmacros_re)
    # Append attributes to all instructions
    for tag in tags:
        for macname in allmacros_re.findall(semdict[tag]):
            if not macname: continue
            macro = get_macro(macname,extdict[tag])
            attribdict[tag] |= set(macro.attribs)
        m = immextre.search(semdict[tag])
        if m:
            if m.group(2).isupper():
                attrib = 'A_EXT_UPPER_IMMED'
            elif m.group(2).islower():
                attrib = 'A_EXT_LOWER_IMMED'
            else:
                raise "Not a letter: %s (%s)" % (m.group(1),tag)
            if not attrib in attribdict[tag]:
                attribdict[tag].add(attrib)

def SEMANTICS(tag, beh, sem):
    #print tag,beh,sem
    extdict[tag] = ""
    behdict[tag] = beh
    semdict[tag] = sem
    attribdict[tag] = set()
    tags.append(tag)        # dicts have no order, this is for order

def EXT_SEMANTICS(ext, tag, beh, sem):
    #print tag,beh,sem
    extnames[ext] = True
    extdict[tag] = ext
    behdict[tag] = beh
    semdict[tag] = sem
    attribdict[tag] = set()
    tags.append(tag)        # dicts have no order, this is for order


def ATTRIBUTES(tag, attribstring):
    attribstring = \
        attribstring.replace("ATTRIBS","").replace("(","").replace(")","")
    if not attribstring:
        return
    attribs = attribstring.split(",")
    for attrib in attribs:
        attribdict[tag].add(attrib.strip())

class Macro(object):
    __slots__ = ['key','name', 'beh', 'attribs', 're','ext']
    def __init__(self,key, name, beh, attribs,ext):
        self.key = key
        self.name = name
        self.beh = beh
        self.attribs = set(attribs)
        self.ext = ext
        self.re = re.compile("\\b" + name + "\\b")

def MACROATTRIB(macname,beh,attribstring,ext=""):
    attribstring = attribstring.replace("(","").replace(")","")
    mackey = macname + ":" + ext
    if attribstring:
        attribs = attribstring.split(",")
    else:
        attribs = []
    macros[mackey] = Macro(mackey,macname,beh,attribs,ext)

# read in file.  Evaluate each line: each line calls a function above

for line in open(sys.argv[1], 'rt').readlines():
    if not line.startswith("#"):
        eval(line.strip())


calculate_attribs()


attribre = re.compile(r'DEF_ATTRIB\(([A-Za-z0-9_]+), ([^,]*), ' +
        r'"([A-Za-z0-9_\.]*)", "([A-Za-z0-9_\.]*)"\)')
for line in open(sys.argv[2], 'rt').readlines():
    if not attribre.match(line):
        continue
    (attrib_base,descr,rreg,wreg) = attribre.findall(line)[0]
    attrib_base = 'A_' + attrib_base
    attribinfo[attrib_base] = {'rreg':rreg, 'wreg':wreg, 'descr':descr}

def compute_tag_regs(tag):
    return uniquify(regre.findall(behdict[tag]))

def compute_tag_immediates(tag):
    return uniquify(immre.findall(behdict[tag]))

##
##  tagregs is the main data structure we'll use
##  tagregs[tag] will contain the registers used by an instruction
##  Within each entry, we'll use the regtype and regid fields
##      regtype can be one of the following
##          C                control register
##          N                new register value
##          P                predicate register
##          R                GPR register
##          M                modifier register
##          Q                HVX predicate vector
##          V                HVX vector register
##          O                HVX new vector register
##      regid can be one of the following
##          d, e             destination register
##          dd               destination register pair
##          s, t, u, v, w    source register
##          ss, tt, uu, vv   source register pair
##          x, y             read-write register
##          xx, yy           read-write register pair
##
tagregs = dict(zip(tags, list(map(compute_tag_regs, tags))))

def is_pair(regid):
    return len(regid) == 2

def is_single(regid):
    return len(regid) == 1

def is_written(regid):
    return regid[0] in "dexy"

def is_writeonly(regid):
    return regid[0] in "de"

def is_read(regid):
    return regid[0] in "stuvwxy"

def is_readwrite(regid):
    return regid[0] in "xy"

def is_scalar_reg(regtype):
    return regtype in "RPC"

def is_hvx_reg(regtype):
    return regtype in "VQ"

def is_old_val(regtype, regid, tag):
    return regtype+regid+'V' in semdict[tag]

def is_new_val(regtype, regid, tag):
    return regtype+regid+'N' in semdict[tag]

tagimms = dict(zip(tags, list(map(compute_tag_immediates, tags))))

def need_slot(tag):
    if ('A_CONDEXEC' in attribdict[tag] or
        'A_STORE' in attribdict[tag] or
        'A_CVI' in attribdict[tag]):
        return 1
    else:
        return 0

def need_part1(tag):
    return re.compile(r"fPART1").search(semdict[tag])

def need_ea(tag):
    return re.compile(r"\bEA\b").search(semdict[tag])

def imm_name(immlett):
    return "%siV" % immlett

##
## Helpers for gen_helper_prototype
##
def_helper_types = {
    'N' : 's32',
    'O' : 's32',
    'P' : 's32',
    'M' : 's32',
    'C' : 's32',
    'R' : 's32',
    'V' : 'ptr',
    'Q' : 'ptr'
}

def_helper_types_pair = {
    'R' : 's64',
    'C' : 's64',
    'S' : 's64',
    'G' : 's64',
    'V' : 'ptr',
    'Q' : 'ptr'
}

def gen_def_helper_opn(f, tag, regtype, regid, toss, numregs, i):
    if (is_pair(regid)):
        f.write(", %s" % (def_helper_types_pair[regtype]))
    elif (is_single(regid)):
        f.write(", %s" % (def_helper_types[regtype]))
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

##
## Generate the DEF_HELPER prototype for an instruction
##     For A2_add: Rd32=add(Rs32,Rt32)
##     We produce:
##         #ifndef fWRAP_A2_add
##         DEF_HELPER_3(A2_add, s32, env, s32, s32)
##         #endif
##
def gen_helper_prototype(f, tag, regs, imms):
    f.write('#ifndef fWRAP_%s\n' % tag)
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
        f.write('DEF_HELPER_1(%s, void, env)\n' % tag)
    else:
        ## Figure out how many arguments the helper will take
        if (numscalarresults == 0):
            def_helper_size = len(regs)+len(imms)+numscalarreadwrite+1
            if need_part1(tag): def_helper_size += 1
            if need_slot(tag): def_helper_size += 1
            f.write('DEF_HELPER_%s(%s' % (def_helper_size, tag))
            ## The return type is void
            f.write(', void' )
        else:
            def_helper_size = len(regs)+len(imms)+numscalarreadwrite
            if need_part1(tag): def_helper_size += 1
            if need_slot(tag): def_helper_size += 1
            f.write('DEF_HELPER_%s(%s' % (def_helper_size, tag))

        ## Generate the qemu DEF_HELPER type for each result
        ## Iterate over this list twice
        ## - Emit the scalar result
        ## - Emit the vector result
        i=0
        for regtype,regid,toss,numregs in regs:
            if (is_written(regid)):
                if (not is_hvx_reg(regtype)):
                    gen_def_helper_opn(f, tag, regtype, regid, toss, numregs, i)
                i += 1

        ## Put the env between the outputs and inputs
        f.write(', env' )
        i += 1

        # Second pass
        for regtype,regid,toss,numregs in regs:
            if (is_written(regid)):
                if (is_hvx_reg(regtype)):
                    gen_def_helper_opn(f, tag, regtype, regid, toss, numregs, i)
                    i += 1

        ## Generate the qemu type for each input operand (regs and immediates)
        for regtype,regid,toss,numregs in regs:
            if (is_read(regid)):
                if (is_hvx_reg(regtype) and is_readwrite(regid)):
                    continue
                gen_def_helper_opn(f, tag, regtype, regid, toss, numregs, i)
                i += 1
        for immlett,bits,immshift in imms:
            f.write(", s32")

        ## Add the arguments for the instruction slot and part1 (if needed)
        if need_slot(tag): f.write(', i32' )
        if need_part1(tag): f.write(' , i32' )
        f.write(')\n')
    f.write('#endif\n')

##
## Helpers for gen_tcg_func
##
def gen_decl_ea_tcg(f):
    f.write("DECL_EA;\n")

def gen_free_ea_tcg(f):
    f.write("FREE_EA;\n")

def genptr_decl(f,regtype,regid,regno):
    regN="%s%sN" % (regtype,regid)
    macro = "DECL_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sV, %s, %d, 0);\n" % \
        (macro, regtype, regid, regN, regno))

def genptr_decl_new(f,regtype,regid,regno):
    regN="%s%sX" % (regtype,regid)
    macro = "DECL_NEW_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sN, %s, %d, 0);\n" % \
        (macro, regtype, regid, regN, regno))

def genptr_decl_opn(f, tag, regtype, regid, toss, numregs, i):
    if (is_pair(regid)):
        genptr_decl(f,regtype,regid,i)
    elif (is_single(regid)):
        if is_old_val(regtype, regid, tag):
            genptr_decl(f,regtype,regid,i)
        elif is_new_val(regtype, regid, tag):
            genptr_decl_new(f,regtype,regid,i)
        else:
            print("Bad register parse: ",regtype,regid,toss,numregs)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

def genptr_decl_imm(f,immlett):
    if (immlett.isupper()):
        i = 1
    else:
        i = 0
    f.write("DECL_IMM(%s,%d);\n" % (imm_name(immlett),i))

def genptr_free(f,regtype,regid,regno):
    macro = "FREE_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sV);\n" % (macro, regtype, regid))

def genptr_free_new(f,regtype,regid,regno):
    macro = "FREE_NEW_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sN);\n" % (macro, regtype, regid))

def genptr_free_opn(f,regtype,regid,i):
    if (is_pair(regid)):
        genptr_free(f,regtype,regid,i)
    elif (is_single(regid)):
        if is_old_val(regtype, regid, tag):
            genptr_free(f,regtype,regid,i)
        elif is_new_val(regtype, regid, tag):
            genptr_free_new(f,regtype,regid,i)
        else:
            print("Bad register parse: ",regtype,regid,toss,numregs)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

def genptr_free_imm(f,immlett):
    f.write("FREE_IMM(%s);\n" % (imm_name(immlett)))

def genptr_src_read(f,regtype,regid):
    macro = "READ_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sV, %s%sN);\n" % \
        (macro,regtype,regid,regtype,regid))

def genptr_src_read_new(f,regtype,regid):
    macro = "READ_NEW_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sN, %s%sX);\n" % \
        (macro,regtype,regid,regtype,regid))

def genptr_src_read_opn(f,regtype,regid):
    if (is_pair(regid)):
        genptr_src_read(f,regtype,regid)
    elif (is_single(regid)):
        if is_old_val(regtype, regid, tag):
            genptr_src_read(f,regtype,regid)
        elif is_new_val(regtype, regid, tag):
            genptr_src_read_new(f,regtype,regid)
        else:
            print("Bad register parse: ",regtype,regid,toss,numregs)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

def gen_helper_call_opn(f, tag, regtype, regid, toss, numregs, i):
    if (i > 0): f.write(", ")
    if (is_pair(regid)):
        f.write("%s%sV" % (regtype,regid))
    elif (is_single(regid)):
        if is_old_val(regtype, regid, tag):
            f.write("%s%sV" % (regtype,regid))
        elif is_new_val(regtype, regid, tag):
            f.write("%s%sN" % (regtype,regid))
        else:
            print("Bad register parse: ",regtype,regid,toss,numregs)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

def gen_helper_decl_imm(f,immlett):
    f.write("DECL_TCG_IMM(tcgv_%s, %s);\n" % \
        (imm_name(immlett), imm_name(immlett)))

def gen_helper_call_imm(f,immlett):
    f.write(", tcgv_%s" % imm_name(immlett))

def gen_helper_free_imm(f,immlett):
    f.write("FREE_TCG_IMM(tcgv_%s);\n" % imm_name(immlett))

def genptr_dst_write(f,regtype, regid):
    macro = "WRITE_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sN, %s%sV);\n" % (macro, regtype, regid, regtype, regid))

def genptr_dst_write_ext(f, regtype, regid, newv="0"):
    macro = "WRITE_%sREG_%s" % (regtype, regid)
    f.write("%s(%s%sN, %s%sV,%s);\n" % \
            (macro, regtype, regid, regtype, regid, newv))

def genptr_dst_write_opn(f,regtype, regid, tag):
    if (is_pair(regid)):
        if (is_hvx_reg(regtype)):
            if ('A_CVI_TMP' in attribdict[tag] or
                'A_CVI_TMP_DST' in attribdict[tag]):
                genptr_dst_write_ext(f, regtype, regid, "EXT_TMP")
            else:
                genptr_dst_write_ext(f, regtype, regid)
        else:
            genptr_dst_write(f, regtype, regid)
    elif (is_single(regid)):
        if (is_hvx_reg(regtype)):
            if 'A_CVI_NEW' in attribdict[tag]:
                genptr_dst_write_ext(f, regtype, regid, "EXT_NEW")
            elif 'A_CVI_TMP' in attribdict[tag]:
                genptr_dst_write_ext(f, regtype, regid, "EXT_TMP")
            elif 'A_CVI_TMP_DST' in attribdict[tag]:
                genptr_dst_write_ext(f, regtype, regid, "EXT_TMP")
            else:
                genptr_dst_write_ext(f, regtype, regid, "EXT_DFL")
        else:
            genptr_dst_write(f, regtype, regid)
    else:
        print("Bad register parse: ",regtype,regid,toss,numregs)

##
## Generate the TCG code to call the helper
##     For A2_add: Rd32=add(Rs32,Rt32), { RdV=RsV+RtV;}
##     We produce:
##       {
##       /* A2_add */
##       DECL_RREG_d(RdV, RdN, 0, 0);
##       DECL_RREG_s(RsV, RsN, 1, 0);
##       DECL_RREG_t(RtV, RtN, 2, 0);
##       READ_RREG_s(RsV, RsN);
##       READ_RREG_t(RtV, RtN);
##       fWRAP_A2_add(
##       do {
##       gen_helper_A2_add(RdV, cpu_env, RsV, RtV);
##       while (0),
##       { RdV=RsV+RtV;});
##       WRITE_RREG_d(RdN, RdV);
##       FREE_RREG_d(RdV);
##       FREE_RREG_s(RsV);
##       FREE_RREG_t(RtV);
##       /* A2_add */
##       }
##
def gen_tcg_func(f, tag, regs, imms):
    f.write('{\n')
    f.write('/* %s */\n' % tag)
    if need_ea(tag): gen_decl_ea_tcg(f)
    i=0
    ## Declare all the operands (regs and immediates)
    for regtype,regid,toss,numregs in regs:
        genptr_decl_opn(f, tag, regtype, regid, toss, numregs, i)
        i += 1
    for immlett,bits,immshift in imms:
        genptr_decl_imm(f,immlett)

    if 'A_PRIV' in attribdict[tag]:
        f.write('fCHECKFORPRIV();\n')
    if 'A_GUEST' in attribdict[tag]:
        f.write('fCHECKFORGUEST();\n')
    if 'A_FPOP' in attribdict[tag]:
        f.write('fFPOP_START();\n');

    ## Read all the inputs
    for regtype,regid,toss,numregs in regs:
        if (is_read(regid)):
            genptr_src_read_opn(f,regtype,regid)

    ## Generate the call to the helper
    f.write("fWRAP_%s(\n" % tag)
    f.write("do {\n")
    for immlett,bits,immshift in imms:
        gen_helper_decl_imm(f,immlett)
    if need_part1(tag): f.write("PART1_WRAP(")
    if need_slot(tag): f.write("SLOT_WRAP(")
    f.write("gen_helper_%s(" % (tag))
    i=0
    ## If there is a scalar result, it is the return type
    for regtype,regid,toss,numregs in regs:
        if (is_written(regid)):
            if (is_hvx_reg(regtype)):
                continue
            gen_helper_call_opn(f, tag, regtype, regid, toss, numregs, i)
            i += 1
    if (i > 0): f.write(", ")
    f.write("cpu_env")
    i=1
    for regtype,regid,toss,numregs in regs:
        if (is_written(regid)):
            if (not is_hvx_reg(regtype)):
                continue
            gen_helper_call_opn(f, tag, regtype, regid, toss, numregs, i)
            i += 1
    for regtype,regid,toss,numregs in regs:
        if (is_read(regid)):
            if (is_hvx_reg(regtype) and is_readwrite(regid)):
                continue
            gen_helper_call_opn(f, tag, regtype, regid, toss, numregs, i)
            i += 1
    for immlett,bits,immshift in imms:
        gen_helper_call_imm(f,immlett)

    if need_slot(tag): f.write(", slot")
    if need_part1(tag): f.write(", part1" )
    f.write(")")
    if need_slot(tag): f.write(")")
    if need_part1(tag): f.write(")")
    f.write(";\n")
    for immlett,bits,immshift in imms:
        gen_helper_free_imm(f,immlett)
    f.write("} while (0)")
    f.write(",\n%s);\n" % semdict[tag] )

    ## Write all the outputs
    for regtype,regid,toss,numregs in regs:
        if (is_written(regid)):
            genptr_dst_write_opn(f,regtype, regid, tag)

    if 'A_FPOP' in attribdict[tag]:
        f.write('fFPOP_END();\n');


    ## Free all the operands (regs and immediates)
    if need_ea(tag): gen_free_ea_tcg(f)
    for regtype,regid,toss,numregs in regs:
        genptr_free_opn(f,regtype,regid,i)
        i += 1
    for immlett,bits,immshift in imms:
        genptr_free_imm(f,immlett)

    f.write("/* %s */\n" % tag)
    f.write("}")

##
## Helpers for gen_helper_definition
##
def gen_decl_ea(f):
    f.write("size4u_t EA;\n")

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

def gen_helper_arg_opn(f,regtype,regid,i):
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
##       #ifndef fWRAP_A2_add
##       int32_t HELPER(A2_add)(CPUHexagonState *env, int32_t RsV, int32_t RtV)
##       {
##       uint32_t slot = 4; slot = slot;
##       int32_t RdV = 0;
##       { RdV=RsV+RtV;}
##       COUNT_HELPER(A2_add);
##       return RdV;
##       }
##       #endif
##
def gen_helper_definition(f, tag, regs, imms):
    f.write('#ifndef fWRAP_%s\n' % tag)
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
                gen_helper_arg_opn(f,regtype,regid,i)
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
        if (not need_slot(tag)): f.write("uint32_t slot = 4; slot = slot;\n" )
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

##
## Bring it all together in the DEF_QEMU macro
##
def gen_qemu(f, tag):
    regs = tagregs[tag]
    imms = tagimms[tag]

    f.write('DEF_QEMU(%s,%s,\n' % (tag,semdict[tag]))
    gen_helper_prototype(f, tag, regs, imms)
    f.write(",\n" )
    gen_tcg_func(f, tag, regs, imms)
    f.write(",\n" )
    gen_helper_definition(f, tag, regs, imms)
    f.write(")\n")

##
## Generate the qemu_def_generated.h file
##
f = StringIO()

f.write("#ifndef DEF_QEMU\n")
f.write("#define DEF_QEMU(TAG,SHORTCODE,HELPER,GENFN,HELPFN)   /* Nothing */\n")
f.write("#endif\n\n")


for tag in tags:
    ## Skip assembler mapped instructions
    if "A_MAPPING" in attribdict[tag]:
        continue
    ## Skip the fake instructions
    if ( "A_FAKEINSN" in attribdict[tag] ) :
        continue
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

    gen_qemu(f, tag)

realf = open('qemu_def_generated.h','w')
realf.write(f.getvalue())
realf.close()
f.close()

##
## Generate the qemu_wrap_generated.h file
##     Gives a default definition of fWRAP_<tag> for each instruction
##
f = StringIO()
for tag in tags:
    f.write( "#ifndef fWRAP_%s\n" % tag )
    f.write( "#define fWRAP_%s(GENHLPR, SHORTCODE) GENHLPR\n" % tag )
    f.write( "#endif\n\n" )
realf = open('qemu_wrap_generated.h', 'wt')
realf.write(f.getvalue())
realf.close()
f.close()

##
## Generate the opcodes_def_generated.h file
##     Gives a list of all the opcodes
##
f = StringIO()
for tag in tags:
    f.write ( "OPCODE(%s),\n" % (tag) )
realf = open('opcodes_def_generated.h', 'wt')
realf.write(f.getvalue())
realf.close()
f.close()

##
## Generate the op_attribs_generated.h file
##     Lists all the attributes associated with each instruction
##
f = StringIO()
for tag in tags:
    f.write('OP_ATTRIB(%s,ATTRIBS(%s))\n' % \
        (tag,string.join(sorted(attribdict[tag]),",")))
realf = open('op_attribs_generated.h', 'wt')
realf.write(f.getvalue())
realf.close()
f.close()

##
## Generate the op_regs_generated.h file
##     Lists the register and immediate operands for each instruction
##
def calculate_regid_reg(tag):
    def letter_inc(x): return chr(ord(x)+1)
    ordered_implregs = [ 'SP','FP','LR' ]
    srcdst_lett = 'X'
    src_lett = 'S'
    dst_lett = 'D'
    retstr = ""
    mapdict = {}
    for reg in ordered_implregs:
        reg_rd = 0
        reg_wr = 0
        if ('A_IMPLICIT_READS_'+reg) in attribdict[tag]: reg_rd = 1
        if ('A_IMPLICIT_WRITES_'+reg) in attribdict[tag]: reg_wr = 1
        if reg_rd and reg_wr:
            retstr += srcdst_lett
            mapdict[srcdst_lett] = reg
            srcdst_lett = letter_inc(srcdst_lett)
        elif reg_rd:
            retstr += src_lett
            mapdict[src_lett] = reg
            src_lett = letter_inc(src_lett)
        elif reg_wr:
            retstr += dst_lett
            mapdict[dst_lett] = reg
            dst_lett = letter_inc(dst_lett)
    return retstr,mapdict

def calculate_regid_letters(tag):
    retstr,mapdict = calculate_regid_reg(tag)
    return retstr

def strip_verif_info_in_regs(x):
    y=x.replace('UREG.','')
    y=y.replace('MREG.','')
    return y.replace('GREG.','')

f = StringIO()

for tag in tags:
    regs = tagregs[tag]
    rregs = []
    wregs = []
    regids = ""
    for regtype,regid,toss,numregs in regs:
        if is_read(regid):
            if regid[0] not in regids: regids += regid[0]
            rregs.append(regtype+regid+numregs)
        if is_written(regid):
            wregs.append(regtype+regid+numregs)
            if regid[0] not in regids: regids += regid[0]
    for attrib in attribdict[tag]:
        if attribinfo[attrib]['rreg']:
            rregs.append(strip_verif_info_in_regs(attribinfo[attrib]['rreg']))
        if attribinfo[attrib]['wreg']:
            wregs.append(strip_verif_info_in_regs(attribinfo[attrib]['wreg']))
    regids += calculate_regid_letters(tag)
    f.write('REGINFO(%s,"%s",\t/*RD:*/\t"%s",\t/*WR:*/\t"%s")\n' % \
        (tag,regids,",".join(rregs),",".join(wregs)))

for tag in tags:
    imms = tagimms[tag]
    f.write( 'IMMINFO(%s' % tag)
    if not imms:
        f.write(''','u',0,0,'U',0,0''')
    for sign,size,shamt in imms:
        if sign == 'r': sign = 's'
        if not shamt:
            shamt = "0"
        f.write(''','%s',%s,%s''' % (sign,size,shamt))
    if len(imms) == 1:
        if sign.isupper():
            myu = 'u'
        else:
            myu = 'U'
        f.write(''','%s',0,0''' % myu)
    f.write(')\n')

realf = open('op_regs_generated.h','w')
realf.write(f.getvalue())
realf.close()
f.close()

##
## Generate the printinsn_generated.h file
##     Data for printing each instruction (format string + operands)
##
def regprinter(m):
    str = m.group(1)
    str += ":".join(["%d"]*len(m.group(2)))
    str += m.group(3)
    if ('S' in m.group(1)) and (len(m.group(2)) == 1):
        str += "/%s"
    elif ('C' in m.group(1)) and (len(m.group(2)) == 1):
        str += "/%s"
    return str

# Regular expression that matches any operator that contains '=' character:
opswithequal_re = '[-+^&|!<>=]?='
# Regular expression that matches any assignment operator.
assignment_re = '[-+^&|]?='

# Out of the operators that contain the = sign, if the operator is also an
# assignment, spaces will be added around it, unless it's enclosed within
# parentheses, or spaces are already present.

equals = re.compile(opswithequal_re)
assign = re.compile(assignment_re)

def spacify(s):
    slen = len(s)
    paren_count = {}
    i = 0
    pc = 0
    while i < slen:
        c = s[i]
        if c == '(':
            pc += 1
        elif c == ')':
            pc -= 1
        paren_count[i] = pc
        i += 1

    # Iterate over all operators that contain the equal sign. If any
    # match is also an assignment operator, add spaces around it if
    # the parenthesis count is 0.
    pos = 0
    out = []
    for m in equals.finditer(s):
        ms = m.start()
        me = m.end()
        # t is the string that matched opswithequal_re.
        t = m.string[ms:me]
        out += s[pos:ms]
        pos = me
        if paren_count[ms] == 0:
            # Check if the entire string t is an assignment.
            am = assign.match(t)
            if am and len(am.group(0)) == me-ms:
                # Don't add spaces if they are already there.
                if ms > 0 and s[ms-1] != ' ':
                    out.append(' ')
                out += t
                if me < slen and s[me] != ' ':
                    out.append(' ')
                continue
        # If this is not an assignment, just append it to the output
        # string.
        out += t

    # Append the remaining part of the string.
    out += s[pos:len(s)]
    return ''.join(out)

immext_casere = re.compile(r'IMMEXT\(([A-Za-z])')

f = StringIO()

for tag in tags:
    if not behdict[tag]: continue
    extendable_upper_imm = False
    extendable_lower_imm = False
    m = immext_casere.search(semdict[tag])
    if m:
        if m.group(1).isupper():
            extendable_upper_imm = True
        else:
            extendable_lower_imm = True
    beh = behdict[tag]
    beh = regre.sub(regprinter,beh)
    beh = absimmre.sub(r"#%s0x%x",beh)
    beh = relimmre.sub(r"PC+%s%d",beh)
    beh = spacify(beh)
    # Print out a literal "%s" at the end, used to match empty string
    # so C won't complain at us
    if ("A_VECX" in attribdict[tag]): macname = "DEF_VECX_PRINTINFO"
    else: macname = "DEF_PRINTINFO"
    f.write('%s(%s,"%s%%s"' % (macname,tag,beh))
    regs_or_imms = reg_or_immre.findall(behdict[tag])
    ri = 0
    seenregs = {}
    for allregs,a,b,c,d,allimm,immlett,bits,immshift in regs_or_imms:
        if a:
            #register
            if b in seenregs:
                regno = seenregs[b]
            else:
                regno = ri
            if len(b) == 1:
                f.write(',REGNO(%d)' % regno)
                if 'S' in a:
                    f.write(',sreg2str(REGNO(%d))' % regno)
                elif 'C' in a:
                    f.write(',creg2str(REGNO(%d))' % regno)
            elif len(b) == 2:
                f.write(',REGNO(%d)+1,REGNO(%d)' % (regno,regno))
            elif len(b) == 4:
                f.write(',REGNO(%d)^3,REGNO(%d)^2,REGNO(%d)^1,REGNO(%d)' % \
                        (regno,regno,regno,regno))
            else:
                print("Put some stuff to handle quads here")
            if b not in seenregs:
                seenregs[b] = ri
                ri += 1
        else:
            #immediate
            if (immlett.isupper()):
                if extendable_upper_imm:
                    if immlett in 'rR':
                        f.write(',insn->extension_valid?"##":""')
                    else:
                        f.write(',insn->extension_valid?"#":""')
                else:
                    f.write(',""')
                ii = 1
            else:
                if extendable_lower_imm:
                    if immlett in 'rR':
                        f.write(',insn->extension_valid?"##":""')
                    else:
                        f.write(',insn->extension_valid?"#":""')
                else:
                    f.write(',""')
                ii = 0
            f.write(',IMMNO(%d)' % ii)
    # append empty string so there is at least one more arg
    f.write(',"")\n')

realf = open('printinsn_generated.h','w')
realf.write(f.getvalue())
realf.close()
f.close()


