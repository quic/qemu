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
DEF_REG_FIELD(USR_PKTCNT_U,
    "pktcnt_u", 10, 1,
    "Enable packet counting in User mode.")
DEF_REG_FIELD(USR_PKTCNT_G,
    "pktcnt_g", 11, 1,
    "Enable packet counting in Guest mode.")
DEF_REG_FIELD(USR_PKTCNT_M,
    "pktcnt_m", 12, 1,
    "Enable packet counting in Monitor mode.")
DEF_REG_FIELD(USR_HFD,
    "hfd", 13, 2,
    "Two bits that let the user control the amount of L1 hardware data cache "
    "prefetching (up to 4 cache lines): "
    "00: No prefetching, "
    "01: Prefetch Loads with post-updating address mode when execution is "
        "within a hardware loop, "
    "10: Prefetch any hardware-detected striding Load when execution is within "
        "a hardware loop, "
    "11: Prefetch any hardware-detected striding Load.")
DEF_REG_FIELD(USR_HFI,
    "hfi", 15, 2,
    "Two bits that let the user control the amount of L1 instruction cache "
    "prefetching. "
    "00: No prefetching, "
    "01: Allow prefetching of at most 1 additional cache line, "
    "10: Allow prefetching of at most 2 additional cache lines.")

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
DEF_REG_FIELD(USR_PFA,
    "pfa", 31, 1,
    "L2 Prefetch Active: Set when non-blocking l2fetch instruction is "
    "prefetching requested data, remains set until l2fetch prefetch operation "
    "is completed (or not active).") /* read-only */

DEF_REG_FIELD(IPENDAD_IAD,
    "iad", 16, 16,
    "Interrupt-auto disable") /* read-only */
DEF_REG_FIELD(IPENDAD_IPEND,
    "ipend", 0, 16,
    "Interrupt pending") /* read-only */

DEF_REG_FIELD(SCHEDCFG_EN, "en", 8, 1, "re-schedule interrupt enable")
DEF_REG_FIELD(SCHEDCFG_INTNO, "intno", 0, 4, "reschedule interrupt number")

DEF_REG_FIELD(BESTWAIT_PRIO, "prio", 0, 10, "bestwait priority")


/* PTE (aka TLB entry) fields */
DEF_REG_FIELD(PTE_PPD,
    "PPD", 0, 24,
    "Physical page number that the corresponding virtual page maps to.")
DEF_REG_FIELD(PTE_C,
    "C", 24, 4,
    "Cacheability attributes for the page.")
DEF_REG_FIELD(PTE_U,
    "U", 28, 1,
    "User mode permitted.")
DEF_REG_FIELD(PTE_R,
    "R", 29, 1,
    "Read-enable.")
DEF_REG_FIELD(PTE_W,
    "W", 30, 1,
    "Write-enable.")
DEF_REG_FIELD(PTE_X,
    "X", 31, 1,
    "Execute-enable.")
DEF_REG_FIELD(PTE_VPN,
    "VPN", 32, 20,
    "Virtual page number that is matched against the load or store address.")
DEF_REG_FIELD(PTE_ASID,
    "ASID", 52, 7,
    "7-bit address space identifier (tag extender)")
DEF_REG_FIELD(PTE_ATR0,
    "ATR0", 59, 1,
    "General purpose attribute bit kept as an attribute of each cache line.")
DEF_REG_FIELD(PTE_ATR1,
    "ATR1", 60, 1,
    "General purpose attribute bit kept as an attribute of each cache line.")
DEF_REG_FIELD(PTE_PA35,
    "PA35", 61, 1,
    "The Extra Physical bit is the most-significant physical address bit.")
DEF_REG_FIELD(PTE_G,
    "G", 62, 1,
    "Global bit. If set, then the ASID is ignored in the match.")
DEF_REG_FIELD(PTE_V,
    "V", 63, 1,
    "Valid bit. indicates whether this entry should be used for matching.")

