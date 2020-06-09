
/*
 *  Copyright (c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
    hexagon_tools_memory_write(env,ADDR,SIZE,(size8u_t)DATA)
#define DEBUG_MEMORY_WRITE_LOCKED(ADDR,SIZE,DATA) \
    hexagon_tools_memory_write_locked(env,ADDR,SIZE,(size8u_t)DATA)

extern void hexagon_tools_memory_read(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr);
extern void hexagon_tools_memory_write(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data);
extern int hexagon_tools_memory_read_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr);
extern int hexagon_tools_memory_write_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data);
extern void hexagon_wait_thread(CPUHexagonState *env);
extern void hexagon_resume_threads(CPUHexagonState *env, uint32_t mask);
extern void hexagon_start_threads(CPUHexagonState *env, uint32_t mask);
extern void hexagon_stop_thread(CPUHexagonState *env);

#endif
