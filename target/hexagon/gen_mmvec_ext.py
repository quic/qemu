#!/usr/bin/env python3

##
##  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
from collections import namedtuple

Q6INSN_data = namedtuple('Q6INSN_data', 'beh,attribs,descr,sem')

data = {}
def Q6INSN_REV(rev, tag, beh, attribs, descr, sem):
    global data
    if tag not in data:
        data[tag] = {}
    data[tag][rev] = Q6INSN_data(beh, attribs, descr, sem)

def read_preprocessed_file(name):
    for line in open(name, "rt").readlines():
        if not line.startswith("#"):
            for insn in line.split("Q6INSN_REV_END"):
                insn = insn.strip()
                if insn != '':
                    eval(insn)

def write_to_output(name):
    with open(name, "w") as out:
        for tag, datum in data.items():
            revs = sorted(datum.keys(), reverse=True)
            latest_rev_datum = datum[revs[0]]
            sem = "do { "
            for i, rev in enumerate(revs):
                if i < len(revs) - 1:
                    sem += f"if (HEXAGON_REV_BYTE() >= 0x{rev}) {{ {datum[rev].sem}; }} else "
                else:
                    sem += f"{{ {datum[rev].sem}; }};"
            sem += " } while (0)"
            out.write(f"Q6INSN({tag}, \"{latest_rev_datum.beh}\"," +
                      f" {latest_rev_datum.attribs}," +
                      f" \"{latest_rev_datum.descr}\", {sem})\n")

def main():
    read_preprocessed_file(sys.argv[1])
    write_to_output(sys.argv[2])

if __name__ == "__main__":
    main()
