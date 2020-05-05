/*
 *  Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#include "sysemu/runstate.h"
#include "internal.h"

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

static size1u_t swi_mem_read1(CPUHexagonState *env, vaddr_t paddr)

{
    size1u_t data = 0;
    unsigned mmu_idx = cpu_mmu_index(env, false);
    data = cpu_ldub_mmuidx_ra(env, paddr, mmu_idx, GETPC());

    return data;
}

static size2u_t swi_mem_read2(CPUHexagonState *env, vaddr_t paddr)

{
    size2u_t data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 2; i++) {
        tdata = swi_mem_read1(env, paddr + i);
        data = ((size2u_t) tdata << (8 * i)) | data;
    }

    return data;
}

static target_ulong swi_mem_read4(CPUHexagonState *env, vaddr_t paddr)

{
    target_ulong data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 4; i++) {
        tdata = swi_mem_read1(env, paddr + i);
        data = ((target_ulong) tdata << (8 * i)) | data;
    }

    return data;
}


static size8u_t swi_mem_read8(CPUHexagonState *env, vaddr_t paddr)

{
    size8u_t data = 0;
    size1u_t tdata;
    int i;

    for (i = 0; i < 8; i++) {
        tdata = swi_mem_read1(env, paddr + i);
        data = ((size8u_t) tdata << (8 * i)) | data;
    }

    return data;
}

static void swi_mem_write1(CPUHexagonState *env, vaddr_t paddr, size1u_t value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stb_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void swi_mem_write2(CPUHexagonState *env, vaddr_t paddr, size2u_t value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stw_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void swi_mem_write4(CPUHexagonState *env, vaddr_t paddr,
    target_ulong value)

{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stl_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void swi_mem_write8(CPUHexagonState *env, vaddr_t paddr, size8u_t value)
{
    unsigned mmu_idx = cpu_mmu_index(env, false);
    cpu_stq_mmuidx_ra(env, paddr, value, mmu_idx, GETPC());
}

static void tools_memory_read(CPUHexagonState *env, vaddr_t vaddr, int size,
    void *retptr)

{
    vaddr_t paddr = vaddr;

    switch (size) {
    case 1:
        (*(size1u_t *) retptr) = swi_mem_read1(env, paddr);
        break;
    case 2:
        (*(size2u_t *) retptr) = swi_mem_read2(env, paddr);
        break;
    case 4:
        (*(target_ulong *) retptr) = swi_mem_read4(env, paddr);
        break;
    case 8:
        (*(size8u_t *) retptr) = swi_mem_read8(env, paddr);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
    }
}

static void tools_memory_write(CPUHexagonState *env, vaddr_t vaddr, int size,
    size8u_t data)
{
    paddr_t paddr = vaddr;

    switch (size) {
    case 1:
        swi_mem_write1(env, paddr, (size1u_t) data);
        log_store32(env, paddr, (size4u_t)data, 1, 0);
        break;
    case 2:
        swi_mem_write2(env, paddr, (size2u_t) data);
        log_store32(env, paddr, (size4u_t)data, 2, 0);
        break;
    case 4:
        swi_mem_write4(env, paddr, (target_ulong) data);
        log_store32(env, paddr, (size4u_t)data, 4, 0);
        break;
    case 8:
        swi_mem_write8(env, paddr, (size8u_t) data);
        log_store32(env, paddr, data, 8, 0);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
    }
}

static int tools_memory_readLocked(CPUHexagonState *env, vaddr_t vaddr,
    int size, void *retptr)

{
    vaddr_t paddr = vaddr;
    int ret = 0;

    switch (size) {
    case 4:
        (*(target_ulong *) retptr) = swi_mem_read4(env, paddr);
        break;
    case 8:
        (*(size8u_t *) retptr) = swi_mem_read8(env, paddr);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
        ret = 1;
        break;
     }

    return ret;
}

static int tools_memory_write_locked(CPUHexagonState *env, vaddr_t vaddr,
    int size, size8u_t data)

{
    paddr_t paddr = vaddr;
    int ret = 0;

    switch (size) {
    case 4:
        swi_mem_write4(env, paddr, (target_ulong) data);
        log_store32(env, vaddr, (size4u_t)data, 4, 0);
        break;
    case 8:
        swi_mem_write8(env, paddr, (size8u_t) data);
        log_store64(env, vaddr, data, 8, 0);
        break;
    default:
        printf("%s:%d: ERROR: bad size = %d!\n", __FUNCTION__, __LINE__, size);
        ret = 1;
        break;
    }

    return ret;
}

#define arch_get_thread_reg(ENV,REG)     ((ENV)->gpr[(REG)])
#define arch_set_thread_reg(ENV,REG,VAL) ((ENV)->gpr[(REG)] = (VAL))
#define arch_set_global_reg(ENV,REG,VAL) ((ENV)->sreg[(REG)] = (VAL))
#define arch_get_thread_sreg(ENV, REG)     ((ENV)->sreg[(REG)])
#define DEBUGMEMORYREADg(ADDR,SIZE,PTR) \
    tools_memory_read(env, ADDR, SIZE, PTR)
#define DEBUGMEMORYWRITE(ADDR,SIZE,DATA) \
    tools_memory_write(env,ADDR,SIZE,(size8u_t)DATA)
#define DEBUGMEMORYREADgLocked(ADDR,SIZE,PTR) \
    tools_memory_readLocked(env, ADDR, SIZE, PTR)
#define DEBUGMEMORYWRITELOCKED(ADDR,SIZE,DATA) \
    tools_memory_write_locked(env,ADDR,SIZE,(size8u_t)DATA)


static int MapError(int ERR)
{
    errno = ERR;
    return (ERR);
}

static int sim_handle_trap_functional(CPUHexagonState *env)

{
    target_ulong what_swi = arch_get_thread_reg(env, HEX_REG_R00);
    target_ulong swi_info = arch_get_thread_reg(env, HEX_REG_R01);
    int i = 0, c;
    int retval = 1;

    HEX_DEBUG_LOG("%s:%d: what_swi 0x%x, swi_info 0x%x\n",
                  __FUNCTION__, __LINE__, what_swi, swi_info);
    switch (what_swi) {
    case SYS_HEAPINFO:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_HEAPINFO\n", __FUNCTION__, __LINE__);
#if 0
        size4u_t bufptr;

        DEBUGMEMORYREADg(swi_info, 4, &bufptr);
         DEBUGMEMORYWRITE(bufptr+0, 4, 0);
        DEBUGMEMORYWRITE(bufptr+1, 4, 0);
        DEBUGMEMORYWRITE(bufptr+2, 4, 0);
         DEBUGMEMORYWRITE(bufptr+3, 4, 0);
#endif
    }
    break;

    case SYS_GET_CMDLINE:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_GET_CMDLINE\n", __FUNCTION__, __LINE__);
        target_ulong bufptr;
        target_ulong bufsize;

        DEBUGMEMORYREADg(swi_info, 4, &bufptr);
        DEBUGMEMORYREADg(swi_info + 4, 4, &bufsize);

        const target_ulong to_copy =
            (bufsize <= (unsigned int) strlen(env->cmdline))
                ? (bufsize - 1)
                : strlen(env->cmdline);

        HEX_DEBUG_LOG("\tcmdline '%s' len to copy %d buf max %d\n",
            env->cmdline, to_copy, bufsize);
        for (i = 0; i < (int) to_copy; i++) {
            DEBUGMEMORYWRITE(bufptr + i, 1, (size8u_t) env->cmdline[i]);
        }
      DEBUGMEMORYWRITE(bufptr + i, 1, (size8u_t) 0);
      arch_set_thread_reg(env, HEX_REG_R00, 0);
    }
    break;

    case SYS_EXCEPTION:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_EXCEPTION\n\tProgram terminated successfully\n",
                       __FUNCTION__, __LINE__);
        target_ulong ret = -1;
        DEBUGMEMORYREADg(swi_info, 4, &ret);

        arch_set_global_reg(env, HEX_SREG_MODECTL, 0);

        exit(ret);
    }
    break;

    case SYS_WRITEC:
    {
        FILE *fp = stdout;
        DEBUGMEMORYREADg(swi_info, 1, &c);
        fprintf(fp, "%c", c);
        fflush(fp);
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
        do {
            DEBUGMEMORYREADg(swi_info + i, 1, &c);
            fprintf(fp, "%c", c);
            i++;
        } while (c);
        fflush(fp);
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

        DEBUGMEMORYREADg(swi_info, 4, &fd);
        DEBUGMEMORYREADg(swi_info + 4, 4, &bufaddr);
        DEBUGMEMORYREADg(swi_info + 8, 4, &count);

        if ((buf = (char *) g_malloc(count)) == NULL) {
            printf("%s:%d: ERROR: Couldn't allocate temporary buffer (%d bytes)",
                    __FUNCTION__, __LINE__, count);
        }

        for (i = 0; i < count; i++) {
            DEBUGMEMORYREADg(bufaddr + i, 1, &buf[i]);
        }
        retval = write(fd, buf, count);
        if (retval == count) {
            arch_set_thread_reg(env, HEX_REG_R00, 0);
        } else if (retval == -1) {
            arch_set_thread_reg(env, HEX_REG_R00, retval);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
        } else {
            arch_set_thread_reg(env, HEX_REG_R00, count - retval);
        }
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

        DEBUGMEMORYREADg(swi_info, 4, &fd);
        DEBUGMEMORYREADg(swi_info + 4, 4, &bufaddr);
        DEBUGMEMORYREADg(swi_info + 8, 4, &count);

        if ((buf = (char *) g_malloc(count)) == NULL) {
            CPUState *cs = env_cpu(env);
            cpu_abort(cs,
                "Error: sim_handle_trap couldn't allocate temporybuffer (%d bytes)",
                 count);
        }

        retval = read(fd, buf, count);
        for (i = 0; i < retval; i++) {
            DEBUGMEMORYWRITE(bufaddr + i, 1, buf[i]);
        }
        if (retval == count) {
            arch_set_thread_reg(env, HEX_REG_R00, 0);
        } else if (retval == -1) {
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
        } else {
            arch_set_thread_reg(env, HEX_REG_R00, count - retval);
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

        DEBUGMEMORYREADg(swi_info, 4, &fcntlFileDesc);
        DEBUGMEMORYREADg(swi_info + 4, 4, &fcntlCmd);
        DEBUGMEMORYREADg(swi_info + 8, 4, &fcntlPBA);
        DEBUGMEMORYREADg(fcntlPBA, 4, &sysfcntl.l_type);
        DEBUGMEMORYREADg(fcntlPBA + 0x4, 4, &sysfcntl.l_whence);
        DEBUGMEMORYREADg(fcntlPBA + 0x8, 4, &sysfcntl.l_start);
        DEBUGMEMORYREADg(fcntlPBA + 0xc, 4, &sysfcntl.l_len);

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
            DEBUGMEMORYWRITE(fcntlPBA, 4, sysfcntl.l_type);
            DEBUGMEMORYWRITE(fcntlPBA + 0x4, 4, sysfcntl.l_whence);
            DEBUGMEMORYWRITE(fcntlPBA + 0x8, 4, sysfcntl.l_start);
          DEBUGMEMORYWRITE(fcntlPBA + 0xc, 4, sysfcntl.l_len);
        }
        arch_set_thread_reg(env, HEX_REG_R00, rc);
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


        DEBUGMEMORYREADg(swi_info, 4, &physicalFilenameAddr);
        DEBUGMEMORYREADg(swi_info + 4, 4, &filemode);
        DEBUGMEMORYREADg(swi_info + 8, 4, &length);

        if (length >= BUFSIZ) {
            printf("%s:%d: ERROR: filename too large\n",
                    __FUNCTION__, __LINE__);
        }

        i = 0;
        do {
            DEBUGMEMORYREADg(physicalFilenameAddr + i, 1, &filename[i]);
            i++;
        } while (filename[i - 1]);
        HEX_DEBUG_LOG("fname %s, fmode %d, len %d\n", filename, filemode, length);

        /* convert ARM ANGEL filemode into host filemode */
        if (filemode < 14)
            real_openmode = mode_table[filemode];
        else {
            real_openmode = 0;
            printf("%s:%d: ERROR: invalid OPEN mode: %d\n",
                   __FUNCTION__, __LINE__, filemode);
        }


        fd = open(filename, real_openmode, 0644);
        arch_set_thread_reg(env, HEX_REG_R00, fd);

        if (fd == -1) {
            printf("ERROR: fopen failed, errno = %d\n", errno);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
        }
    }
    break;

    case SYS_CLOSE:
    {
        HEX_DEBUG_LOG("%s:%d: SYS_CLOSE\n", __FUNCTION__, __LINE__);
        int fd;
        DEBUGMEMORYREADg(swi_info, 4, &fd);

        if (fd == 0 || fd == 1 || fd == 2) {
            /* silently ignore request to close stdin/stdout */
            arch_set_thread_reg(env, HEX_REG_R00, 0);
        } else {
            int closedret = close(fd);

            if (closedret == -1) {
            arch_set_thread_reg(env, HEX_REG_R01,
                                MapError(errno));
            } else {
                arch_set_thread_reg(env, HEX_REG_R00, closedret);
            }
        }
    }
    break;

    case SYS_ISERROR:
        arch_set_thread_reg(env, HEX_REG_R00, 0);
        break;

    case SYS_ISTTY:
    {
        int fd;
        DEBUGMEMORYREADg(swi_info, 4, &fd);
        arch_set_thread_reg(env, HEX_REG_R00,
                            isatty(fd));
    }
    break;

    case SYS_SEEK:
    {
        int fd;
        int pos;
        int retval;

        DEBUGMEMORYREADg(swi_info, 4, &fd);
        DEBUGMEMORYREADg(swi_info + 4, 4, &pos);

        retval = lseek(fd, pos, SEEK_SET);
        if (retval == -1) {
            arch_set_thread_reg(env, HEX_REG_R00, -1);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
        } else {
            arch_set_thread_reg(env, HEX_REG_R00, 0);
        }
    }
    break;

    case SYS_FLEN:
    {
        off_t oldpos;
        off_t len;
        int fd;

        DEBUGMEMORYREADg(swi_info, 4, &fd);

        oldpos = lseek(fd, 0, SEEK_CUR);
        if (oldpos == -1) {
            arch_set_thread_reg(env, HEX_REG_R00, -1);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
            break;
        }
        len = lseek(fd, 0, SEEK_END);
        if (len == -1) {
            arch_set_thread_reg(env, HEX_REG_R00, -1);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
            break;
        }
        if (lseek(fd, oldpos, SEEK_SET) == -1) {
            arch_set_thread_reg(env, HEX_REG_R00, -1);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
            break;
        }
        arch_set_thread_reg(env, HEX_REG_R00, len);
    }
    break;

  case SYS_COREDUMP:
      printf("CRASH!\n");
      printf("I think the exception was: ");
      switch (0x0ff & arch_get_thread_sreg(env, HEX_SREG_SSR)) {
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
      case HEX_EXCP_FETCH_NO_XPAGE:
          printf("0x%x, Privilege violation: User/Guest mode execute"
                 " to page with no execute permissions",
                 HEX_EXCP_FETCH_NO_XPAGE);
          break;
      case HEX_EXCP_FETCH_NO_UPAGE:
          printf("0x%x, Privilege violation: "
                 "User mode exececute to page with no user permissions",
                 HEX_EXCP_FETCH_NO_UPAGE);
          break;
      case HEX_EXCP_INVALID_PACKET:
          printf("0x%x, Invalid packet",
                 HEX_EXCP_INVALID_PACKET);
          break;
      case 0x1a:
          printf("0x1a, Privelege violation: guest mode insn in user mode");
          break;
      case 0x1b:
          printf("0x1b, Privelege violation: "
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
                 arch_get_thread_sreg(env, HEX_SREG_BADVA));
          break;
      case 0x21:
          printf("0x21, Misaligned Store @ 0x%x",
                 arch_get_thread_sreg(env, HEX_SREG_BADVA));
          break;
      case HEX_EXCP_PRIV_NO_READ:
          printf("0x%x, Privilege violation: user/guest read permission @ 0x%x",
                 HEX_EXCP_PRIV_NO_READ,
                 arch_get_thread_sreg(env, HEX_SREG_BADVA));
          break;
      case HEX_EXCP_PRIV_NO_WRITE:
          printf("0x%x, Privilege violation: "
                 "user/guest write permission @ 0x%x",
                 HEX_EXCP_PRIV_NO_WRITE,
                 arch_get_thread_sreg(env, HEX_SREG_BADVA));
          break;
      case HEX_EXCP_PRIV_NO_UREAD:
          printf("0x%x, Privilege violation: user read permission @ 0x%x",
                 HEX_EXCP_PRIV_NO_UREAD,
                 arch_get_thread_sreg(env, HEX_SREG_BADVA));
          break;
      case HEX_EXCP_PRIV_NO_UWRITE:
          printf("0x%x, Privilege violation: user write permission @ 0x%x",
                 HEX_EXCP_PRIV_NO_UWRITE,
                 arch_get_thread_sreg(env, HEX_SREG_BADVA));
          break;
      case 0x26:
          printf("0x26, Coprocessor VMEM address error. @ 0x%x",
                 arch_get_thread_sreg(env, HEX_SREG_BADVA));
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

        DEBUGMEMORYREADg(swi_info, 4, &fd);

        current_pos = lseek(fd, 0, SEEK_CUR);
        if (current_pos == -1) {
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
        }
        arch_set_thread_reg(env, HEX_REG_R00, current_pos);

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

        DEBUGMEMORYREADg(swi_info, 4, &bufptr);
        DEBUGMEMORYREADg(swi_info + 4, 4, &id);
        DEBUGMEMORYREADg(swi_info + 8, 4, &buflen);

        if (buflen < 40) {
            CPUState *cs = env_cpu(env);
            cpu_abort(cs, "Error: sim_handle_trap output buffer too small");
        }
        /*
         * Loop until we find a file that doesn't alread exist.
         */
        do {
            snprintf(buf, 40, "/tmp/sim-tmp-%d-%d", getpid() + ftry, id);
            ftry++;
        } while ((rc = access(buf, F_OK)) == 0);

        for (i = 0; i <= (int) strlen(buf); i++) {
            DEBUGMEMORYWRITE(bufptr + i, 1, buf[i]);
        }

        arch_set_thread_reg(env, HEX_REG_R00, 0);
    }
    break;

    case SYS_REMOVE:
    {
        char buf[BUFSIZ];
        size4u_t bufptr;
        int retval;

        DEBUGMEMORYREADg(swi_info, 4, &bufptr);
        i = 0;
        do {
            DEBUGMEMORYREADg(bufptr + i, 1, &buf[i]);
            i++;
        } while (buf[i - 1]);

        retval = unlink(buf);
        if (retval == -1) {
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
        }
        arch_set_thread_reg(env, HEX_REG_R00, retval);
    }
    break;

    case SYS_RENAME:
    {
        char buf[BUFSIZ];
        char buf2[BUFSIZ];
        size4u_t bufptr, bufptr2;
        int retval;

        DEBUGMEMORYREADg(swi_info, 4, &bufptr);
        DEBUGMEMORYREADg(swi_info + 8, 4, &bufptr2);
        i = 0;
        do {
            DEBUGMEMORYREADg(bufptr + i, 1, &buf[i]);
            i++;
        } while (buf[i - 1]);
        i = 0;
        do {
            DEBUGMEMORYREADg(bufptr2 + i, 1, &buf2[i]);
            i++;
        } while (buf2[i - 1]);

        retval = rename(buf, buf2);
        if (retval == -1) {
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
        }
        arch_set_thread_reg(env, HEX_REG_R00, retval);
    }
    break;

    case SYS_CLOCK:
    {
        int retval = time(NULL);
        if (retval == -1) {
            arch_set_thread_reg(env, HEX_REG_R00, -1);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
            break;
        }
        arch_set_thread_reg(env, HEX_REG_R00, retval * 100);
    }

    case SYS_TIME:
    {
        int retval = time(NULL);
        if (retval == -1) {
            arch_set_thread_reg(env, HEX_REG_R00, -1);
            arch_set_thread_reg(env, HEX_REG_R01, MapError(errno));
            break;
        }
        arch_set_thread_reg(env, HEX_REG_R00, retval);
    }
    break;

    case SYS_ERRNO:
        arch_set_thread_reg(env, HEX_REG_R00, errno);
        break;

    case SYS_PROF_ON:
    case SYS_PROF_OFF:
    case SYS_PROF_STATSRESET:
    case SYS_DUMP_PMU_STATS:
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
    target_ulong what_swi = arch_get_thread_reg(env, HEX_REG_R00);

    retval = sim_handle_trap_functional(env);

    if(!retval) {
        fprintf(stderr, "I don't know that swi request: 0x%x\n", what_swi);
    }

    return retval;
}

