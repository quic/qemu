#!/usr/bin/env python3

##
##  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

behdict = {}          # tag ->behavior
semdict = {}          # tag -> semantics
extdict = {}          # tag -> What extension an instruction belongs to (or "")
extnames = {}         # ext name -> True
attribdict = {}       # tag -> attributes
macros = {}           # macro -> macro information...
attribinfo = {}       # Register information and misc
tags = []             # list of all tags
overrides = {}        # tags with helper overrides

def is_sysemu_tag(tag):
    return ("A_PRIV" in attribdict[tag] or
            "A_GUEST" in attribdict[tag])

def tag_ignore(tag):
    tag_skips = (
        'Y6_diag',
        'Y6_diag0',
        'Y6_diag1',
#        'V6_v6mpyhubs10_vxx',
#        'V6_v6mpyvubs10_vxx',
    )
    attr_skips = (
        'A_FAKEINSN',
        'A_MAPPING',
    )
    return tag in tag_skips or any(attr in attribdict[tag] for attr in attr_skips)

def tag_skip(tag):
    tag_skips = (
    )
    attr_skips = (
#        'A_FAKEINSN',
#        'A_MAPPING',
    )
    return tag in tag_skips or any(attr in attribdict[tag] for attr in attr_skips)

def get_sys_tags():
    return sorted(tag for tag in frozenset(tags) if is_sysemu_tag(tag) and not tag_skip(tag))

def get_user_tags():
    return sorted(tag for tag in frozenset(tags) if not is_sysemu_tag(tag) and not tag_skip(tag))

def get_all_tags():
    return get_user_tags() + get_sys_tags()

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

# When qemu needs an attribute that isn't in the imported files,
# we'll add it here.
def add_qemu_macro_attrib(name, attrib, ext=""):
    key = name + ":" + ext
    macros[key].attribs.add(attrib)

immextre = re.compile(r'f(MUST_)?IMMEXT[(]([UuSsRr])')
def calculate_attribs():
    add_qemu_macro_attrib('fREAD_PC', 'A_IMPLICIT_READS_PC')
    add_qemu_macro_attrib('fTRAP', 'A_IMPLICIT_READS_PC')
    add_qemu_macro_attrib('fWRITE_P0', 'A_WRITES_PRED_REG')
    add_qemu_macro_attrib('fWRITE_P1', 'A_WRITES_PRED_REG')
    add_qemu_macro_attrib('fWRITE_P2', 'A_WRITES_PRED_REG')
    add_qemu_macro_attrib('fWRITE_P3', 'A_WRITES_PRED_REG')
    add_qemu_macro_attrib('fSET_OVERFLOW', 'A_IMPLICIT_WRITES_USR')
    add_qemu_macro_attrib('fSET_LPCFG', 'A_IMPLICIT_WRITES_USR')

    add_qemu_macro_attrib('fLOAD_LOCKED', 'A_LLSC')
    add_qemu_macro_attrib('fSTORE_LOCKED', 'A_LLSC')
    add_qemu_macro_attrib('fCLEAR_RTE_EX', 'A_IMPLICIT_WRITES_SSR')

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

    # Figure out which instructions write predicate registers
    tagregs = get_tagregs()
    for tag in tags:
        regs = tagregs[tag]
        for regtype, regid, _, numregs in regs:
            if regtype == "P" and is_written(regid):
                attribdict[tag].add('A_WRITES_PRED_REG')

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
def get_tagregs():
    return dict(zip(tags, list(map(compute_tag_regs, tags))))

def get_tagimms():
    return dict(zip(tags, list(map(compute_tag_immediates, tags))))

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
    return regtype in "RPCGS"

def is_sreg(regtype):
    return regtype == "S"

def is_greg(regtype):
    return regtype == "G"

def is_hvx_reg(regtype):
    return regtype in "VQ"

def is_old_val(regtype, regid, tag):
    there = regtype+regid+'V' in semdict[tag]
    return regtype+regid+'V' in semdict[tag]

def is_new_val(regtype, regid, tag):
    return regtype+regid+'N' in semdict[tag]

def need_slot(tag):
    if ('A_CONDEXEC' in attribdict[tag] or
        'A_STORE' in attribdict[tag] or
        'A_LOAD' in attribdict[tag] or
        'A_CVI' in attribdict[tag] or
        'A_HMX' in attribdict[tag]):
        return 1
    else:
        return 0

def need_part1(tag):
    return re.compile(r"fPART1").search(semdict[tag])

def need_ea(tag):
    return re.compile(r"\bEA\b").search(semdict[tag])

def skip_qemu_helper(tag):
    return tag in overrides.keys()

def is_tmp_result(tag):
    return ('A_CVI_TMP' in attribdict[tag] or
            'A_CVI_TMP_DST' in attribdict[tag])

def is_hmx(tag):
    return ('A_HMX' in attribdict[tag])

def is_scatter_gather(tag):
    return ('A_CVI_SCATTER' in attribdict[tag] or 'A_CVI_SCATTER_RELEASE' in attribdict[tag] or 'A_CVI_GATHER' in attrbdict[tag]);

def is_gather(tag):
    return ('A_CVI_GATHER' in attribdict[tag]);

def is_hmx_act(tag):
    return ('A_HMX' in attribdict[tag] and 'A_PAIR_1OF2' in attribdict[tag]);

def is_hmx(tag):
    return ('A_HMX' in attribdict[tag])

def is_scatter_gather(tag):
    return ('A_CVI_SCATTER' in attribdict[tag] or 'A_CVI_SCATTER_RELEASE' in attribdict[tag] or 'A_CVI_GATHER' in attribdict[tag]);

def is_gather(tag):
    return ('A_CVI_GATHER' in attribdict[tag]);

def imm_name(immlett):
    return "%siV" % immlett

def read_semantics_file(name):
    eval_line = ""
    for line in open(name, 'rt').readlines():
        if not line.startswith("#"):
            eval_line += line
            if line.endswith("\\\n"):
                eval_line.rstrip("\\\n")
            else:
                eval(eval_line.strip())
                eval_line = ""

def read_attribs_file(name):
    attribre = re.compile(r'DEF_ATTRIB\(([A-Za-z0-9_]+),\s*([^,]*),\s*' +
        r'"([A-Za-z0-9_\.]*)",\s*"([A-Za-z0-9_\.]*)"\)')
    for line in open(name, 'rt').readlines():
        if not attribre.match(line):
            continue
        (attrib_base,descr,rreg,wreg) = attribre.findall(line)[0]
        attrib_base = 'A_' + attrib_base
        attribinfo[attrib_base] = {'rreg':rreg, 'wreg':wreg, 'descr':descr}

def read_overrides_file(name):
    overridere = re.compile("#define fGEN_TCG_([A-Za-z0-9_]+)\(.*")
    for line in open(name, 'rt').readlines():
        if not overridere.match(line):
            continue
        tag = overridere.findall(line)[0]
        overrides[tag] = True