/* SYSCFG fields */
DEF_REG_FIELD(SYSCFG_MMUEN,
    "MMUEN", 0, 1,
    "read-write bit that enables the MMU.")
DEF_REG_FIELD(SYSCFG_ICEN,
    "ICEN", 1, 1,
    "read-write bit that enables the instruction cache.")
DEF_REG_FIELD(SYSCFG_DCEN,
    "DCEN", 2, 1,
    "read-write bit that enables the data cache.")
DEF_REG_FIELD(SYSCFG_ISDBTRUSTED,
    "ISDBTRUSTED", 3, 1,
    "read-write bit that controls ISDB TRUSTED behavior.")
DEF_REG_FIELD(SYSCFG_GIE,
    "GIE", 4, 1,
    "read-write global interrupt enable bit.")
DEF_REG_FIELD(SYSCFG_ISDBREADY,
    "ISDBCOREREADY", 5, 1,
    "read-write bit that controls ISDB_CORE_READY.")
DEF_REG_FIELD(SYSCFG_PCYCLEEN,
    "PCYCLEEN", 6, 1,
    "read-write bit that controls the operation of the 64-bit PCYCLE counter.")
DEF_REG_FIELD(SYSCFG_V2X,
    "V2X", 7, 1,
    "read-write bit reserved for use by an optional coprocessor.")
DEF_REG_FIELD(SYSCFG_IGNOREDABORT,
    "IDA", 8, 1,
    "read-write bit that causes data aborts to be ignored when set.")
DEF_REG_FIELD(SYSCFG_PM,
    "PM", 9, 1,
    "Enables the Performance Monitor Unit (PMU).")
DEF_REG_FIELD(SYSCFG_TLBLOCK,
    "TL", 11, 1,
    "Control the operation of the hardware TLB lock.")
DEF_REG_FIELD(SYSCFG_K0LOCK,
    "KL", 12, 1,
    "Control the operation of the hardware kernel lock.")
DEF_REG_FIELD(SYSCFG_BQ,
    "BQ", 13, 1,
    "Controls the bus Quality-of-Service.")
DEF_REG_FIELD(SYSCFG_PRIO,
    "PRIO", 14, 1,
    "Controls the Hardware Priority Scheduling feature.")
DEF_REG_FIELD(SYSCFG_DMT,
    "DMT", 15, 1,
    "Controls Multi-Thread Scheduling.")
DEF_REG_FIELD(SYSCFG_L2CFG,
    "L2CFG", 16, 3,
    "read-write bits that configure L2 cache TCM/CACHE partition.")
DEF_REG_FIELD(SYSCFG_ITCM,
    "ITCM", 19, 1,
    "read-write bit that enables the Instruction TCM.")
DEF_REG_FIELD(SYSCFG_CLADEN,
    "CLADEN", 20, 1,
    "CLADE Cache Compression Enable.")
DEF_REG_FIELD(SYSCFG_L2NWA,
    "L2NWA", 21, 1,
    "When set, L2 is configured as no-write-allocate.")
DEF_REG_FIELD(SYSCFG_L2NRA,
    "L2NRA", 22, 1,
    "When set, L2 is configured as no-read-allocate.")
DEF_REG_FIELD(SYSCFG_L2WB,
    "L2WB", 23, 1,
    "When set, L2 is configured as Write-back.")
DEF_REG_FIELD(SYSCFG_L2P,
    "L2P", 24, 1,
    "read-write bit that enables L2 memory parity protection.")
DEF_REG_FIELD(SYSCFG_SLVCTL0,
    "SLVCTL0", 25, 2,
    "Slave 0 Control - Sets the behavior of the slave.")
DEF_REG_FIELD(SYSCFG_SLVCTL1,
    "SLVCTL1", 27, 2,
    "Slave 0 Control - Sets the behavior of the slave.")
DEF_REG_FIELD(SYSCFG_L2PARTSIZE,
    "L2PART", 29, 2,
    "read-write bits that control partitioning for the L2 cache.")
