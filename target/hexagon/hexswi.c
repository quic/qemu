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

#include <math.h>
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
#include "arch.h"
#include "fma_emu.h"
#include "conv_emu.h"
#include "mmvec/mmvec.h"
#include "mmvec/macros_auto.h"
#ifndef CONFIG_USER_ONLY
#include "hex_mmu.h"
#endif
#include "sysemu/runstate.h"

#ifndef CONFIG_USER_ONLY

#define SYS_OPEN            0x01
#define SYS_CLOSE           0x02
#define SYS_WRITEC          0x03
#define SYS_WRITE0          0x04
#define SYS_WRITE           0x05
#define SYS_READ            0x06
#define SYS_READC           0x07
#define SYS_ISERROR         0x08
#define SYS_ISTTY           0x09
#define SYS_SEEK            0x0a
#define SYS_FLEN            0x0c
#define SYS_TMPNAM          0x0d
#define SYS_REMOVE          0x0e
#define SYS_RENAME          0x0f
#define SYS_CLOCK           0x10
#define SYS_TIME            0x11
#define SYS_SYSTEM          0x12
#define SYS_ERRNO           0x13
#define SYS_GET_CMDLINE     0x15
#define SYS_HEAPINFO        0x16
#define SYS_ENTERSVC        0x17  /* from newlib */
#define SYS_EXCEPTION       0x18  /* from newlib */
#define SYS_ELAPSED         0x30
#define SYS_TICKFREQ        0x31
#define SYS_READ_CYCLES     0x40
#define SYS_PROF_ON         0x41
#define SYS_PROF_OFF        0x42
#define SYS_WRITECREG       0x43
#define SYS_READ_TCYCLES    0x44
#define SYS_LOG_EVENT       0x45
#define SYS_REDRAW          0x46
#define SYS_READ_ICOUNT     0x47
#define SYS_PROF_STATSRESET 0x48
#define SYS_DUMP_PMU_STATS  0x4a
#define SYS_CAPTURE_SIGINT  0x50
#define SYS_OBSERVE_SIGINT  0x51
#define SYS_READ_PCYCLES    0x52
#define SYS_APP_REPORTED    0x53
#define SYS_COREDUMP        0xCD
#define SYS_SWITCH_THREADS  0x75
#define SYS_ISESCKEY_PRESSED 0x76
#define SYS_FTELL           0x100
#define SYS_FSTAT           0x101
#define SYS_FSTATVFS        0x102
#define SYS_STAT            0x103
#define SYS_GETCWD          0x104
#define SYS_ACCESS          0x105
#define SYS_FCNTL           0x106
#define SYS_GETTIMEOFDAY    0x107
#define SYS_OPENDIR         0x180
#define SYS_CLOSEDIR        0x181
#define SYS_READDIR         0x182
#define SYS_MKDIR           0x183
#define SYS_RMDIR           0x184
#define SYS_FTRUNC          0x186

static const int DIR_INDEX_OFFSET = 0x0b000;

static int MapError(int ERR)
{
    errno = ERR;
    return (ERR);
}


static int sim_handle_trap_functional(CPUHexagonState *env)

