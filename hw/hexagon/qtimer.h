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
typedef enum {
    QTMR_AC_CNTFRQ = 0x0,
    QTMR_AC_CNTSR = 0x4,
    QTMR_AC_CNTTID = 0x8,
    QTMR_AC_CNTACR_0 = 0x40,
    QTMR_AC_CNTACR_1 = 0x44,
    QTMR_AC_CNTVOFF_LO_0 = 0x80,
    QTMR_AC_CNTVOFF_HI_0 = 0x84,
    QTMR_AC_CNTVOFF_LO_1 = 0x88,
    QTMR_AC_CNTVOFF_HI_1 = 0x8C,
    QTMR_VERSION = 0xFD0,

// Segment 1,
    QTMR_FRAME0_V1_CNTPCT_LO = 0x1000,
    QTMR_FRAME0_V1_CNTPCT_HI = 0x1004,
    QTMR_FRAME0_V1_CNTVCT_LO = 0x1008,
    QTMR_FRAME0_V1_CNTVCT_HI = 0x100C,
    QTMR_FRAME0_V1_CNTFRQ = 0x1010,
    QTMR_FRAME0_V1_CNTPL0ACR = 0x1014,
    QTMR_FRAME0_V1_CNTVOFF_LO = 0x1018,
    QTMR_FRAME0_V1_CNTVOFF_HI = 0x101C,
    QTMR_FRAME0_V1_CNTP_CVAL_LO = 0x1020,
    QTMR_FRAME0_V1_CNTP_CVAL_HI = 0x1024,
    QTMR_FRAME0_V1_CNTP_TVAL = 0x1028,
    QTMR_FRAME0_V1_CNTP_CTL = 0x102C,
    QTMR_FRAME0_V1_CNTV_CVAL_LO = 0x1030,
    QTMR_FRAME0_V1_CNTV_CVAL_HI = 0x1034,
    QTMR_FRAME0_V1_CNTV_TVAL = 0x1038,
    QTMR_FRAME0_V1_CNTV_CTL = 0x103C,

// Segment 2,
    QTMR_FRAME1_V1_CNTPCT_LO = 0x2000,
    QTMR_FRAME1_V1_CNTPCT_HI = 0x2004,
    QTMR_FRAME1_V1_CNTVCT_LO = 0x2008,
    QTMR_FRAME1_V1_CNTVCT_HI = 0x200C,
    QTMR_FRAME1_V1_CNTFRQ = 0x2010,
    QTMR_FRAME1_V1_CNTPL0ACR = 0x2014,
    QTMR_FRAME1_V1_CNTVOFF_LO = 0x2018,
    QTMR_FRAME1_V1_CNTVOFF_HI = 0x201C,
    QTMR_FRAME1_V1_CNTP_CVAL_LO = 0x2020,
    QTMR_FRAME1_V1_CNTP_CVAL_HI = 0x2024,
    QTMR_FRAME1_V1_CNTP_TVAL = 0x2028,
    QTMR_FRAME1_V1_CNTP_CTL = 0x202C,
    QTMR_FRAME1_V1_CNTV_CVAL_LO = 0x2030,
    QTMR_FRAME1_V1_CNTV_CVAL_HI = 0x2034,
    QTMR_FRAME1_V1_CNTV_TVAL = 0x2038,
    QTMR_FRAME1_V1_CNTV_CTL = 0x203C,
} QTMRRegister;

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