DEF_REG_FIELD(SYSCFG_L2GCA,
    "L2GCA", 31, 1,
    "Indicates that an L2 Global Cacheop is active.")

/* SSR fields */
DEF_REG_FIELD(SSR_CAUSE,
    "cause", 0, 8,
    "8-bit field that contains the reason for various exception.")
DEF_REG_FIELD(SSR_ASID,
    "asid", 8, 7,
    "7-bit field that contains the Address Space Identifier.")
DEF_REG_FIELD(SSR_UM,
    "um", 16, 1,
    "read-write bit.")
DEF_REG_FIELD(SSR_EX,
    "ex", 17, 1,
    "set when an interrupt or exception is accepted.")
DEF_REG_FIELD(SSR_IE,
    "ie", 18, 1,
    "indicates whether the global interrupt is enabled.")
DEF_REG_FIELD(SSR_GM,
    "gm", 19, 1,
    "Guest mode bit.")
DEF_REG_FIELD(SSR_V0,
    "v0", 20, 1,
    "if BADVA0 register contents are from a valid slot 0 instruction.")
DEF_REG_FIELD(SSR_V1,
     "v1", 21, 1,
    "if BADVA1 register contents are from a valid slot 1 instruction.")
DEF_REG_FIELD(SSR_BVS,
    "bvs", 22, 1,
    "BADVA Selector.")
DEF_REG_FIELD(SSR_CE,
    "ce", 23, 1,
    "grants user or guest read permissions to the PCYCLE register aliases.")
DEF_REG_FIELD(SSR_PE,
    "pe", 24, 1,
    "grants guest read permissions to the PMU register aliases.")
DEF_REG_FIELD(SSR_BP,
    "bp", 25, 1,
    "Internal Bus Priority bit.")
DEF_REG_FIELD(SSR_XA,
    "xa", 27, 3,
    "Extension Active, which control operation of an attached coprocessor.")
DEF_REG_FIELD(SSR_SS,
    "ss", 30, 1,
    "Single Step, which enables single-step exceptions.")
DEF_REG_FIELD(SSR_XE,
    "xe", 31, 1,
    "Coprocessor Enable, which enables use of an attached coprocessor.")

/* misc registers */
DEF_REG_FIELD(IMASK_MASK,
    "mask",0,16,
    "IMASK is an 8-bit read-write register "
    "used to hold a per-thread interrupt mask.")

DEF_REG_FIELD(STID_PRIO,
    "prio",16,8,
    "PRIO is an 8-bit read-write register "
    "used to hold a thread priority mask.")
DEF_REG_FIELD(STID_STID,
    "stid",0,8,
    "STID is an 8-bit read-write register "
    "used to hold a software thread-id.")

/* MODECTL fields */
DEF_REG_FIELD(MODECTL_E,
    "Enabled",0,8,
    "The MODECTL register reflects the current processing mode of all threads. "
    "E bits when set mean that the corresponding thread is on.")
DEF_REG_FIELD(MODECTL_W,
    "Wait",16,8,
    "W bit, when set means the thread, if on, is in wait mode.")

/* ISDB ST fields */
DEF_REG_FIELD(ISDBST_WAITRUN,
    "WAITRUN",24,8,
    "These bits indicate which threads are in wait vs. run mode."
    " They reflect the W bit field of the core MODECTL register. Bit 24 is "
    "used for TNUM0, bit 25 for TNUM1, etc.")
DEF_REG_FIELD(ISDBST_ONOFF,
    "ONOFF",16,8,
    "These bits indicate which threads are in Off mode. "
    "They reflect the E bit field of the core MODECTL register. Bit 16 is "
    "used for TNUM0, bit 17 for TNUM1, etc. A thread that is off cannot be "
    "debugged, e.g., if ISDB commands are sent to a thread that is off then "
    "the results are undefined.")