{
    target_ulong ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    target_ulong what_swi = ARCH_GET_THREAD_REG(env, HEX_REG_R00);
    target_ulong swi_info = ARCH_GET_THREAD_REG(env, HEX_REG_R01);
    int i = 0, c;
    int retval = 1;

    HEX_DEBUG_LOG("%s:%d: tid %d, what_swi 0x%x, swi_info 0x%x\n",
                  __FUNCTION__, __LINE__, env->threadId, what_swi, swi_info);
    switch (what_swi) {
    case SYS_HEAPINFO:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_HEAPINFO\n", __FUNCTION__, __LINE__);
#if 0
        size4u_t bufptr;

        DEBUG_MEMORY_READ(swi_info, 4, &bufptr);
         DEBUG_MEMORY_WRITE(bufptr+0, 4, 0);
        DEBUG_MEMORY_WRITE(bufptr+1, 4, 0);
        DEBUG_MEMORY_WRITE(bufptr+2, 4, 0);
         DEBUG_MEMORY_WRITE(bufptr+3, 4, 0);
#endif
    }
    break;

    case SYS_GET_CMDLINE:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_GET_CMDLINE\n", __FUNCTION__, __LINE__);
        target_ulong bufptr;
        target_ulong bufsize;

        DEBUG_MEMORY_READ(swi_info, 4, &bufptr);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &bufsize);

        const target_ulong to_copy =
            (env->cmdline != NULL) ?
                ((bufsize <= (unsigned int)strlen(env->cmdline)) ?
                     (bufsize - 1) :
                     strlen(env->cmdline)) :
                0;

        HEX_DEBUG_LOG("\tcmdline '%s' len to copy %d buf max %d\n",
            env->cmdline, to_copy, bufsize);
        for (i = 0; i < (int) to_copy; i++) {
            DEBUG_MEMORY_WRITE(bufptr + i, 1, (size8u_t) env->cmdline[i]);
        }
      DEBUG_MEMORY_WRITE(bufptr + i, 1, (size8u_t) 0);
      ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
    }
    break;

    case SYS_EXCEPTION:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_EXCEPTION\n\t"
            "Program terminated successfully\n",
            __FUNCTION__, __LINE__);
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_MODECTL, 0);

        /* sometimes qurt returns pointer to rval and sometimes the */
        /* actual numeric value.  here we inspect value and make a  */
        /* choice as to probable intent. */
        target_ulong ret;
        ret = ARCH_GET_THREAD_REG(env, HEX_REG_R02);
        HEX_DEBUG_LOG("%s: swi_info 0x%x, ret %d/0x%x\n",
            __FUNCTION__, swi_info, ret, ret);

        HexagonCPU *cpu = hexagon_env_get_cpu(env);
        if (!cpu->vp_mode) {
            exit(ret);
        }
        else {
            CPUState *cs = CPU(cpu);
            qemu_log_mask(CPU_LOG_RESET | LOG_GUEST_ERROR, "resetting\n");
            cpu_reset(cs);
        }
    }
    break;

    case SYS_WRITEC:
    {
        FILE *fp = stdout;
        rcu_read_lock();
        DEBUG_MEMORY_READ(swi_info, 1, &c);
        fprintf(fp, "%c", c);
        fflush(fp);
        rcu_read_unlock();
    }
    break;

    case SYS_WRITECREG:
    {
        char c = swi_info;
        FILE *fp = stdout;

        fprintf(fp, "%c", c);
        fflush(stdout);
    }
    break;

    case SYS_WRITE0:
    {
        FILE *fp = stdout;
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


  case SYS_WRITE:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_WRITE\n", __FUNCTION__, __LINE__);
        char *buf;
        int fd;
        target_ulong bufaddr;
        int count;
        int retval;

        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &bufaddr);
        DEBUG_MEMORY_READ(swi_info + 8, 4, &count);

        int malloc_count = (count) ? count : 1;
        if ((buf = (char *) g_malloc(malloc_count)) == NULL) {
            printf("%s:%d: "
                "ERROR: Couldn't allocate temporary buffer (%d bytes)",
                __FUNCTION__, __LINE__, count);
        }

        rcu_read_lock();
        for (i = 0; i < count; i++) {
            DEBUG_MEMORY_READ(bufaddr + i, 1, &buf[i]);
        }
        retval = 0;
        if (count) {
          retval = write(fd, buf, count);
        }
        if (retval == count) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        } else if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, retval);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        } else {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, count - retval);
        }
        rcu_read_unlock();
        free(buf);
    }
    break;

    case SYS_READ:
    {
        int fd;
        char *buf;
        size4u_t bufaddr;
        int count;
        int retval;

        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &bufaddr);
        DEBUG_MEMORY_READ(swi_info + 8, 4, &count);
