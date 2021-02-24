/*
 * Qualcomm QCT QTimer
 *
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
#ifndef QTIMER_TIMER_H
#define QTIMER_TIMER_H

#define QTMR_AC_CNTFRQ (0x000)
#define QTMR_AC_CNTSR (0x004)
#define QTMR_AC_CNTSR_NSN_1 (1 << 0)
#define QTMR_AC_CNTSR_NSN_2 (1 << 1)
#define QTMR_AC_CNTTID (0x08)
#define QTMR_AC_CNTACR_0 (0x40)
#define QTMR_AC_CNTACR_1 (0x44)
#define QTMR_AC_CNTACR_RWPT (1 << 5) /* R/W of CNTP_* regs */
#define QTMR_AC_CNTACR_RWVT (1 << 4) /* R/W of CNTV_* regs */
#define QTMR_AC_CNTACR_RVOFF (1 << 3) /* R/W of CNTVOFF register */
#define QTMR_AC_CNTACR_RFRQ (1 << 2) /* R/W of CNTFRQ register */
#define QTMR_AC_CNTACR_RPVCT (1 << 1) /* R/W of CNTVCT register */
#define QTMR_AC_CNTACR_RPCT (1 << 0) /* R/W of CNTPCT register */
#define QTMR_VERSION (0x0fd0)
#define QTMR_CNTPCT_LO (0x000)
#define QTMR_CNTPCT_HI (0x004)
#define QTMR_CNT_FREQ (0x010)
#define QTMR_CNTP_CVAL_LO (0x020)
#define QTMR_CNTP_CVAL_HI (0x024)
#define QTMR_CNTP_TVAL (0x028)
#define QTMR_CNTP_CTL (0x02c)
#define QTMR_CNTP_CTL_ISTAT (1 << 2)
#define QTMR_CNTP_CTL_INTEN (1 << 1)
#define QTMR_CNTP_CTL_ENABLE (1 << 0)

#define QTIMER_MEM_SIZE_BYTES 0x1000
#define QTIMER_MEM_REGION_SIZE_BYTES 0x1000

#define HEX_TIMER_DEBUG 0
#define HEX_TIMER_LOG(...) \
    do { \
        if (HEX_TIMER_DEBUG) { \
            rcu_read_lock(); \
            fprintf(stderr, __VA_ARGS__); \
            rcu_read_unlock(); \
        } \
    } while (0)

#endif