static int do_store_exclusive(CPUHexagonState *env, bool single)
{
    target_ulong addr;
    target_ulong val;
    uint64_t val_i64;
    int segv = 0;
    int reg;

    addr = env->llsc_addr;
    reg = env->llsc_reg;

    env->pred[reg] = 0;
    if (single) {
        segv = DEBUGMEMORYREADgLocked(addr, 4, &val);
    } else {
        segv = DEBUGMEMORYREADgLocked(addr, 8, &val_i64);
    }
    if (!segv) {
        if (single) {
            if (val == env->llsc_val) {
                segv = DEBUGMEMORYWRITELOCKED(addr, 4,
                                              (size8u_t)(env->llsc_newval));
                if (!segv) {
                    env->pred[reg] = 0xff;
                }
            }
        } else {
            if (val_i64 == env->llsc_val_i64) {
                segv = DEBUGMEMORYWRITELOCKED(addr, 8,
                                              (size8u_t)(env->llsc_newval_i64));
                if (!segv) {
                    env->pred[reg] = 0xff;
                }
            }
        }
    }
    env->llsc_addr = ~0;
    if (!segv) {
        env->next_PC += 4;
    }

    return segv;
}

void hexagon_cpu_do_interrupt(CPUState *cs)

{
    HexagonCPU *cpu = HEXAGON_CPU(cs);
    CPUHexagonState *env = &cpu->env;

    switch (cs->exception_index) {
    case HEX_EXCP_VIC0:
    case HEX_EXCP_VIC1:
    case HEX_EXCP_VIC2:
    case HEX_EXCP_VIC3:
        sim_handle_trap(env);
        HEX_DEBUG_LOG("\tVIC interrupt ignored\n");
        break;

    case HEX_EXCP_TRAP0:
        sim_handle_trap(env);
        env->sreg[HEX_SREG_ELR] = env->next_PC;
        env->sreg[HEX_SREG_SSR] |= SSR_EX;
        env->sreg[HEX_SREG_SSR] &= ~(SSR_CAUSE);
        env->sreg[HEX_SREG_SSR] |= cs->exception_index & (SSR_CAUSE);

        target_ulong evb = env->sreg[HEX_SREG_EVB];
        target_ulong trap0_inst;
        DEBUGMEMORYREADg(evb+(8<<2), 4, &trap0_inst);
        HEX_DEBUG_LOG("\tEVB = 0x%x, trap0 = 0x%x, new PC = 0x%x\n",
                      evb, trap0_inst, evb + (8<<2));

        fCHECK_PCALIGN(addr);
        env->branch_taken = 1;
        env->next_PC = evb + (8<<2);
        env->gpr[HEX_REG_PC] = evb + (8<<2);
        cs->exception_index = HEX_EXCP_NONE;
        break;

    case HEX_EXCP_SC4:
        do_store_exclusive(env, true);
        break;

      case HEX_EXCP_TLBMISSX_CAUSE_NORMAL:
      case HEX_EXCP_TLBMISSX_CAUSE_NEXTPAGE:
          qemu_log_mask(CPU_LOG_MMU,
                        "TLB miss EX exception (0x%x) caught: "
                        "PC = 0x%" PRIx32 ", BADVA = 0x%" PRIx32 "\n",
                        cs->exception_index, env->gpr[HEX_REG_PC],
                        env->sreg[HEX_SREG_BADVA]);
          env->sreg[HEX_SREG_ELR] = env->gpr[HEX_REG_PC];
          env->sreg[HEX_SREG_SSR] =
              (env->sreg[HEX_SREG_SSR] & ~SSR_CAUSE) | cs->exception_index;
          env->sreg[HEX_SREG_SSR] |= SSR_EX;
          env->gpr[HEX_REG_PC] = env->sreg[HEX_SREG_EVB] | (4 << 2);
          break;
      case HEX_EXCP_TLBMISSRW_CAUSE_READ:
      case HEX_EXCP_TLBMISSRW_CAUSE_WRITE:
          qemu_log_mask(CPU_LOG_MMU,
                        "TLB miss RW exception (0x%x) caught: "
                        "PC = 0x%" PRIx32 ", BADVA = 0x%" PRIx32 "\n",
                        cs->exception_index, env->gpr[HEX_REG_PC],
                        env->sreg[HEX_SREG_BADVA]);
          env->sreg[HEX_SREG_ELR] = env->gpr[HEX_REG_PC];
          /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
          env->sreg[HEX_SREG_SSR] =
              (env->sreg[HEX_SREG_SSR] & ~SSR_CAUSE) | cs->exception_index;
          env->sreg[HEX_SREG_SSR] |= SSR_EX;
          env->gpr[HEX_REG_PC] = env->sreg[HEX_SREG_EVB] | (6 << 2);
          break;

      case HEX_EXCP_FETCH_NO_XPAGE:
      case HEX_EXCP_FETCH_NO_UPAGE:
      case HEX_EXCP_PRIV_NO_READ:
      case HEX_EXCP_PRIV_NO_UREAD:
      case HEX_EXCP_PRIV_NO_WRITE:
      case HEX_EXCP_PRIV_NO_UWRITE:
          qemu_log_mask(CPU_LOG_MMU,
                        "MMU permission exception (0x%x) caught: "
                        "PC = 0x%" PRIx32 ", BADVA = 0x%" PRIx32 "\n",
                        cs->exception_index, env->gpr[HEX_REG_PC],
                        env->sreg[HEX_SREG_BADVA]);
          env->sreg[HEX_SREG_ELR] = env->gpr[HEX_REG_PC];
          /* env->sreg[HEX_SREG_BADVA] is set when the exception is raised */
          env->sreg[HEX_SREG_SSR] =
              (env->sreg[HEX_SREG_SSR] & ~SSR_CAUSE) | cs->exception_index;
          env->sreg[HEX_SREG_SSR] |= SSR_EX;
          env->gpr[HEX_REG_PC] = env->sreg[HEX_SREG_EVB] | (2 << 2);
          break;

      case HEX_EXCP_PRIV_USER_NO_SINSN:
          env->sreg[HEX_SREG_ELR] = env->gpr[HEX_REG_PC];
          env->sreg[HEX_SREG_SSR] =
              (env->sreg[HEX_SREG_SSR] & ~SSR_CAUSE) | cs->exception_index;
          env->sreg[HEX_SREG_SSR] |= SSR_EX;
          env->gpr[HEX_REG_PC] = env->sreg[HEX_SREG_EVB] | (2 << 2);
          break;

      case HEX_EXCP_IMPRECISE_MULTI_TLB_MATCH:
          /*
           * FIXME
           * Imprecise exceptions are delivered to all HW threads in
           * run or wait mode
           */
          /* After the exception handler, return to the next packet */
          env->sreg[HEX_SREG_ELR] = env->gpr[HEX_REG_PC] + 4;
          env->sreg[HEX_SREG_SSR] =
              (env->sreg[HEX_SREG_SSR] & ~SSR_CAUSE) | cs->exception_index;
          env->sreg[HEX_SREG_SSR] |= SSR_EX;
          env->sreg[HEX_SREG_DIAG] = (0x4 << 4) | (env->sreg[HEX_SREG_HTID] & 0xf);
          env->gpr[HEX_REG_PC] = env->sreg[HEX_SREG_EVB] | (1 << 2);
          break;

      default:
        printf("%s:%d: throw error\n", __FUNCTION__, __LINE__);
        cpu_abort(cs, "Hexagon Unsupported exception %d/0x%x\n",
                  cs->exception_index, cs->exception_index);
        break;
    }
}

void register_trap_exception(CPUHexagonState *env, uintptr_t next_pc,
    int traptype, int imm)

{
    HEX_DEBUG_LOG("%s:\n\tpc = 0x%x, npc = 0x%lx, type %d, imm %d\n",
        __FUNCTION__, env->gpr[HEX_REG_PC], next_pc, traptype, imm);

    CPUState *cs = env_cpu(env);
    cs->exception_index = (traptype == 0) ? HEX_EXCP_TRAP0 : HEX_EXCP_TRAP1;
    cpu_loop_exit(cs);
}

