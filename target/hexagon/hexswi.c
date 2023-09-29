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

#include "qemu/osdep.h"
#include "cpu.h"
#ifdef CONFIG_USER_ONLY
#include "qemu.h"
#include "exec/helper-proto.h"
#else
#include "hw/boards.h"
#include "hw/hexagon/hexagon.h"
#endif
#include "exec/exec-all.h"
#include "exec/cpu_ldst.h"
#include "qemu/log.h"
#include "tcg/tcg-op.h"
#include "internal.h"
#include "macros.h"
#include "sys_macros.h"
#include "arch.h"
#include "fma_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros_auto.h"
#ifndef CONFIG_USER_ONLY
#include "hex_mmu.h"
#endif
#include "sysemu/runstate.h"
#include <dirent.h>
#include "semihosting/common-semi.h"
#include "semihosting/guestfd.h"
#include "gdbstub/syscalls.h"
#include "semihosting/syscalls.h"

#ifndef CONFIG_USER_ONLY

/* non-arm-compatible semihosting calls */
#define HEXAGON_SPECIFIC_SWI_FLAGS \
    DEF_SWI_FLAG(WRITEC,           0x03) \
    DEF_SWI_FLAG(WRITE0,           0x04) \
    DEF_SWI_FLAG(ISTTY,            0x09) \
    DEF_SWI_FLAG(ENTERSVC,         0x17) /* from newlib */ \
    DEF_SWI_FLAG(EXCEPTION,        0x18) /* from newlib */ \
    DEF_SWI_FLAG(READ_CYCLES,      0x40) \
    DEF_SWI_FLAG(PROF_ON,          0x41) \
    DEF_SWI_FLAG(PROF_OFF,         0x42) \
    DEF_SWI_FLAG(WRITECREG,        0x43) \
    DEF_SWI_FLAG(READ_TCYCLES,     0x44) \
    DEF_SWI_FLAG(LOG_EVENT,        0x45) \
    DEF_SWI_FLAG(REDRAW,           0x46) \
    DEF_SWI_FLAG(READ_ICOUNT,      0x47) \
    DEF_SWI_FLAG(PROF_STATSRESET,  0x48) \
    DEF_SWI_FLAG(DUMP_PMU_STATS,   0x4a) \
    DEF_SWI_FLAG(CAPTURE_SIGINT,   0x50) \
    DEF_SWI_FLAG(OBSERVE_SIGINT,   0x51) \
    DEF_SWI_FLAG(READ_PCYCLES,     0x52) \
    DEF_SWI_FLAG(APP_REPORTED,     0x53) \
    DEF_SWI_FLAG(COREDUMP,         0xCD) \
    DEF_SWI_FLAG(SWITCH_THREADS,   0x75) \
    DEF_SWI_FLAG(ISESCKEY_PRESSED, 0x76) \
    DEF_SWI_FLAG(FTELL,            0x100) \
    DEF_SWI_FLAG(FSTAT,            0x101) \
    DEF_SWI_FLAG(FSTATVFS,         0x102) \
    DEF_SWI_FLAG(STAT,             0x103) \
    DEF_SWI_FLAG(GETCWD,           0x104) \
    DEF_SWI_FLAG(ACCESS,           0x105) \
    DEF_SWI_FLAG(FCNTL,            0x106) \
    DEF_SWI_FLAG(GETTIMEOFDAY,     0x107) \
    DEF_SWI_FLAG(OPENDIR,          0x180) \
    DEF_SWI_FLAG(CLOSEDIR,         0x181) \
    DEF_SWI_FLAG(READDIR,          0x182) \
    DEF_SWI_FLAG(MKDIR,            0x183) \
    DEF_SWI_FLAG(RMDIR,            0x184) \
    DEF_SWI_FLAG(EXEC,             0x185) \
    DEF_SWI_FLAG(FTRUNC,           0x186)

#define DEF_SWI_FLAG(name, val) HEX_SYS_ ##name = val,
enum hex_swi_flag {
    HEXAGON_SPECIFIC_SWI_FLAGS
};
#undef DEF_SWI_FLAG

