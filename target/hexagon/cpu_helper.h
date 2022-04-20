
/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_CPU_HELPER_H
#define HEXAGON_CPU_HELPER_H

#define ARCH_GET_THREAD_REG(ENV,REG)     ((ENV)->gpr[(REG)])
#define ARCH_SET_THREAD_REG(ENV,REG,VAL) ((ENV)->gpr[(REG)] = (VAL))
#define ARCH_GET_SYSTEM_REG(ENV,REG)     (((int)(REG) < (int)HEX_SREG_GLB_START) ? \
    (ENV)->t_sreg[(REG)] : (ENV)->g_sreg[(REG)])
#define ARCH_SET_SYSTEM_REG(ENV,REG,VAL) (((int)(REG) < (int)HEX_SREG_GLB_START) ? \
    ((ENV)->t_sreg[(REG)] = (VAL)) : ((ENV)->g_sreg[(REG)] = (VAL)))
#define DEBUG_MEMORY_READ_ENV(ENV,ADDR,SIZE,PTR) \
    hexagon_tools_memory_read(ENV, ADDR, SIZE, PTR)
#define DEBUG_MEMORY_READ(ADDR,SIZE,PTR) \
    hexagon_tools_memory_read(env, ADDR, SIZE, PTR)
#define DEBUG_MEMORY_READ_LOCKED(ADDR,SIZE,PTR) \
    hexagon_tools_memory_read_locked(env, ADDR, SIZE, PTR)
#define DEBUG_MEMORY_WRITE(ADDR,SIZE,DATA) \
    hexagon_tools_memory_write(env, ADDR, SIZE, (uint64_t)DATA)

extern void hexagon_tools_memory_read(CPUHexagonState *env, target_ulong vaddr,
    int size, void *retptr);
extern void hexagon_tools_memory_write(CPUHexagonState *env, target_ulong vaddr,
    int size, uint64_t data);
extern int hexagon_tools_memory_read_locked(CPUHexagonState *env,
                                            target_ulong vaddr, int size,
                                            void *retptr);
extern void hexagon_touch_memory(CPUHexagonState *env, uint32_t start_addr,
                                 uint32_t length);

extern void hexagon_wait_thread(CPUHexagonState *env);
extern void hexagon_resume_thread(CPUHexagonState *env, uint32_t ei);
extern void hexagon_resume_threads(CPUHexagonState *env, uint32_t mask);
extern void hexagon_start_threads(CPUHexagonState *env, uint32_t mask);
extern void hexagon_stop_thread(CPUHexagonState *env);
extern void hexagon_modify_ssr(CPUHexagonState *env, uint32_t new, uint32_t old);
extern void hexagon_ssr_set_cause(CPUHexagonState *env, uint32_t cause);
/*
 * Clear the interrupt pending bits in the mask.
 */
extern void hexagon_clear_interrupts(CPUHexagonState *global_env, uint32_t mask);

/*
 * Assert interrupt number @a irq.  This function will search for the appropriate
 * thread to handle the given interrupt and if it's not disabled, it will dispatch
 * it or pend it if none is available.  Threads selected to handle interrupts that
 * are waiting will be resumed.
 */
extern bool hexagon_assert_interrupt(CPUHexagonState *global_env, uint32_t irq);
extern void hexagon_enable_int(CPUHexagonState *env, uint32_t int_num);



extern const char *get_sys_ssr_str(uint32_t ssr);
extern const char *get_sys_str(CPUHexagonState *env);
extern int sys_in_monitor_mode_reg(uint32_t ssr);
extern int sys_in_guest_mode_reg(uint32_t ssr);
extern int sys_in_user_mode_reg(uint32_t ssr);
extern int sys_in_monitor_mode(CPUHexagonState *env);
extern int sys_in_guest_mode(CPUHexagonState *env);
extern int sys_in_user_mode(CPUHexagonState *env);
extern int get_cpu_mode(CPUHexagonState *env);
extern int get_exe_mode(CPUHexagonState *env);
extern const char *get_exe_mode_str(CPUHexagonState *env);
extern void set_wait_mode(CPUHexagonState *env);
extern void clear_wait_mode(CPUHexagonState *env);
uint64_t hexagon_get_sys_pcycle_count(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_low(CPUHexagonState *env);
uint32_t hexagon_get_sys_pcycle_count_high(CPUHexagonState *env);
void hexagon_set_sys_pcycle_count(CPUHexagonState *env, uint64_t);
void hexagon_set_sys_pcycle_count_low(CPUHexagonState *env, uint32_t);
void hexagon_set_sys_pcycle_count_high(CPUHexagonState *env, uint32_t);
#endif