/*
 * Need to make sure the page we are going to write to is available.
 * The file pointer advances with the read.  If the write to bufaddr
 * faults this function will be restarted but the file pointer
 * will be wrong.
 */
        hexagon_touch_memory(env, bufaddr, count);

        int malloc_count = (count) ? count : 1;
        if ((buf = (char *) g_malloc(malloc_count)) == NULL) {
            CPUState *cs = env_cpu(env);
            cpu_abort(cs,
                "Error: %s couldn't allocate "
                "temporybuffer (%d bytes)",
                __FUNCTION__, count);
        }

        retval = 0;
        if (count) {
            retval = read(fd, buf, count);
            for (i = 0; i < retval; i++) {
                DEBUG_MEMORY_WRITE(bufaddr + i, 1, buf[i]);
            }
        }
        if (retval == count) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        } else if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        } else {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, count - retval);
        }
        free(buf);
    }
    break;

#if 0
    case SYS_FCNTL:
    {
        struct __SYS_FCNTL sysfcntl;
        size4u_t fcntlFileDesc;
        size4u_t fcntlCmd;
        size4u_t fcntlPBA;  /* Physical Buffer Address */
        int rc;
        struct flock fl;

        DEBUG_MEMORY_READ(swi_info, 4, &fcntlFileDesc);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &fcntlCmd);
        DEBUG_MEMORY_READ(swi_info + 8, 4, &fcntlPBA);
        DEBUG_MEMORY_READ(fcntlPBA, 4, &sysfcntl.l_type);
        DEBUG_MEMORY_READ(fcntlPBA + 0x4, 4, &sysfcntl.l_whence);
        DEBUG_MEMORY_READ(fcntlPBA + 0x8, 4, &sysfcntl.l_start);
        DEBUG_MEMORY_READ(fcntlPBA + 0xc, 4, &sysfcntl.l_len);

        if (sysfcntl.l_type == DK_F_GETLK)
            fl.l_type = F_GETLK;
        else if (sysfcntl.l_type == DK_F_SETLK)
            fl.l_type = F_SETLK;

        fl.l_whence = sysfcntl.l_whence;
        fl.l_start = sysfcntl.l_start;
        fl.l_len = sysfcntl.l_len;
        fl.l_pid = getpid();

        rc = fcntl(fcntlFileDesc, fcntlCmd, &fl);

/*
 * The F_GETLK updates the input fl structure so that must be copied
 * back out to the simulator.
 */
        if ((rc != -1) && (fcntlCmd == DK_F_GETLK)) {
            DEBUG_MEMORY_WRITE(fcntlPBA, 4, sysfcntl.l_type);
            DEBUG_MEMORY_WRITE(fcntlPBA + 0x4, 4, sysfcntl.l_whence);
            DEBUG_MEMORY_WRITE(fcntlPBA + 0x8, 4, sysfcntl.l_start);
          DEBUG_MEMORY_WRITE(fcntlPBA + 0xc, 4, sysfcntl.l_len);
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, rc);
        break;
    }
