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

#ifndef HEXAGON_INTERNAL_H
#define HEXAGON_INTERNAL_H

/*
 * Change HEX_DEBUG to 1 to turn on debugging output
 */
#define HEX_DEBUG 0
#define HEX_DEBUG_LOG(...) \
    do { \
        if (HEX_DEBUG) { \
            rcu_read_lock(); \
            fprintf(stderr, __VA_ARGS__); \
            rcu_read_unlock(); \
        } \
    } while (0)

/*
 * Change COUNT_HEX_HELPERS to 1 to count how many times each helper
 * is called.  This is useful to figure out which helpers would benefit
 * from writing an fWRAP macro.
 */
#define COUNT_HEX_HELPERS 0

extern void QEMU_NORETURN do_raise_exception_err(CPUHexagonState *env,
                                                 uint32_t exception,
                                                 uintptr_t pc);

extern void hexagon_dump(CPUHexagonState *env, FILE *f);
extern int hexagon_gdb_read_register(CPUState *cpu, GByteArray *buf, int reg);
extern int hexagon_gdb_write_register(CPUState *cpu, uint8_t *buf, int reg);

extern void hexagon_debug_vreg(CPUHexagonState *env, int regnum);
extern void hexagon_debug_qreg(CPUHexagonState *env, int regnum);
extern void hexagon_debug(CPUHexagonState *env);
extern void hexagon_store_byte(CPUHexagonState *env, uint8_t store_byte,
    target_ulong dst_vaddr);
extern void hexagon_load_byte(CPUHexagonState *env, uint8_t *load_byte,
    target_ulong src_vaddr);

#if COUNT_HEX_HELPERS
extern void print_helper_counts(void);
#endif

extern const char * const hexagon_regnames[TOTAL_PER_THREAD_REGS];
extern const char * const hexagon_sregnames[];
extern const char * const hexagon_gregnames[];

extern void init_genptr(void);

#define hexagon_cpu_mmu_enabled(env) \
    GET_SYSCFG_FIELD(SYSCFG_MMUEN, ARCH_GET_SYSTEM_REG(env, HEX_SREG_SYSCFG))

#endif