#define DEF_SWI_FLAG(_, val) case val:
static inline bool is_hexagon_specific_swi_flag(enum hex_swi_flag what_swi)
{
    switch (what_swi) {
    HEXAGON_SPECIFIC_SWI_FLAGS
        return true;
    }
    return false;
}
#undef DEF_SWI_FLAG

static const int DIR_INDEX_OFFSET = 0x0b000;

static int MapError(int ERR)
{
    return errno = ERR;
}

static void common_semi_ftell_cb(CPUState *cs, uint64_t ret, int err)
{
    if (err) {
        ret = -1;
    }
    common_semi_cb(cs, ret, err);
}

static int sim_handle_trap_functional(CPUHexagonState *env)
{
    g_assert(qemu_mutex_iothread_locked());

    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    target_ulong what_swi = ARCH_GET_THREAD_REG(env, HEX_REG_R00);
    target_ulong swi_info = ARCH_GET_THREAD_REG(env, HEX_REG_R01);
    int i = 0;
    int retval = 1;

    HEX_DEBUG_LOG("%s:%d: tid %d, what_swi 0x%x, swi_info 0x%x\n",
                  __func__, __LINE__, env->threadId, what_swi, swi_info);

    if (!is_hexagon_specific_swi_flag(what_swi)) {
        CPUState *cs = env_cpu(env);
        do_common_semihosting(cs);
        return retval;
    }

    switch (what_swi) {
    case HEX_SYS_EXCEPTION:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_EXCEPTION\n\t"
            "Program terminated successfully\n",
            __func__, __LINE__);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_MODECTL, 0);

        /* sometimes qurt returns pointer to rval and sometimes the */
        /* actual numeric value.  here we inspect value and make a  */
        /* choice as to probable intent. */
        target_ulong ret;
        ret = ARCH_GET_THREAD_REG(env, HEX_REG_R02);
        HEX_DEBUG_LOG("%s: swi_info 0x%x, ret %d/0x%x\n",
            __func__, swi_info, ret, ret);

        HexagonCPU *cpu = env_archcpu(env);
        if (!cpu->vp_mode) {
            hexagon_dump_json(env);
            exit(ret);
        } else {
            CPUState *cs = CPU(cpu);
            qemu_log_mask(CPU_LOG_RESET | LOG_GUEST_ERROR, "resetting\n");
            cpu_reset(cs);
        }
    }
    break;

    /* We override arm-compatible version to print at stdout, not console. */
    case HEX_SYS_WRITEC:
    {
        FILE *fp = stdout;
        char c;
        rcu_read_lock();
        DEBUG_MEMORY_READ(swi_info, 1, &c);
        fprintf(fp, "%c", c);
        fflush(fp);
        rcu_read_unlock();
    }
    break;

    case HEX_SYS_WRITECREG:
    {
        char c = swi_info;
        FILE *fp = stdout;

        fprintf(fp, "%c", c);
        fflush(stdout);
    }
    break;

    /* We override arm-compatible version to print at stdout, not console. */
    case HEX_SYS_WRITE0:
    {
        FILE *fp = stdout;
        char c;
        i = 0;
        rcu_read_lock();
        do {
            DEBUG_MEMORY_READ(swi_info + i, 1, &c);
            fprintf(fp, "%c", c);
            i++;
        } while (c);
        fflush(fp);
        rcu_read_unlock();
        break;
    }

    /*
     * Hexagon's SYS_ISTTY is a bit different than arm's: we do not return -1
     * on error, neither errno. So we override with out own implementation.
     */
    case HEX_SYS_ISTTY:
    {
        int fd;
        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        ARCH_SET_THREAD_REG(env, HEX_REG_R00,
                            isatty(fd));
    }
    break;

    case HEX_SYS_STAT:
    case HEX_SYS_FSTAT:
    {
        /*
         * This must match the caller's definition, it would be in the
         * caller's angel.h or equivalent header.
         */
        struct __SYS_STAT {
            uint64_t dev;
            uint64_t ino;
            uint32_t mode;
            uint32_t nlink;
            uint64_t rdev;
            uint32_t size;
            uint32_t __pad1;
            uint32_t atime;
            uint32_t mtime;
            uint32_t ctime;
            uint32_t __pad2;
        } sys_stat;
        struct stat st_buf;
        uint8_t *st_bufptr = (uint8_t *)&sys_stat;
        int rc, err;
        char filename[BUFSIZ];
        target_ulong physicalFilenameAddr;
        target_ulong statBufferAddr;
        DEBUG_MEMORY_READ(swi_info, 4, &physicalFilenameAddr);

        if (what_swi == HEX_SYS_STAT) {
            i = 0;
            do {
                DEBUG_MEMORY_READ(physicalFilenameAddr + i, 1, &filename[i]);
                i++;
            } while ((i < BUFSIZ) && filename[i - 1]);
            rc = stat(filename, &st_buf);
            err = errno;
        } else{
            int fd = physicalFilenameAddr;
            GuestFD *gf = get_guestfd(fd);
            if (gf->type != GuestFDHost) {
                fprintf(stderr, "fstat semihosting only implemented for native mode.\n");
                g_assert_not_reached();
            }
            rc = fstat(gf->hostfd, &st_buf);
            err = errno;
        }
        if (rc == 0) {
            sys_stat.dev   = st_buf.st_dev;
            sys_stat.ino   = st_buf.st_ino;
            sys_stat.mode  = st_buf.st_mode;
            sys_stat.nlink = (uint32_t) st_buf.st_nlink;
            sys_stat.rdev  = st_buf.st_rdev;
            sys_stat.size  = (uint32_t) st_buf.st_size;
#if defined(__linux__)
            sys_stat.atime = (uint32_t) st_buf.st_atim.tv_sec;
            sys_stat.mtime = (uint32_t) st_buf.st_mtim.tv_sec;
            sys_stat.ctime = (uint32_t) st_buf.st_ctim.tv_sec;
#elif defined(_WIN32)
            sys_stat.atime = st_buf.st_atime;
            sys_stat.mtime = st_buf.st_mtime;
            sys_stat.ctime = st_buf.st_ctime;
#endif
        } else {
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, err);
        }
        DEBUG_MEMORY_READ(swi_info + 4, 4, &statBufferAddr);

        for (i = 0; i < sizeof(sys_stat); i++) {
            DEBUG_MEMORY_WRITE(statBufferAddr + i, 1, st_bufptr[i]);
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, rc);
    }
    break;

    case HEX_SYS_FTRUNC:
    {
        int fd;
        off_t size_limit;
        CPUState *cs = env_cpu(env);
        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        DEBUG_MEMORY_READ(swi_info + 4, 8, &size_limit);
        semihost_sys_ftruncate(cs, common_semi_cb, fd, size_limit);
    }
    break;

    case HEX_SYS_ACCESS:
    {
        char filename[BUFSIZ];
        size4u_t FileNameAddr;
        size4u_t BufferMode;
        int rc;

        i = 0;

        DEBUG_MEMORY_READ(swi_info, 4, &FileNameAddr);
        do {
            DEBUG_MEMORY_READ(FileNameAddr + i, 1, &filename[i]);
            i++;
        } while ((i < BUFSIZ) && (filename[i - 1]));
        filename[i] = 0;

        DEBUG_MEMORY_READ(swi_info + 4, 4, &BufferMode);

        rc = access(filename, BufferMode);
        if (rc != 0) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        }

        ARCH_SET_THREAD_REG(env, HEX_REG_R00, rc);
    }
    break;
    case HEX_SYS_GETCWD:
    {
        char *cwdPtr;
        size4u_t BufferAddr;
        size4u_t BufferSize;

        DEBUG_MEMORY_READ(swi_info, 4, &BufferAddr);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &BufferSize);

        cwdPtr = malloc(BufferSize);
        if (!cwdPtr || !getcwd(cwdPtr, BufferSize)) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        } else {
            for (int i = 0; i < BufferSize; i++) {
                DEBUG_MEMORY_WRITE(BufferAddr + i, 1, (size8u_t)cwdPtr[i]);
            }
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, BufferAddr);
        }
        free(cwdPtr);
        break;
    }

    case HEX_SYS_EXEC:
    {
        qemu_log_mask(LOG_UNIMP, "SYS_EXEC is deprecated\n");
    }
    break;

    case HEX_SYS_OPENDIR:
    {
        DIR *dir;
        char buf[BUFSIZ];
        int dir_index = 0;

        i = 0;
        do {
            DEBUG_MEMORY_READ(swi_info + i, 1, &buf[i]);
            i++;
        } while (buf[i - 1]);

        dir = opendir(buf);
        if (dir != NULL) {
            env->dir_list = g_list_append(env->dir_list, dir);
            dir_index = g_list_index(env->dir_list, dir) + DIR_INDEX_OFFSET;
        } else
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));

        ARCH_SET_THREAD_REG(env, HEX_REG_R00, dir_index);
        break;
    }

    case HEX_SYS_READDIR:
    {
        DIR *dir;
        struct dirent *host_dir_entry = NULL;
        vaddr_t guest_dir_entry;
        int dir_index = swi_info - DIR_INDEX_OFFSET;
        int i;

        dir = g_list_nth_data(env->dir_list, dir_index);

        if (dir) {
            errno = 0;
            host_dir_entry = readdir(dir);
            if (host_dir_entry == NULL && errno != 0) {
                ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            }
        } else
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, EBADF);

        guest_dir_entry = ARCH_GET_THREAD_REG(env, HEX_REG_R02) +
            sizeof(int32_t);
        if (host_dir_entry) {
            vaddr_t guest_dir_ptr = guest_dir_entry;

            for (i = 0; i < sizeof(host_dir_entry->d_name); i++) {
                DEBUG_MEMORY_WRITE(guest_dir_ptr + i, 1,
                    host_dir_entry->d_name[i]);
                if (!host_dir_entry->d_name[i]) {
                    break;
                }
            }
            ARCH_SET_THREAD_REG(env, HEX_REG_R00,
                guest_dir_entry - sizeof(int32_t));
        } else
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        break;
    }

    case HEX_SYS_CLOSEDIR:
    {
        DIR *dir;
        int ret = 0;

        dir = g_list_nth_data(env->dir_list, swi_info);
        if (dir != NULL) {
            ret = closedir(dir);
            if (ret != 0) {
                ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            }
        } else
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, EBADF);
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, ret);
        break;
    }

    case HEX_SYS_COREDUMP:
      printf("CRASH!\n");
      printf("I think the exception was: ");
      switch (GET_SSR_FIELD(SSR_CAUSE, ssr)) {
      case 0x43:
          printf("0x43, NMI");
          break;
      case 0x42:
          printf("0x42, Data abort");
          break;
      case 0x44:
          printf("0x44, Multi TLB match");
          break;
      case HEX_CAUSE_BIU_PRECISE:
          printf("0x%x, Bus Error (Precise BIU error)",
                 HEX_CAUSE_BIU_PRECISE);
          break;
      case HEX_CAUSE_DOUBLE_EXCEPT:
          printf("0x%x, Exception observed when EX = 1 (double exception)",
                 HEX_CAUSE_DOUBLE_EXCEPT);
          break;
      case HEX_CAUSE_FETCH_NO_XPAGE:
          printf("0x%x, Privilege violation: User/Guest mode execute"
                 " to page with no execute permissions",
                 HEX_CAUSE_FETCH_NO_XPAGE);
          break;
      case HEX_CAUSE_FETCH_NO_UPAGE:
          printf("0x%x, Privilege violation: "
                 "User mode exececute to page with no user permissions",
                 HEX_CAUSE_FETCH_NO_UPAGE);
          break;
      case HEX_CAUSE_INVALID_PACKET:
          printf("0x%x, Invalid packet",
                 HEX_CAUSE_INVALID_PACKET);
          break;
      case HEX_CAUSE_PRIV_USER_NO_GINSN:
          printf("0x%x, Privilege violation: guest mode insn in user mode",
                 HEX_CAUSE_PRIV_USER_NO_GINSN);
          break;
      case HEX_CAUSE_PRIV_USER_NO_SINSN:
          printf("0x%x, Privilege violation: "
                 "monitor mode insn ins user/guest mode",
                 HEX_CAUSE_PRIV_USER_NO_SINSN);
          break;
      case HEX_CAUSE_REG_WRITE_CONFLICT:
          printf("0x%x, Multiple writes to same register",
                 HEX_CAUSE_REG_WRITE_CONFLICT);
          break;
      case HEX_CAUSE_PC_NOT_ALIGNED:
          printf("0x%x, PC not aligned",
                 HEX_CAUSE_PC_NOT_ALIGNED);
          break;
      case HEX_CAUSE_MISALIGNED_LOAD:
          printf("0x%x, Misaligned Load @ 0x%x",
                 HEX_CAUSE_MISALIGNED_LOAD,
                 ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case HEX_CAUSE_MISALIGNED_STORE:
          printf("0x%x, Misaligned Store @ 0x%x",
                 HEX_CAUSE_MISALIGNED_STORE,
                 ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case HEX_CAUSE_PRIV_NO_READ:
          printf("0x%x, Privilege violation: "
              "user/guest read permission @ 0x%x",
              HEX_CAUSE_PRIV_NO_READ,
              ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case HEX_CAUSE_PRIV_NO_WRITE:
          printf("0x%x, Privilege violation: "
              "user/guest write permission @ 0x%x",
              HEX_CAUSE_PRIV_NO_WRITE,
              ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case HEX_CAUSE_PRIV_NO_UREAD:
          printf("0x%x, Privilege violation: user read permission @ 0x%x",
                 HEX_CAUSE_PRIV_NO_UREAD,
                 ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case HEX_CAUSE_PRIV_NO_UWRITE:
          printf("0x%x, Privilege violation: user write permission @ 0x%x",
                 HEX_CAUSE_PRIV_NO_UWRITE,
                 ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case HEX_CAUSE_COPROC_LDST:
          printf("0x%x, Coprocessor VMEM address error. @ 0x%x",
                 HEX_CAUSE_COPROC_LDST,
                 ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case HEX_CAUSE_STACK_LIMIT:
          printf("0x%x, Stack limit check error", HEX_CAUSE_STACK_LIMIT);
          break;
      case HEX_CAUSE_FPTRAP_CAUSE_BADFLOAT:
          printf("0x%X, Floating-Point: Execution of Floating-Point "
                 "instruction resulted in exception",
                 HEX_CAUSE_FPTRAP_CAUSE_BADFLOAT);
          break;
      case HEX_CAUSE_NO_COPROC_ENABLE:
          printf("0x%x, Illegal Execution of Coprocessor Instruction",
                 HEX_CAUSE_NO_COPROC_ENABLE);
          break;
      case HEX_CAUSE_NO_COPROC2_ENABLE:
          printf("0x%x, "
                 "Illegal Execution of Secondary Coprocessor Instruction",
                 HEX_CAUSE_NO_COPROC2_ENABLE);
          break;
      case HEX_CAUSE_UNSUPORTED_HVX_64B:
          printf("0x%x, "
                 "Unsuported Execution of Coprocessor Instruction with 64bits Mode On",
                 HEX_CAUSE_UNSUPORTED_HVX_64B);
          break;
      default:
          printf("Don't know");
          break;
      }
      printf("\nRegister Dump:\n");
      hexagon_dump(env, stdout, 0);
      break;

    case HEX_SYS_FTELL:
    {
        int fd;
        CPUState *cs = env_cpu(env);
        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        semihost_sys_lseek(cs, common_semi_ftell_cb, fd, 0, GDB_SEEK_CUR);
    }
    break;

    case HEX_SYS_READ_CYCLES:
    case HEX_SYS_READ_TCYCLES:
    {
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, 0);
        break;
    }

    case HEX_SYS_READ_ICOUNT:
    {
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, 0);
        break;
    }

    case HEX_SYS_READ_PCYCLES:
    {
        ARCH_SET_THREAD_REG(env, HEX_REG_R00,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLELO));
        ARCH_SET_THREAD_REG(env, HEX_REG_R01,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLEHI));
        break;
    }

    case HEX_SYS_APP_REPORTED:
        break;

    case HEX_SYS_PROF_ON:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(ENOSYS));
        qemu_log_mask(LOG_UNIMP, "SYS_PROF_ON is bogus on QEMU!\n");
        break;
    case HEX_SYS_PROF_OFF:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(ENOSYS));
        qemu_log_mask(LOG_UNIMP, "SYS_PROF_OFF bogus on QEMU!\n");
        break;
    case HEX_SYS_PROF_STATSRESET:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(ENOSYS));
        qemu_log_mask(LOG_UNIMP, "SYS_PROF_STATSRESET bogus on QEMU!\n");
        break;
    case HEX_SYS_DUMP_PMU_STATS:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(ENOSYS));
        qemu_log_mask(LOG_UNIMP, "PMU stats are bogus on QEMU!\n");
        break;

    default:
        printf("error: unknown swi call 0x%x\n", what_swi);
        CPUState *cs = env_cpu(env);
        cpu_abort(cs, "Hexagon Unsupported swi call 0x%x\n", what_swi);
        retval = 0;
        break;
    }

    return retval;
}


