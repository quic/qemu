/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
 * For registers that have individual fields, explain them here
 *   DEF_REG_FIELD(tag,
 *                 name,
 *                 bit start offset,
 *                 width,
 *                 description
 */

/* USR fields */
DEF_REG_FIELD(USR_OVF,
    "ovf", 0, 1,
    "Sticky Saturation Overflow - "
    "Set when saturation occurs while executing instruction that specifies "
    "optional saturation, remains set until explicitly cleared by a USR=Rs "
    "instruction.")
DEF_REG_FIELD(USR_FPINVF,
    "fpinvf", 1, 1,
    "Floating-point IEEE Invalid Sticky Flag.")
DEF_REG_FIELD(USR_FPDBZF,
    "fpdbzf", 2, 1,
    "Floating-point IEEE Divide-By-Zero Sticky Flag.")
DEF_REG_FIELD(USR_FPOVFF,
    "fpovff", 3, 1,
    "Floating-point IEEE Overflow Sticky Flag.")
DEF_REG_FIELD(USR_FPUNFF,
    "fpunff", 4, 1,
    "Floating-point IEEE Underflow Sticky Flag.")
DEF_REG_FIELD(USR_FPINPF,
    "fpinpf", 5, 1,
    "Floating-point IEEE Inexact Sticky Flag.")

DEF_REG_FIELD(USR_LPCFG,
    "lpcfg", 8, 2,
    "Hardware Loop Configuration: "
    "Number of loop iterations (0-3) remaining before pipeline predicate "
    "should be set.")

DEF_REG_FIELD(USR_FPRND,
    "fprnd", 22, 2,
    "Rounding Mode for Floating-Point Instructions: "
    "00: Round to nearest, ties to even (default), "
    "01: Toward zero, "
    "10: Downward (toward negative infinity), "
    "11: Upward (toward positive infinity).")

DEF_REG_FIELD(USR_FPINVE,
    "fpinve", 25, 1,
    "Enable trap on IEEE Invalid.")
DEF_REG_FIELD(USR_FPDBZE,
    "fpdbze", 26, 1, "Enable trap on IEEE Divide-By-Zero.")
DEF_REG_FIELD(USR_FPOVFE,
    "fpovfe", 27, 1,
    "Enable trap on IEEE Overflow.")
DEF_REG_FIELD(USR_FPUNFE,
    "fpunfe", 28, 1,
    "Enable trap on IEEE Underflow.")
DEF_REG_FIELD(USR_FPINPE,
    "fpinpe", 29, 1,
    "Enable trap on IEEE Inexact.")

