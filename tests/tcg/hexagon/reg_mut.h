/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef REG_MUT_H
#define REG_MUT_H

extern int err;

#define check(N, EXPECT) \
    do { \
        uint64_t value = N; \
        uint64_t expect = EXPECT; \
        if (value != EXPECT) { \
            printf("ERROR: \"%s\" 0x%04llx != 0x%04llx at %s:%d\n", #N, value, \
                   expect, __FILE__, __LINE__); \
            err++; \
        } \
    } while (0)

#define check_ne(N, EXPECT) \
    do { \
        uint64_t value = N; \
        uint64_t expect = EXPECT; \
        if (value == EXPECT) { \
            printf("ERROR: \"%s\" 0x%04llx == 0x%04llx at %s:%d\n", #N, value, \
                   expect, __FILE__, __LINE__); \
            err++; \
        } \
    } while (0)

#define READ_WRITE_REG_NOCLOBBER(cur_val, result, reg_name, new_val) \
    asm volatile("%0 = " reg_name "\n\t" \
                 reg_name " = %2\n\t" \
                 "%1 = " reg_name "\n\t" \
                 : "=r"(cur_val), "=r"(result) \
                 : "r"(new_val) \
                 : )

#define WRITE_REG_NOCLOBBER(output, reg_name, input) \
    asm volatile(reg_name " = %1\n\t" \
                 "%0 = " reg_name "\n\t" \
                 : "=r"(output) \
                 : "r"(input) \
                 : )

#define WRITE_REG_ENCODED(output, reg_name, input, encoding) \
    asm volatile("r0 = %1\n\t" \
                 encoding "\n\t" \
                 "%0 = " reg_name "\n\t" \
                 : "=r"(output) \
                 : "r"(input) \
                 : "r0")

#define WRITE_REG_PAIR_ENCODED(output, reg_name, input, encoding) \
    asm volatile("r1:0 = %1\n\t" \
                 encoding "\n\t" \
                 "%0 = " reg_name "\n\t" \
                 : "=r"(output) \
                 : "r"(input) \
                 : "r1:0")

#endif
