
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

extern void hexagon_tools_memory_read(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr);
extern void hexagon_tools_memory_write(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data);
extern int hexagon_tools_memory_read_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr);
extern int hexagon_tools_memory_write_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data);

#endif
