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

#define HEX_EXCP_NONE                                           -1
#define HEX_EXCP_RESET                                          0x000
#define HEX_EXCP_FETCH_NO_XPAGE                                 0x011
#define HEX_EXCP_FETCH_NO_UPAGE                                 0x012
#define HEX_EXCP_INVALID_PACKET                                 0x015
#define HEX_EXCP_INVALID_OPCODE                                 0x015
#define HEX_EXCP_PRIV_NO_READ                                   0x022
#define HEX_EXCP_PRIV_NO_WRITE                                  0x023
#define HEX_EXCP_PRIV_NO_UREAD                                  0x024
#define HEX_EXCP_PRIV_NO_UWRITE                                 0x025
#define HEX_EXCP_IMPRECISE_MULTI_TLB_MATCH                      0x044
#define HEX_EXCP_TLBMISSX_CAUSE_NORMAL                          0x060
#define HEX_EXCP_TLBMISSX_CAUSE_NEXTPAGE                        0x061
#define HEX_EXCP_TLBMISSRW_CAUSE_READ                           0x070
#define HEX_EXCP_TLBMISSRW_CAUSE_WRITE                          0x071
#define HEX_EXCP_PRIV_USER_NO_SINSN                             0x01b
#define HEX_EXCP_VIC0                                           0x0c2
#define HEX_EXCP_VIC1                                           0x0c3
#define HEX_EXCP_VIC2                                           0x0c4
#define HEX_EXCP_VIC3                                           0x0c5

#define HEX_EXCP_TRAP0                                          0x172
#define HEX_EXCP_TRAP1                                          0x173
#define HEX_EXCP_SC4                                            0x100
#define HEX_EXCP_SC8                                            0x200

#define PACKET_WORDS_MAX         4

extern int disassemble_hexagon(uint32_t *words, int nwords,
                               char *buf, int bufsize);

#endif