static int sim_handle_trap(CPUHexagonState *env)

{
    g_assert(qemu_mutex_iothread_locked());

    int retval = 0;
    target_ulong what_swi = ARCH_GET_THREAD_REG(env, HEX_REG_R00);

    retval = sim_handle_trap_functional(env);

    if (!retval) {
        qemu_log_mask(LOG_GUEST_ERROR, "unknown swi request: 0x%x\n", what_swi);
    }

    return retval;
}

static void set_addresses(CPUHexagonState *env,
    target_ulong pc_offset, target_ulong exception_index)

{
    ARCH_SET_SYSTEM_REG(env, HEX_SREG_ELR,
        ARCH_GET_THREAD_REG(env, HEX_REG_PC) + pc_offset);
    ARCH_SET_THREAD_REG(env, HEX_REG_PC,
        ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB) | (exception_index << 2));
}

static const char *event_name[] = {
         [HEX_EVENT_RESET] = "HEX_EVENT_RESET",
         [HEX_EVENT_IMPRECISE] = "HEX_EVENT_IMPRECISE",
         [HEX_EVENT_TLB_MISS_X] = "HEX_EVENT_TLB_MISS_X",
         [HEX_EVENT_TLB_MISS_RW] = "HEX_EVENT_TLB_MISS_RW",
         [HEX_EVENT_TRAP0] = "HEX_EVENT_TRAP0",
         [HEX_EVENT_TRAP1] = "HEX_EVENT_TRAP1",
         [HEX_EVENT_FPTRAP] = "HEX_EVENT_FPTRAP",
         [HEX_EVENT_DEBUG] = "HEX_EVENT_DEBUG",
         [HEX_EVENT_INT0] = "HEX_EVENT_INT0",
         [HEX_EVENT_INT1] = "HEX_EVENT_INT1",
         [HEX_EVENT_INT2] = "HEX_EVENT_INT2",
         [HEX_EVENT_INT3] = "HEX_EVENT_INT3",
         [HEX_EVENT_INT4] = "HEX_EVENT_INT4",
         [HEX_EVENT_INT5] = "HEX_EVENT_INT5",
         [HEX_EVENT_INT6] = "HEX_EVENT_INT6",
         [HEX_EVENT_INT7] = "HEX_EVENT_INT7",
         [HEX_EVENT_INT8] = "HEX_EVENT_INT8",
         [HEX_EVENT_INT9] = "HEX_EVENT_INT9",
         [HEX_EVENT_INTA] = "HEX_EVENT_INTA",
         [HEX_EVENT_INTB] = "HEX_EVENT_INTB",
         [HEX_EVENT_INTC] = "HEX_EVENT_INTC",
         [HEX_EVENT_INTD] = "HEX_EVENT_INTD",
         [HEX_EVENT_INTE] = "HEX_EVENT_INTE",
         [HEX_EVENT_INTF] = "HEX_EVENT_INTF"
};