DEF_REG_FIELD(ISDBST_DEBUGMODE,
    "DEBUGMODE",8,8,
    "These bits indicate which threads are in Debug mode. "
    "Bit 8 is used for thread0, bit 9 for thread1, etc. If these bits "
    "indicate a thread is in Debug mode, then the wait/run mode bit (above) "
    "indicates the mode before entering Debug mode.")
DEF_REG_FIELD(ISDBST_STUFFSTATUS,
    "STUFFSTATUS",5,1,
    "0: Stuff instruction was successful, 1: "
    "Stuff instruction caused an exception.")
DEF_REG_FIELD(ISDBST_CMDSTATUS,
    "CMDSTATUS",4,1,
    "0: ISDB command status was successful, 1: ISDB command failed. "
    "It could be from ISDB_TRUSTED settings or because the thread experienced "
    "a TLB miss and could not proceed because another thread was holding "
    "the TLB lock.")
DEF_REG_FIELD(ISDBST_PROCMODE,
    "PROCMODE",3,1,"")
DEF_REG_FIELD(ISDBST_MBXINSTATUS,
    "MBXINSTATUS",2,1,
    "ISDB mailbox in (ISDB --> core) status. This bit is cleared (0) when "
    "the core reads the mailbox in register meaning empty, and set (1) when "
    "the written by APB, meaning full.")
DEF_REG_FIELD(ISDBST_MBXOUTSTATUS,
    "MBXOUTSTATUS",1,1,
    "ISDB mailbox out (core --> ISDB) status. This bit is set (1) when "
    "written by the core meaning full and cleared (0) when read by "
    "APB, meaning empty.")
DEF_REG_FIELD(ISDBST_READY,
    "READY",0,1,
    "0: ISDB is busy with a command, 1: ISDB ready to accept a new command.")

DEF_REG_FIELD(CCR_L1ICP,
    "l1icp",0,2,
    "")
DEF_REG_FIELD(CCR_L1DCP,
    "l1dcp",3,2,
    "")
DEF_REG_FIELD(CCR_L2CP,
    "l2cp",6,2,
    "")

DEF_REG_FIELD(CCR_HFI,
    "hfi",16,1,
    "If set (1), honor the USR[HFI] settings, otherwise disable hardware "
    "prefetching into I$.")
DEF_REG_FIELD(CCR_HFD,
    "hfd",17,1,
    "If set (1), honor the USR[HFD] settings, otherwise disable hardware "
    "prefetching into D$.")
DEF_REG_FIELD(CCR_HFIL2,
    "hfil2",18,1,
    "This feature is deprecated. These bits are reserved and "
    "should not be set.")
DEF_REG_FIELD(CCR_HFDL2,
    "hfdl2",19,1,
    "This bit is reserved. For compatibility with earlier versions, it is a "
    "read-write bit, but should always be written with zero to ensure "
    "forward compatibility.")
DEF_REG_FIELD(CCR_SFD,
    "sfd",20,1,
    "If clear (0), the dcfetch and l2fetch instructions are treated as a "
    "NOPs. If set (1), dcfetch and l2fetch attempt to prefetch data based "
    "on the cacheability (CCCC) and partition (L1DCP, L2CP, L1DP, L2PART) "
    "settings")

DEF_REG_FIELD(CCR_GIE,
    "gie",24,1,
    "When set, enables interrupts in guest mode.")
DEF_REG_FIELD(CCR_GTE,
    "gte",25,1,
    "When set, enables trap0 in guest mode.")
DEF_REG_FIELD(CCR_GEE,
    "gee",26,1,
    "When set, enables error (?) in guest mode.")
DEF_REG_FIELD(CCR_GRE,
    "gre",27,1,
    "When set, enables return and get/set interrupt in guest mode.")
DEF_REG_FIELD(CCR_VV1,
    "vv1",29,1,
    "Virtualize VIC 1")
DEF_REG_FIELD(CCR_VV2,
    "vv2",30,1,
    "Virtualize VIC 2")
DEF_REG_FIELD(CCR_VV3,
    "vv3",31,1,
    "Virtualize VIC 3")

