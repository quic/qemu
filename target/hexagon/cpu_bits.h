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

#ifndef HEXAGON_CPU_BITS_H
#define HEXAGON_CPU_BITS_H

#define HEX_CAUSE_NONE                      -1
#define HEX_CAUSE_RESET                     0x000
#define HEX_CAUSE_TRAP0                     0x008
#define HEX_CAUSE_TRAP1                     0x009
#define HEX_CAUSE_FETCH_NO_XPAGE            0x011
#define HEX_CAUSE_FETCH_NO_UPAGE            0x012
#define HEX_CAUSE_INVALID_PACKET            0x015
#define HEX_CAUSE_INVALID_OPCODE            0x015
#define HEX_CAUSE_PC_NOT_ALIGNED            0x01e
#define HEX_CAUSE_MISALIGNED_LOAD           0x020
#define HEX_CAUSE_MISALIGNED_STORE          0x021
#define HEX_CAUSE_PRIV_NO_READ              0x022
#define HEX_CAUSE_PRIV_NO_READ              0x022
#define HEX_CAUSE_PRIV_NO_WRITE             0x023
#define HEX_CAUSE_PRIV_NO_UREAD             0x024
#define HEX_CAUSE_PRIV_NO_UWRITE            0x025
#define HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH 0x044
#define HEX_CAUSE_TLBMISSX_CAUSE_NORMAL     0x060
#define HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE   0x061
#define HEX_CAUSE_TLBMISSRW_CAUSE_READ      0x070
#define HEX_CAUSE_TLBMISSRW_CAUSE_WRITE     0x071
#define HEX_CAUSE_PRIV_USER_NO_GINSN        0x01a
#define HEX_CAUSE_PRIV_USER_NO_SINSN        0x01b
#define HEX_CAUSE_INT0                      0x0c0
#define HEX_CAUSE_INT1                      0x0c1
#define HEX_CAUSE_INT2                      0x0c2
#define HEX_CAUSE_INT3                      0x0c3
#define HEX_CAUSE_INT4                      0x0c4
#define HEX_CAUSE_INT5                      0x0c5
#define HEX_CAUSE_INT6                      0x0c6
#define HEX_CAUSE_INT7                      0x0c7
#define HEX_CAUSE_VIC0                      0x0c2
#define HEX_CAUSE_VIC1                      0x0c3
#define HEX_CAUSE_VIC2                      0x0c4
#define HEX_CAUSE_VIC3                      0x0c5

#define HEX_EVENT_NONE                      -1
#define HEX_EVENT_RESET                     0x0
#define HEX_EVENT_IMPRECISE                 0x1
#define HEX_EVENT_PRECISE                   0x2
#define HEX_EVENT_TLB_MISS_X                0x4
#define HEX_EVENT_TLB_MISS_RW               0x6
#define HEX_EVENT_TRAP0                     0x8
#define HEX_EVENT_TRAP1                     0x9
#define HEX_EVENT_FPTRAP                    0xb
#define HEX_EVENT_DEBUG                     0xc
#define HEX_EVENT_INT0                      0x10
#define HEX_EVENT_INT1                      0x11
#define HEX_EVENT_INT2                      0x12
#define HEX_EVENT_INT3                      0x13
#define HEX_EVENT_INT4                      0x14
#define HEX_EVENT_INT5                      0x15
#define HEX_EVENT_INT6                      0x16
#define HEX_EVENT_INT7                      0x17
#define HEX_EVENT_TLBLOCK_WAIT              0x300
#define HEX_EVENT_K0LOCK_WAIT               0x400

#define PACKET_WORDS_MAX                    4

extern int disassemble_hexagon(uint32_t *words, int nwords,
                               char *buf, int bufsize);

#endif