#endif

    case SYS_OPEN:
    {
        char filename[BUFSIZ];
        target_ulong physicalFilenameAddr;

        unsigned int filemode;
        int length;
        int real_openmode;
        int fd;
        static const unsigned int mode_table[] = {
            O_RDONLY,
            O_RDONLY | O_BINARY,
            O_RDWR,
            O_RDWR | O_BINARY,
            O_WRONLY | O_CREAT | O_TRUNC,
            O_WRONLY | O_CREAT | O_TRUNC | O_BINARY,
            O_RDWR | O_CREAT | O_TRUNC,
            O_RDWR | O_CREAT | O_TRUNC | O_BINARY,
            O_WRONLY | O_APPEND | O_CREAT,
            O_WRONLY | O_APPEND | O_CREAT | O_BINARY,
            O_RDWR | O_APPEND | O_CREAT,
            O_RDWR | O_APPEND | O_CREAT | O_BINARY,
            O_RDWR | O_CREAT,
            O_RDWR | O_CREAT | O_EXCL
        };


        DEBUG_MEMORY_READ(swi_info, 4, &physicalFilenameAddr);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &filemode);
        DEBUG_MEMORY_READ(swi_info + 8, 4, &length);

        if (length >= BUFSIZ) {
            printf("%s:%d: ERROR: filename too large\n",
                    __FUNCTION__, __LINE__);
        }

        i = 0;
        do {
            DEBUG_MEMORY_READ(physicalFilenameAddr + i, 1, &filename[i]);
            i++;
        } while (filename[i - 1]);
        HEX_DEBUG_LOG("fname %s, fmode %d, len %d\n",
            filename, filemode, length);

        /* convert ARM ANGEL filemode into host filemode */
        if (filemode < 14)
            real_openmode = mode_table[filemode];
        else {
            real_openmode = 0;
            printf("%s:%d: ERROR: invalid OPEN mode: %d\n",
                   __FUNCTION__, __LINE__, filemode);
        }


        fd = open(filename, real_openmode, 0644);
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, fd);

        if (fd == -1) {
            HexagonCPU *cpu = hexagon_env_get_cpu(env);
            if (cpu->usefs && g_strrstr(filename, ".so") != NULL
                && errno == ENOENT) {
                /* Didn't find it, so now we also try to open in the
                 * 'search dir':
                 */
                GString *lib_filename_str = g_string_new(cpu->usefs);

                g_string_append_printf(lib_filename_str, "/%s", filename);
                gchar *lib_filename = g_string_free(lib_filename_str, false);
                fd = open(lib_filename, real_openmode, 0644);
                ARCH_SET_THREAD_REG(env, HEX_REG_R00, fd);

                if (fd == -1) {
                    ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
                }
                g_free(lib_filename);
            } else {
                ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            }
        }
    }
    break;

    case SYS_CLOSE:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_CLOSE\n", __FUNCTION__, __LINE__);
        int fd;
        DEBUG_MEMORY_READ(swi_info, 4, &fd);

        if (fd == 0 || fd == 1 || fd == 2) {
            /* silently ignore request to close stdin/stdout */
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        } else {
            int closedret = close(fd);

            if (closedret == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R01,
                                MapError(errno));
            } else {
                ARCH_SET_THREAD_REG(env, HEX_REG_R00, closedret);
            }
        }
    }
    break;

    case SYS_ISERROR:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        break;

    case SYS_ISTTY:
    {
        int fd;
        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        ARCH_SET_THREAD_REG(env, HEX_REG_R00,
                            isatty(fd));
    }
    break;

    case SYS_SEEK:
    {
        int fd;
        int pos;
        int retval;

        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &pos);

        retval = lseek(fd, pos, SEEK_SET);
        if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        } else {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, retval);
        }
    }
    break;

    case SYS_FLEN:
    {
        off_t oldpos;
        off_t len;
        int fd;

        DEBUG_MEMORY_READ(swi_info, 4, &fd);

        oldpos = lseek(fd, 0, SEEK_CUR);
        if (oldpos == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            break;
        }
        len = lseek(fd, 0, SEEK_END);
        if (len == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            break;
        }
        if (lseek(fd, oldpos, SEEK_SET) == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            break;
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, len);
    }
    break;

    case SYS_FTRUNC:
    {
        int fd;
        int retval;
        off64_t size_limit;

        DEBUG_MEMORY_READ(swi_info, 4, &fd);
        DEBUG_MEMORY_READ(swi_info + 4, 8, &size_limit);

        retval = ftruncate(fd, size_limit);
        if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        } else {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, retval);
        }
    }
    break;

  case SYS_OPENDIR:
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
        }
        else
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));

        ARCH_SET_THREAD_REG(env, HEX_REG_R00, dir_index);
        break;
    }
  case SYS_READDIR:
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
        }
        else
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, EBADF);

        guest_dir_entry = ARCH_GET_THREAD_REG(env, HEX_REG_R02) +
            sizeof(int32_t) /* Index entry */ ;
        if (host_dir_entry) {
            vaddr_t guest_dir_ptr = guest_dir_entry;
#if 0
            DEBUG_MEMORY_WRITE(guest_dir_ptr,
                sizeof(host_dir_entry->d_ino),
                host_dir_entry->d_ino);
            guest_dir_ptr += sizeof(host_dir_entry->d_ino);
#endif

            for (i = 0; i < sizeof(host_dir_entry->d_name); i++) {
                DEBUG_MEMORY_WRITE(guest_dir_ptr + i, 1,
                    host_dir_entry->d_name[i]);
                if (! host_dir_entry->d_name[i])
                    break;
            }
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, guest_dir_entry - sizeof(int32_t));
        }
        else
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0 /* NULL */);
        break;
    }
    break;
  case SYS_CLOSEDIR:
  {
        DIR *dir;
        int ret = 0;

        dir = g_list_nth_data(env->dir_list, swi_info);
        if (dir != NULL) {
            ret = closedir(dir);
            if (ret != 0) {
                ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            }
        }
        else
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, EBADF);
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, ret);
        break;
  }

  case SYS_COREDUMP:
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
      case 0x01:
          printf("0x01, Bus Error (Precise BIU error)");
          break;
      case 0x03:
          printf("0x03, Exception observed when EX = 1 (double exception)");
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
      case 0x1a:
          printf("0x1a, Privilege violation: guest mode insn in user mode");
          break;
      case 0x1b:
          printf("0x1b, Privilege violation: "
                 "monitor mode insn ins user/guest mode");
          break;
      case 0x1d:
          printf("0x1d, Multiple writes to same register");
          break;
      case 0x1e:
          printf("0x1e, PC not aligned");
          break;
      case 0x20:
          printf("0x20, Misaligned Load @ 0x%x",
                 ARCH_GET_SYSTEM_REG(env, HEX_SREG_BADVA));
          break;
      case 0x21:
          printf("0x21, Misaligned Store @ 0x%x",
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
      case 0x26:
          printf("0x26, Coprocessor VMEM address error. @ 0x%x",
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
      default:
          printf("Don't know");
          break;
      }
      printf("\nRegister Dump:\n");
      hexagon_dump(env, stdout);
      break;

    case SYS_FTELL:
    {
        int fd;
        off_t current_pos;

        DEBUG_MEMORY_READ(swi_info, 4, &fd);

        current_pos = lseek(fd, 0, SEEK_CUR);
        if (current_pos == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, current_pos);

    }
    break;

    case SYS_TMPNAM:
    {
        char buf[40];
        size4u_t bufptr;
        int id, rc;
        int buflen;
        int ftry = 0;
        buf[39] = 0;

        DEBUG_MEMORY_READ(swi_info, 4, &bufptr);
        DEBUG_MEMORY_READ(swi_info + 4, 4, &id);
        DEBUG_MEMORY_READ(swi_info + 8, 4, &buflen);

        if (buflen < 40) {
            CPUState *cs = env_cpu(env);
            cpu_abort(cs, "Error: %s output buffer too small", __FUNCTION__);
        }
        /*
         * Loop until we find a file that doesn't alread exist.
         */
        do {
            snprintf(buf, 40, "/tmp/sim-tmp-%d-%d", getpid() + ftry, id);
            ftry++;
        } while ((rc = access(buf, F_OK)) == 0);

        for (i = 0; i <= (int) strlen(buf); i++) {
            DEBUG_MEMORY_WRITE(bufptr + i, 1, buf[i]);
        }

        ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
    }
    break;

    case SYS_REMOVE:
    {
        char buf[BUFSIZ];
        size4u_t bufptr;
        int retval;

        DEBUG_MEMORY_READ(swi_info, 4, &bufptr);
        i = 0;
        do {
            DEBUG_MEMORY_READ(bufptr + i, 1, &buf[i]);
            i++;
        } while (buf[i - 1]);

        retval = unlink(buf);
        if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, retval);
    }
    break;

    case SYS_RENAME:
    {
        char buf[BUFSIZ];
        char buf2[BUFSIZ];
        size4u_t bufptr, bufptr2;
        int retval;

        DEBUG_MEMORY_READ(swi_info, 4, &bufptr);
        DEBUG_MEMORY_READ(swi_info + 8, 4, &bufptr2);
        i = 0;
        do {
            DEBUG_MEMORY_READ(bufptr + i, 1, &buf[i]);
            i++;
        } while (buf[i - 1]);
        i = 0;
        do {
            DEBUG_MEMORY_READ(bufptr2 + i, 1, &buf2[i]);
            i++;
        } while (buf2[i - 1]);

        retval = rename(buf, buf2);
        if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, retval);
    }
    break;

    case SYS_CLOCK:
    {
        int retval = time(NULL);
        if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            break;
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, retval * 100);
    }
    break;

    case SYS_TIME:
    {
        int retval = time(NULL);
        if (retval == -1) {
            ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
            ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(errno));
            break;
        }
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, retval);
    }
    break;

    case SYS_ERRNO:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, errno);
        break;

    case SYS_READ_CYCLES:
    case SYS_READ_TCYCLES:
    {
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, 0);
        break;
    }

    case SYS_READ_ICOUNT:
    {
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, 0);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, 0);
        break;
    }

    case SYS_READ_PCYCLES:
    {
        ARCH_SET_THREAD_REG(env, HEX_REG_R00,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLELO));
        ARCH_SET_THREAD_REG(env, HEX_REG_R01,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_PCYCLEHI));
        break;
    }

    case SYS_APP_REPORTED:
        break;

    case SYS_PROF_ON:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(ENOSYS));
        qemu_log_mask(LOG_UNIMP, "SYS_PROF_ON is bogus on QEMU!\n");
        break;
    case SYS_PROF_OFF:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(ENOSYS));
        qemu_log_mask(LOG_UNIMP, "SYS_PROF_OFF bogus on QEMU!\n");
        break;
    case SYS_PROF_STATSRESET:
        ARCH_SET_THREAD_REG(env, HEX_REG_R00, -1);
        ARCH_SET_THREAD_REG(env, HEX_REG_R01, MapError(ENOSYS));
        qemu_log_mask(LOG_UNIMP, "SYS_PROF_STATSRESET bogus on QEMU!\n");
        break;
    case SYS_DUMP_PMU_STATS:
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
    int retval = 0;
    target_ulong what_swi = ARCH_GET_THREAD_REG(env, HEX_REG_R00);

    retval = sim_handle_trap_functional(env);

    if(!retval) {
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

#define CHECK_EX 0

void hexagon_cpu_do_interrupt(CPUState *cs)

{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    HEX_DEBUG_LOG("%s: tid %d, event 0x%x, cause 0x%x\n",
      __FUNCTION__, env->threadId, cs->exception_index, env->cause_code);
    qemu_log_mask(CPU_LOG_INT,
            "\t%s: event 0x%x, cause 0x%x\n", __func__,
            cs->exception_index, env->cause_code);

#if CHECK_EX
    const uint32_t ssr = ARCH_GET_SYSTEM_REG(env, HEX_SREG_SSR);
    target_ulong EX = GET_SSR_FIELD(SSR_EX, ssr);
    if (EX && env->cause_code != HEX_CAUSE_IMPRECISE_NMI) {
        cpu_abort(cs, "hexagon_cpu_do_interrupt: EX already set, exiting\n");
    }
#endif

        int int_num = -1;
    switch (cs->exception_index) {
    case HEX_EVENT_INT0:
    case HEX_EVENT_INT1:
    case HEX_EVENT_INT2:
    case HEX_EVENT_INT3:
    case HEX_EVENT_INT4:
    case HEX_EVENT_INT5:
    case HEX_EVENT_INT6:
    case HEX_EVENT_INT7:
    case HEX_EVENT_INT8:
    case HEX_EVENT_INT9:
    case HEX_EVENT_INTA:
    case HEX_EVENT_INTB:
    case HEX_EVENT_INTC:
    case HEX_EVENT_INTD:
    case HEX_EVENT_INTE:
    case HEX_EVENT_INTF:
        HEX_DEBUG_LOG("\tHEX_EVENT_INT%d: tid = %u, "
            "PC = 0x%x, next_PC = 0x%x\n",
            cs->exception_index - HEX_EVENT_INT0,
            env->threadId,
            ARCH_GET_THREAD_REG(env, HEX_REG_PC),
            env->next_PC);
        int_num = cs->exception_index - HEX_EVENT_INT0;

        /* Clear pending: */
        hexagon_clear_interrupts(env, 1 << int_num);

        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 0, cs->exception_index);
        env->branch_taken = 1;
        break;

    case HEX_EVENT_TRAP0:
        HEX_DEBUG_LOG("\ttid = %u, trap0, pc = 0x%x, elr = 0x%x, "
            "index 0x%x, cause 0x%x\n",
            env->threadId,
            ARCH_GET_THREAD_REG(env, HEX_REG_PC),
            env->next_PC,
            cs->exception_index,
            env->cause_code);

        if (env->cause_code == 0) {
            sim_handle_trap(env);
        }

        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        env->branch_taken = 1;
        break;

    case HEX_EVENT_TRAP1:
        HEX_DEBUG_LOG("\ttid = %u, trap1, pc = 0x%x, elr = 0x%x, "
            "index 0x%x, cause 0x%x\n",
            env->threadId,
            ARCH_GET_THREAD_REG(env, HEX_REG_PC),
            env->next_PC,
            cs->exception_index,
            env->cause_code);
        HEX_DEBUG_LOG("\tEVB 0x%x, shifted index %d/0x%x, final 0x%x\n",
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB),
            cs->exception_index << 2,
            cs->exception_index << 2,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB)|(cs->exception_index << 2));

        hexagon_ssr_set_cause(env, env->cause_code);
        set_addresses(env, 4, cs->exception_index);
        env->branch_taken = 1;
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
        switch(env->cause_code) {
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
        ARCH_SET_SYSTEM_REG(env, HEX_SREG_ELR, env->next_PC);
        ARCH_SET_THREAD_REG(env, HEX_REG_PC,
            ARCH_GET_SYSTEM_REG(env, HEX_SREG_EVB) |
            (cs->exception_index << 2));
        break;

    case HEX_EVENT_PRECISE:
        switch(env->cause_code) {
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
        switch(env->cause_code) {
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
        printf("%s:%d: throw error\n", __FUNCTION__, __LINE__);
        cpu_abort(cs, "Hexagon Unsupported exception 0x%x/0x%x\n",
                  cs->exception_index, env->cause_code);
        break;
    }

    cs->exception_index = HEX_EVENT_NONE;
}

void register_trap_exception(CPUHexagonState *env, uintptr_t next_pc,
    int traptype, int imm)

{
    HEX_DEBUG_LOG("%s:\n\ttid = %d, pc = 0x%x, npc = 0x%lx, traptype %d, "
        "imm %d\n",
        __FUNCTION__, env->threadId, ARCH_GET_THREAD_REG(env, HEX_REG_PC),
        next_pc, traptype, imm);

    CPUState *cs = env_cpu(env);
    /* assert(cs->exception_index == HEX_EVENT_NONE); */

    cs->exception_index = (traptype == 0) ? HEX_EVENT_TRAP0 : HEX_EVENT_TRAP1;

    env->cause_code = imm;
    cpu_loop_exit(cs);
}
#endif

