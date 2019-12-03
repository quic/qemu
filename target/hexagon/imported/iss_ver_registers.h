/*
 *  Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#define HEX_REG_R00 0
#define HEX_REG_R01 1
#define HEX_REG_R02 2
#define HEX_REG_R03 3
#define HEX_REG_R04 4
#define HEX_REG_R05 5
#define HEX_REG_R06 6
#define HEX_REG_R07 7
#define HEX_REG_R08 8
#define HEX_REG_R09 9
#define HEX_REG_R10 10
#define HEX_REG_R11 11
#define HEX_REG_R12 12
#define HEX_REG_R13 13
#define HEX_REG_R14 14
#define HEX_REG_R15 15
#define HEX_REG_R16 16
#define HEX_REG_R17 17
#define HEX_REG_R18 18
#define HEX_REG_R19 19
#define HEX_REG_R20 20
#define HEX_REG_R21 21
#define HEX_REG_R22 22
#define HEX_REG_R23 23
#define HEX_REG_R24 24
#define HEX_REG_R25 25
#define HEX_REG_R26 26
#define HEX_REG_R27 27
#define HEX_REG_R28 28
#define HEX_REG_R29 29
#define HEX_REG_SP 29
#define HEX_REG_FP 30
#define HEX_REG_R30 30
#define HEX_REG_LR 31
#define HEX_REG_R31 31
#define HEX_REG_SA0 32
#define HEX_REG_LC0 33
#define HEX_REG_SA1 34
#define HEX_REG_LC1 35
#define HEX_REG_P3_0 36
#define HEX_REG_M0 38
#define HEX_REG_M1 39
#define HEX_REG_USR 40
#define HEX_REG_PC 41
#define HEX_REG_UGP 42
#define HEX_REG_GP 43
#define HEX_REG_CS0 44
#define HEX_REG_CS1 45
#define HEX_REG_UPCYCLELO 46
#define HEX_REG_UPCYCLEHI 47
#define HEX_REG_FRAMELIMIT 48
#define HEX_REG_FRAMEKEY 49
#define HEX_REG_PKTCNTLO 50
#define HEX_REG_PKTCNTHI 51
/* Use reserved control registers for qemu execution counts */
#define HEX_REG_QEMU_PKT_CNT 52
#define HEX_REG_QEMU_INSN_CNT 53
#define HEX_REG_QEMU_HVX_CNT 54
#define HEX_REG_UTIMERLO 62
#define HEX_REG_UTIMERHI 63
#define HEX_REG_SGP0 64
#define HEX_REG_SGP1 65
#define HEX_REG_STID 66
#define HEX_REG_ELR 67
#define HEX_REG_BADVA0 68
#define HEX_REG_BADVA1 69
#define HEX_REG_SSR 70
#define HEX_REG_CCR 71
#define HEX_REG_HTID 72
#define HEX_REG_BADVA 73
#define HEX_REG_IMASK 74
#define HEX_REG_GEVB 75

/*
 * Not used in qemu
 */
#ifdef FIXME
// New interrupts, keep old defines for the time being
#define HEX_REG_GELR 80
#define HEX_REG_GSR 81 
#define HEX_REG_GOSP 82
#define HEX_REG_GBADVA 83

#define HEX_REG_G0 80
#define HEX_REG_G1 81
#define HEX_REG_G2 82
#define HEX_REG_G3 83

#define HEX_REG_G4 84
#define HEX_REG_G5 85
#define HEX_REG_G6 86
#define HEX_REG_G7 87
#define HEX_REG_G8 88
#define HEX_REG_G9 89
#define HEX_REG_G10 90
#define HEX_REG_G11 91
#define HEX_REG_G12 92
#define HEX_REG_G13 93
#define HEX_REG_G14 94
#define HEX_REG_G15 95
#define HEX_REG_G16 96
#define HEX_REG_G17 97
#define HEX_REG_G18 98
#define HEX_REG_G19 99
#define HEX_REG_G20 100
#define HEX_REG_G21 101
#define HEX_REG_G22 102
#define HEX_REG_G23 103
#define HEX_REG_G24 104
#define HEX_REG_G25 105
#define HEX_REG_G26 106
#define HEX_REG_G27 107
#define HEX_REG_G28 108
#define HEX_REG_G29 109
#define HEX_REG_G30 110
#define HEX_REG_G31 111
#define GREG_EVB 0
#define GREG_MODECTL 1
#define GREG_SYSCFG 2
#define GREG_EVB1 3
#define GREG_IPENDAD 4
#define GREG_VID 5
#define GREG_VID1 6
#define GREG_BESTWAIT 7
#define GREG_SCHEDCFG 9
#define GREG_CFGBASE 11
#define GREG_DIAG 12
#define GREG_REV 13
#define GREG_PCYCLELO 14
#define GREG_PCYCLEHI 15
#define GREG_ISDBST 16
#define GREG_ISDBCFG0 17
#define GREG_ISDBCFG1 18
#define GREG_LIVELOCK 19
#define GREG_BRKPTPC0 20
#define GREG_BRKPTCFG0 21
#define GREG_BRKPTPC1 22
#define GREG_BRKPTCFG1 23
#define GREG_ISDBMBXIN 24
#define GREG_ISDBMBXOUT 25
#define GREG_ISDBEN 26
#define GREG_ISDBGPR 27
#define GREG_PMUCNT4 28
#define GREG_PMUCNT5 29
#define GREG_PMUCNT6 30
#define GREG_PMUCNT7 31
#define GREG_PMUCNT0 32
#define GREG_PMUCNT1 33
#define GREG_PMUCNT2 34
#define GREG_PMUCNT3 35
#define GREG_PMUEVTCFG 36
#define GREG_PMUSTID0 37
#define GREG_PMUEVTCFG1 38
#define GREG_PMUSTID1 39
#define GREG_TIMERLO 40
#define GREG_TIMERHI 41
#define GREG_PMUCFG 42
#define GREG_AVS 43
// GREG_AVS renamed to GREG_RGDR2 in v67
#define GREG_RGDR2 43
#define GREG_RGDR 44
#define GREG_TURKEY 45
#define GREG_DUCK 46
#define GREG_CHICKEN 47
#define GREG_COMMIT1T 48
#define GREG_COMMIT2T 49
#define GREG_COMMIT3T 50
#define GREG_COMMIT4T 51
#define GREG_COMMIT5T 52
#define GREG_COMMIT6T 53
#define GREG_PCYCLE1T 54
#define GREG_PCYCLE2T 55
#define GREG_PCYCLE3T 56
#define GREG_PCYCLE4T 57
#define GREG_PCYCLE5T 58
#define GREG_PCYCLE6T 59
#define GREG_STFINST  60
#define GREG_ISDBCMD  61
#define GREG_ISDBVER  62
#define GREG_BRKPTINFO 63
#endif
