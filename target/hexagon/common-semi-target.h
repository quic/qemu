/*
 * Target-specific parts of semihosting/arm-compat-semi.c.
 *
 * Copyright (c) 2023, Qualcomm.
 *
 * Copyright (c) 2005, 2007 CodeSourcery.
 * Copyright (c) 2019, 2022 Linaro
 * Copyright © 2020 by Keith Packard <keithp@keithp.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TARGET_HEXAGON_COMMON_SEMI_TARGET_H
#define TARGET_HEXAGON_COMMON_SEMI_TARGET_H

#include "cpu.h"
#include "cpu_helper.h"
#include "qemu/log.h"
#include "semihosting/uaccess.h"

/* Default argument loading macro will fault on a missing page. */
#define SEMIHOSTING_CUSTOM_GET_ARG(n) \
    DEBUG_MEMORY_READ(args + (n) * 4, 4, &arg ## n)

#define SEMIHOSTING_EXT_OPEN_MODES

static inline target_ulong common_semi_arg(CPUState *cs, int argno)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    return ARCH_GET_THREAD_REG(env, HEX_REG_R00 + argno);
}

static inline void common_semi_set_ret(CPUState *cs, target_ulong ret)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    ARCH_SET_THREAD_REG(env, HEX_REG_R00, ret);
}

#define SEMIHOSTING_SET_ERR
static inline void common_semi_set_err(CPUState *cs, target_ulong err)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    ARCH_SET_THREAD_REG(env, HEX_REG_R01, err);
}

static inline bool common_semi_sys_exit_extended(CPUState *cs, int nr)
{
    return (nr == TARGET_SYS_EXIT_EXTENDED || sizeof(target_ulong) == 8);
}

static inline bool is_64bit_semihosting(CPUArchState *env)
{
    return false;
}

static inline target_ulong common_semi_stack_bottom(CPUState *cs)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    return ARCH_GET_THREAD_REG(env, HEX_REG_SP);
}

static inline bool common_semi_has_synccache(CPUArchState *env)
{
    return false;
}

#define SEMIHOSTING_READ_PREPARE_BUF
static inline void common_semi_read_prepare_buf(CPUState *cs, target_ulong buf,
                                                target_ulong len)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    /*
     * Need to make sure the page we are going to write to is available.
     * The file pointer advances with the read.  If the write to bufaddr
     * faults the swi function will be restarted but the file pointer
     * will be wrong.
     */
    hexagon_touch_memory(env, buf, len);
}

#define SEMIHOSTING_CUSTOM_OPEN
static inline void common_semi_sys_open(CPUState *cs, target_ulong fname,
                                        target_ulong fname_len, int gdb_flags,
                                        int mode)
{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    /* First we try standard open routine */
    semihost_sys_open(cs, common_semi_cb, fname, fname_len, gdb_flags, mode);

    /* If we didn't find the file, also try to open in the 'search dir' */
    if ((int32_t)ARCH_GET_THREAD_REG(env, HEX_REG_R00) < 0) {
        int ret, err = ARCH_GET_THREAD_REG(env, HEX_REG_R01);
        char *filename;

        if (use_gdb_syscalls()) {
            qemu_log_mask(LOG_UNIMP, "open semihosting w/ usefs not implemented for gdb");
            return;
        }
        ret = validate_lock_user_string(&filename, cs, fname, fname_len);
        if (ret < 0) {
            common_semi_cb(cs, -1, -ret);
            return;
        }

        if (cpu->usefs && g_strrstr(filename, ".so") != NULL && err == ENOENT) {
            GString *lib_filename = g_string_new(cpu->usefs);
            g_string_append_printf(lib_filename, "/%s", filename);
            semihost_host_open_str(cs, common_semi_cb, lib_filename->str,
                                   gdb_flags, mode);
            g_string_free(lib_filename, true);
        }

        unlock_user(filename, fname, 0);
    }
}

#endif
