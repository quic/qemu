/*
 * Copyright(c) 2022-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef HEX_INTERRUPTS_H
#define HEX_INTERRUPTS_H

bool hex_check_interrupts(CPUHexagonState *env);
void hex_clear_interrupts(CPUHexagonState *env, uint32_t mask, uint32_t type);
void hex_raise_interrupts(CPUHexagonState *env, uint32_t mask, uint32_t type);
void hex_interrupt_update(CPUHexagonState *env);

#endif