void hexagon_cpu_do_interrupt(CPUState *cs)

{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;
    const bool exception_context = qemu_mutex_iothread_locked();
    LOCK_IOTHREAD(exception_context);

    HEX_DEBUG_LOG("%s: tid %d, event 0x%x, cause 0x%x\n",
      __func__, env->threadId, cs->exception_index, env->cause_code);
    qemu_log_mask(CPU_LOG_INT,
            "\t%s: event 0x%x:%s, cause 0x%x(%d)\n", __func__,
            cs->exception_index, event_name[cs->exception_index], env->cause_code, env->cause_code);

    env->llsc_addr = ~0;

    CPU_MEMOP_PC_SET_ON_EXCEPTION(env);

    switch (cs->exception_index) {
    case HEX_EVENT_TRAP0:
        HEX_DEBUG_LOG("\ttid = %u, trap0, pc = 0x%x, elr = 0x%x, "
            "index 0x%x, cause 0x%x\n",
            env->threadId,
            ARCH_GET_THREAD_REG(env, HEX_REG_PC),
            ARCH_GET_THREAD_REG(env, HEX_REG_PC) + 4,
            cs->exception_index,
            env->cause_code);

        if (env->cause_code == 0) {
            sim_handle_trap(env);
        }

        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        break;

    case HEX_EVENT_TRAP1:
        HEX_DEBUG_LOG("\ttid = %u, trap1, pc = 0x%x, elr = 0x%x, "
            "index 0x%x, cause 0x%x\n",
            env->threadId,
            ARCH_GET_THREAD_REG(env, HEX_REG_PC),
            ARCH_GET_THREAD_REG(env, HEX_REG_PC) + 4,
            cs->exception_index,
            env->cause_code);
        HEX_DEBUG_LOG("\tEVB 0x%x, shifted index %d/0x%x, final 0x%x\n",
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB),
            cs->exception_index << 2,
            cs->exception_index << 2,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB) |
                (cs->exception_index << 2));

        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        break;

    case HEX_EVENT_TLB_MISS_X:
        switch (env->cause_code) {
        case HEX_CAUSE_TLBMISSX_CAUSE_NORMAL:
        case HEX_CAUSE_TLBMISSX_CAUSE_NEXTPAGE:
            qemu_log_mask(CPU_LOG_MMU,
                "TLB miss EX exception (0x%x) caught: "
                "Cause code (0x%x) "
                "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                ", BADVA = 0x%" PRIx32 "\n",
                cs->exception_index, env->cause_code,
                env->threadId,
                ARCH_GET_THREAD_REG(env, HEX_REG_PC),
                ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
            hex_tlb_lock(env);

            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        default:
            cpu_abort(cs, "1:Hexagon exception %d/0x%x: "
                "Unknown cause code %d/0x%x\n",
                cs->exception_index, cs->exception_index,
                env->cause_code, env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_TLB_MISS_RW:
        switch (env->cause_code) {
        case HEX_CAUSE_TLBMISSRW_CAUSE_READ:
        case HEX_CAUSE_TLBMISSRW_CAUSE_WRITE:
            qemu_log_mask(CPU_LOG_MMU,
                "TLB miss RW exception (0x%x) caught: "
                "Cause code (0x%x) "
                "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                ", BADVA = 0x%" PRIx32 "\n",
                cs->exception_index, env->cause_code,
                env->threadId, env->gpr[HEX_REG_PC],
                ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
            hex_tlb_lock(env);

            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
            break;

        default:
            cpu_abort(cs, "2:Hexagon exception %d/0x%x: "
                "Unknown cause code %d/0x%x\n",
                cs->exception_index, cs->exception_index,
                env->cause_code, env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_FPTRAP:
        hexagon_ssr_set_cause(env, env->cause_code);
        /*
         * FIXME - QTOOL-89796 Properly handle FP exception traps
         *     ARCH_SET_SYSTEM_REG(env, HEX_SREG_ELR, env->next_PC);
         */
        ARCH_SET_THREAD_REG(env, HEX_REG_PC,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB) |
            (cs->exception_index << 2));
        break;

    case HEX_EVENT_DEBUG:
        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 0, cs->exception_index);
        ATOMIC_STORE(env->ss_pending, false);
        break;

    case HEX_EVENT_PRECISE:
        switch (env->cause_code) {
        case HEX_CAUSE_FETCH_NO_XPAGE:
        case HEX_CAUSE_FETCH_NO_UPAGE:
        case HEX_CAUSE_PRIV_NO_READ:
        case HEX_CAUSE_PRIV_NO_UREAD:
        case HEX_CAUSE_PRIV_NO_WRITE:
        case HEX_CAUSE_PRIV_NO_UWRITE:
        case HEX_CAUSE_MISALIGNED_LOAD:
        case HEX_CAUSE_MISALIGNED_STORE:
        case HEX_CAUSE_PC_NOT_ALIGNED:
            qemu_log_mask(CPU_LOG_MMU,
                "MMU permission exception (0x%x) caught: "
                "Cause code (0x%x) "
                "TID = 0x%" PRIx32 ", PC = 0x%" PRIx32
                ", BADVA = 0x%" PRIx32 "\n",
                cs->exception_index, env->cause_code,
                env->threadId, env->gpr[HEX_REG_PC],
                ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));


            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
            break;

        case HEX_CAUSE_PRIV_USER_NO_SINSN:
        case HEX_CAUSE_PRIV_USER_NO_GINSN:
        case HEX_CAUSE_INVALID_OPCODE:
        case HEX_CAUSE_NO_COPROC_ENABLE:
        case HEX_CAUSE_NO_COPROC2_ENABLE:
        case HEX_CAUSE_UNSUPORTED_HVX_64B:
        case HEX_CAUSE_REG_WRITE_CONFLICT:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        case HEX_CAUSE_COPROC_LDST:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        case HEX_CAUSE_STACK_LIMIT:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 0, cs->exception_index);
            break;

        default:
            cpu_abort(cs, "3:Hexagon exception %d/0x%x: "
                "Unknown cause code %d/0x%x\n",
                cs->exception_index, cs->exception_index,
                env->cause_code, env->cause_code);
            break;
        }
        break;

    case HEX_EVENT_IMPRECISE:
        switch (env->cause_code) {
        case HEX_CAUSE_IMPRECISE_MULTI_TLB_MATCH:
            /*
             * FIXME
             * Imprecise exceptions are delivered to all HW threads in
             * run or wait mode
             */
            /* After the exception handler, return to the next packet */


            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 4, cs->exception_index);
            ARCH_SET_SYSTEM_REG(env, HEX_SREG_DIAG,
                (0x4 << 4) | (ARCH_GET_SYSTEM_REG(env, HEX_SREG_HTID) & 0xF));
            break;

        case HEX_CAUSE_IMPRECISE_NMI:
            hexagon_ssr_set_cause(env, env->cause_code);
            set_addresses(env, 4, cs->exception_index);
            ARCH_SET_SYSTEM_REG(env, HEX_SREG_DIAG,
                                (0x3 << 4) |
                                    (ARCH_GET_SYSTEM_REG(env, HEX_SREG_DIAG)));
            /* FIXME use thread mask */
            break;

        default:
            cpu_abort(cs, "4:Hexagon exception %d/0x%x: "
                "Unknown cause code %d/0x%x\n",
                cs->exception_index, cs->exception_index,
                env->cause_code, env->cause_code);
            break;
        }
        break;

    default:
        printf("%s:%d: throw error\n", __func__, __LINE__);
        cpu_abort(cs, "Hexagon Unsupported exception 0x%x/0x%x\n",
                  cs->exception_index, env->cause_code);
        break;
    }

    cs->exception_index = HEX_EVENT_NONE;
    UNLOCK_IOTHREAD(exception_context);
}

void register_trap_exception(CPUHexagonState *env, int traptype, int imm,
                             target_ulong PC)
{
    HEX_DEBUG_LOG("%s:\n\ttid = %d, pc = 0x%" PRIx32
                  ", traptype %d, "
                  "imm %d\n",
                  __func__, env->threadId,
                  ARCH_GET_THREAD_REG(env, HEX_REG_PC),
                  traptype, imm);

    CPUState *cs = env_cpu(env);
    /* assert(cs->exception_index == HEX_EVENT_NONE); */

    cs->exception_index = (traptype == 0) ? HEX_EVENT_TRAP0 : HEX_EVENT_TRAP1;
    ASSERT_DIRECT_TO_GUEST_UNSET(env, cs->exception_index);

    env->cause_code = imm;
    env->gpr[HEX_REG_PC] = PC;
    cpu_loop_exit(cs);
}
#endif

