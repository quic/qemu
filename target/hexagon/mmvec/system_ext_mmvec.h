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

#ifndef HEXAGON_SYSTEM_EXT_MMVEC_H
#define HEXAGON_SYSTEM_EXT_MMVEC_H

#define thread_t CPUHexagonState
extern void mem_load_vector_oddva(thread_t* thread, Insn * insn, vaddr_t vaddr, vaddr_t lookup_vaddr, int slot, int size, size1u_t* data, int etm_size, int use_full_va);
extern void mem_store_vector_oddva(thread_t* thread, Insn * insn, vaddr_t vaddr, vaddr_t lookup_vaddr, int slot, int size, size1u_t* data, size1u_t* mask, unsigned invert, int use_full_va);
extern void mem_vector_scatter_init(thread_t* thread, Insn * insn, vaddr_t base_vaddr, int length, int element_size);
extern void mem_vector_scatter_finish(thread_t* thread, Insn * insn, int op);
extern void mem_vector_gather_finish(thread_t* thread, Insn * insn);
extern void mem_vector_gather_init(thread_t* thread, Insn * insn, vaddr_t base_vaddr,  int length, int element_size);
extern void mem_gather_store(CPUHexagonState *env, target_ulong vaddr,
                      int slot, uint8_t *data);

#endif
