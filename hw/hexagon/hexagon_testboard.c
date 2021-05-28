/*
 * Hexagon Baseboard System emulation.
 *
 * Copyright (c) 2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */


#include "qemu/osdep.h"
#include "qemu/units.h"
#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/qtimer.h"
#include "hw/hexagon/machine_configs.h"
#include "hw/intc/l2vic.h"
#include "hw/loader.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "elf.h"
#include "cpu.h"
#include "hex_mmu.h"
#include "hmx/ext_hmx.h"
#include "include/migration/cpu.h"
#include "include/sysemu/sysemu.h"
#include "target/hexagon/internal.h"

typedef enum {
  v66_rev = 0xa666,
  v68_rev = 0x8d68,
  v69_rev = 0x8c69,
  v73_rev = 0x8c73,
} Rev_t;

static hexagon_config_table *cfgTable;
static hexagon_config_extensions *cfgExtensions;
static bool syscfg_is_linux = false;

const rev_features_t rev_features_v68 = {
};

const options_struct options_struct_v68 = {
    .l2tcm_base  = 0,  /* FIXME - Should be l2tcm_base ?? */
    .hmx_mac_fxp_callback = (void *)0,
    .hmx_mac_flt_callback = (void *)0,
};

const arch_proc_opt_t arch_proc_opt_v68 = {
    .vtcm_size = VTCM_SIZE,
    .vtcm_offset = VTCM_OFFSET,
    .dmadebugfile = NULL,
    .hmxdebuglvl = 0,
    .hmx_output_depth = 32,
    .hmx_spatial_size = 6,
    .hmx_channel_size = 5,
    .hmx_block_size = 2048,
    .hmx_mxmem_debug_acc_preload = 0,
    .pmu_enable = 0,
    .hmxdebugfile = 0,
    .hmx_mxmem_debug = 0,
    .hmxaccpreloadfile = 0,
    .hmxarray_new = 0,
    .hmxmpytrace = 0,
    .hmx_v1 = 0,
    .hmx_power_config = 0,
    .hmx_8x4_mpy_mode = 0,
    .hmx_group_conv_mode = 1,
    .dmadebug_verbosity = 0,
    .xfp_inexact_enable = 1,
    .xfp_cvt_frac = 13,
    .xfp_cvt_int = 3,
    .QDSP6_DMA_PRESENT     = 1,
    .QDSP6_MX_FP_PRESENT   = 1,
    .QDSP6_MX_RATE = 8,
    .QDSP6_MX_FP_RATE = 4,
    .QDSP6_MX_FP_ACC_INT = 7,
    .QDSP6_MX_FP_ACC_FRAC = 22,
    .QDSP6_MX_FP_ACC_EXP = 9,
    .QDSP6_MX_CHANNELS = 32,
    .QDSP6_MX_ROWS = 64,
    .QDSP6_MX_COLS = 32,
    .QDSP6_MX_FP_ROWS = 32,
    .QDSP6_MX_FP_COLS = 32,
    .QDSP6_MX_CVT_MPY_SZ = 10,
    .QDSP6_MX_SUB_COLS = 1,
    .QDSP6_MX_ACCUM_WIDTH = 32,
    .QDSP6_MX_CVT_WIDTH = 12,
    .QDSP6_MX_FP_ACC_NORM = 3,
    .QDSP6_DMA_EXTENDED_VA_PRESENT = 0,
    .QDSP6_MX_PARALLEL_GRPS = 8,
    .QDSP6_VX_PRESENT = 1,
    .QDSP6_VX_CONTEXTS = 4,
};

struct ProcessorState ProcessorStateV68 = {
    .features = &rev_features_v68,
    .options = &options_struct_v68,
    .arch_proc_options = &arch_proc_opt_v68,
    .runnable_threads_max = 4,
    .thread_system_mask = 0xf,
    .shared_extptr = 0,
};


/* Board init.  */
static struct hexagon_board_boot_info hexagon_binfo;
static int ELF_FLAG_ARCH_MASK = 0x0ff;

static GString *get_exe_dir(GString *exe_dir) {
#ifdef __linux__
    char exe_name[1024];
    ssize_t exe_length;

    exe_length = readlink("/proc/self/exe", exe_name, sizeof(exe_name));
    if (exe_length == -1)
    {
        return NULL;
    }
    exe_name[sizeof(exe_name)-1] = '\0';
    char *exe_dir_ = dirname(exe_name);
    return g_string_assign(exe_dir, exe_dir_);
#else
#error "No host implementation for get_exe_dir() provided"
#endif
}


static void hexagon_load_kernel(CPUHexagonState *env)

{
    uint64_t pentry;
    long kernel_size;

    kernel_size =
        load_elf(hexagon_binfo.kernel_filename, NULL, NULL, NULL, &pentry, NULL,
                 NULL, &hexagon_binfo.kernel_elf_flags, 0, EM_HEXAGON, 0, 0);

    if (kernel_size <= 0) {
        error_report("no kernel file '%s'",
            hexagon_binfo.kernel_filename);
        exit(1);
    }

    GString *exe_dir_str = g_string_new("");
    env->exe_arch = (hexagon_binfo.kernel_elf_flags & ELF_FLAG_ARCH_MASK);
    GString *lib_search_dir = g_string_new("");

    exe_dir_str = get_exe_dir(exe_dir_str);
    gchar *exe_dir = g_string_free(exe_dir_str, false);

    /* $exe_dir/../target/hexagon/lib/v$exe_elf_machine_flags/G0/pic/ */
    g_string_printf(lib_search_dir,
        "%s/../target/hexagon/lib/v%x/G0/pic", exe_dir,
        env->exe_arch);

    env->lib_search_dir = g_string_free(lib_search_dir, false);
    g_free(exe_dir);

    env->gpr[HEX_REG_PC] = pentry;
}

static void hexagon_common_init(MachineState *machine, Rev_t rev)
{
    DeviceState *dev;
    int i;

    machine->smp.cpus = THREADS_MAX;
    memset(&hexagon_binfo, 0, sizeof(hexagon_binfo));
    if (machine->kernel_filename) {
        hexagon_binfo.ram_size = machine->ram_size;
        hexagon_binfo.kernel_filename = machine->kernel_filename;
    }

    HexagonCPU *cpu_0 = NULL;
    CPUHexagonState *env_0 = NULL;
    for (i = 0; i < machine->smp.cpus; i++) {
        HexagonCPU *cpu = HEXAGON_CPU(cpu_create(machine->cpu_type));
        CPUHexagonState *env = &cpu->env;

        HEX_DEBUG_LOG("%s: first cpu at 0x%p, env %p\n",
                __FUNCTION__, cpu, env);

        if (i == 0) {
            cpu_0 = cpu;
            env_0 = env;

            GString *argv = g_string_new(machine->kernel_filename);
            g_string_append(argv, " ");
            g_string_append(argv, machine->kernel_cmdline);
            env->cmdline = g_string_free(argv, false);
            env->dir_list = NULL;

            env->g_sreg[HEX_SREG_EVB] = 0x0;
            env->g_sreg[HEX_SREG_CFGBASE] =
                HEXAGON_CFG_ADDR_BASE(cfgExtensions->cfgbase);
            env->g_sreg[HEX_SREG_REV] = rev;
            env->g_sreg[HEX_SREG_MODECTL] = 0x1;
        }

    }

    HexagonCPU *cpu = cpu_0;

    machine->enable_graphics = 0;

    MemoryRegion *address_space = get_system_memory();

    cfgTable->raw[HEXAGON_CFGSPACE_ENTRIES-1] = 0x12341234;
    MemoryRegion *config_table_rom = g_new(MemoryRegion, 1);
    memory_region_init_rom(config_table_rom, NULL, "config_table.rom",
                           sizeof(*cfgTable), &error_fatal);
    memory_region_add_subregion(address_space, cfgExtensions->cfgbase,
                                config_table_rom);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "lpddr4.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    MemoryRegion *vtcm = g_new(MemoryRegion, 1);
    memory_region_init_ram(vtcm, NULL, "vtcm.ram", cfgTable->vtcm_size_kb * 1024,
        &error_fatal);
    memory_region_add_subregion(address_space, cfgTable->vtcm_base, vtcm);

    /* Test region for cpz addresses above 32-bits */
    MemoryRegion *cpz = g_new(MemoryRegion, 1);
    memory_region_init_ram(cpz, NULL, "cpz.ram", 0x10000000, &error_fatal);
    memory_region_add_subregion(address_space, 0x910000000, cpz);

    /* Skip if the core doesn't allocate space for TCM */
    if (cfgExtensions->l2tcm_size) {
        MemoryRegion *tcm = g_new(MemoryRegion, 1);
        memory_region_init_ram(tcm, NULL, "tcm.ram", cfgExtensions->l2tcm_size,
            &error_fatal);
        memory_region_add_subregion(address_space, cfgTable->l2tcm_base, tcm);
    }

    dev = sysbus_create_varargs("l2vic", cfgExtensions->l2vic_base,
                                             /* IRQ#, Evnt#,CauseCode */
        qdev_get_gpio_in(DEVICE(cpu), 0), /* IRQ 0, 16, 0xc0 */
        qdev_get_gpio_in(DEVICE(cpu), 1), /* IRQ 1, 17, 0xc1 */
        qdev_get_gpio_in(DEVICE(cpu), 2), /* IRQ 2, 18, 0xc2  VIC0 interface */
        qdev_get_gpio_in(DEVICE(cpu), 3), /* IRQ 3, 19, 0xc3  VIC1 interface */
        qdev_get_gpio_in(DEVICE(cpu), 4), /* IRQ 4, 20, 0xc4  VIC2 interface */
        qdev_get_gpio_in(DEVICE(cpu), 5), /* IRQ 5, 21, 0xc5  VIC3 interface */
        qdev_get_gpio_in(DEVICE(cpu), 6), /* IRQ 6, 22, 0xc6 */
        qdev_get_gpio_in(DEVICE(cpu), 7), /* IRQ 7, 23, 0xc7 */
        NULL);

    sysbus_create_varargs("fastl2vic", cfgTable->fastl2vic_base, NULL);

    /* This is tightly with the IRQ selected must match the value below
     * or the interrupts will not be seen
     */
    sysbus_create_varargs("qutimer", 0xfab20000,
                          NULL);
    unsigned QTMR0_IRQ = syscfg_is_linux ? 2 : 3;
    sysbus_create_varargs("hextimer", cfgExtensions->qtmr_rg0,
                          qdev_get_gpio_in(dev, QTMR0_IRQ), NULL);
    sysbus_create_varargs("hextimer", cfgExtensions->qtmr_rg1,
                          qdev_get_gpio_in(dev, 4), NULL);

    hexagon_config_table *config_table = cfgTable;

    config_table->l2tcm_base = HEXAGON_CFG_ADDR_BASE(cfgTable->l2tcm_base);
    config_table->subsystem_base = HEXAGON_CFG_ADDR_BASE(cfgExtensions->csr_base);
    config_table->vtcm_base = HEXAGON_CFG_ADDR_BASE(cfgTable->vtcm_base);
    config_table->l2cfg_base = HEXAGON_CFG_ADDR_BASE(cfgTable->l2cfg_base);
    config_table->fastl2vic_base =
                             HEXAGON_CFG_ADDR_BASE(cfgTable->fastl2vic_base);

    rom_add_blob_fixed_as("config_table.rom", config_table,
        sizeof(*config_table), cfgExtensions->cfgbase, &address_space_memory);

    if (machine->kernel_filename) {
        hexagon_load_kernel(env_0);
    }
}

void hexagon_read_timer(uint32_t *low, uint32_t *high)
{
    const hwaddr low_addr  = cfgExtensions->qtmr_rg0 + QTMR_CNTPCT_LO;
    const hwaddr high_addr = cfgExtensions->qtmr_rg0 + QTMR_CNTPCT_HI;

    cpu_physical_memory_read(low_addr, low, sizeof(*low));
    cpu_physical_memory_read(high_addr, high, sizeof(*high));
}

uint32_t hexagon_find_last_irq(uint32_t vid)
{
    int offset = (vid ==  HEX_SREG_VID) ? L2VIC_VID_0 : L2VIC_VID_1;

    const hwaddr pend_mem = cfgExtensions->l2vic_base + offset;
    uint32_t irq;
    cpu_physical_memory_read(pend_mem, &irq, sizeof(irq));
    return irq;
}

void hexagon_set_vid(uint32_t offset, int val) {
    assert ((offset == L2VIC_VID_0) || (offset == L2VIC_VID_1));
    const hwaddr pend_mem = cfgExtensions->l2vic_base + offset;
    cpu_physical_memory_write(pend_mem, &val, sizeof(val));
}

void hexagon_clear_last_irq(uint32_t offset) {
    /* currently only l2vic is the only attached it uses vid0, remove
     * the assert below if anther is added */
    hexagon_set_vid(offset, L2VIC_NO_PENDING);
}

void hexagon_clear_l2vic_pending(int int_num)
{
    uint32_t val;
    target_ulong vidval;
    uint32_t slice = (int_num / 32) * 4;
    val = 1 << (int_num % 32);
    const hwaddr addr = cfgExtensions->l2vic_base + L2VIC_INT_PENDINGn;
    cpu_physical_memory_read(addr + slice, &vidval, sizeof(vidval));
    vidval &= ~val;
    cpu_physical_memory_write(addr + slice, &vidval, sizeof(vidval));
}


int hexagon_find_l2vic_pending(void)
{
    int intnum = 0;
    const hwaddr pend = cfgExtensions->l2vic_base + L2VIC_INT_STATUSn;
    uint64_t pending[L2VIC_INTERRUPT_MAX / (sizeof(uint64_t) * CHAR_BIT)];
    cpu_physical_memory_read(pend, pending, sizeof(pending));

    const hwaddr stat = cfgExtensions->l2vic_base + L2VIC_INT_STATUSn;
    uint64_t status[L2VIC_INTERRUPT_MAX / (sizeof(uint64_t) * CHAR_BIT)];
    cpu_physical_memory_read(stat, status, sizeof(status));

    intnum = find_next_bit(pending, L2VIC_INTERRUPT_MAX, intnum);
    while (intnum < L2VIC_INTERRUPT_MAX) {
        /* Pending is set but status isn't the interrupt is pending */
        if (!test_bit(intnum, status)) {
            break;
        }
        intnum = find_next_bit(pending, L2VIC_INTERRUPT_MAX, intnum+1);
    }
    return (intnum < L2VIC_INTERRUPT_MAX) ? intnum : L2VIC_NO_PENDING;
}


#define TYPE_FASTL2VIC "fastl2vic"
#define FASTL2VIC(obj) OBJECT_CHECK(FastL2VICState, (obj), TYPE_FASTL2VIC)
#define FASTL2VIC_ENABLE 0x0
#define FASTL2VIC_DISABLE 0x1
#define FASTL2VIC_INT 0x2

typedef struct FastL2VICState {
    SysBusDevice parent_obj;
    MemoryRegion iomem;
} FastL2VICState;

#define L2VICA(s, n) \
    (s[(n) >> 2])

static void fastl2vic_write(void *opaque, hwaddr offset,
                        uint64_t val, unsigned size) {

    if (offset == 0) {

        uint32_t cmd = (val >> 16) & 0x3;
        uint32_t irq = val & 0x3ff;
        uint32_t slice = (irq / 32) * 4;
        val = 1 << (irq % 32);

        if (cmd == FASTL2VIC_ENABLE) {
            const hwaddr addr = cfgExtensions->l2vic_base
                                + L2VIC_INT_ENABLE_SETn;
            cpu_physical_memory_write(addr + slice, &val, size);
        } else if (cmd == FASTL2VIC_DISABLE) {
            const hwaddr addr = cfgExtensions->l2vic_base
                                + L2VIC_INT_ENABLE_CLEARn;
            cpu_physical_memory_write(addr + slice, &val, size);
        } else if (cmd == FASTL2VIC_INT) {
            const hwaddr addr = cfgExtensions->l2vic_base
                                + L2VIC_SOFT_INTn;
            cpu_physical_memory_write(addr + slice, &val, size);
        }
        /* RESERVED */
        else if (cmd == 0x3) {
            g_assert(0);
        }
        return;
    }
    qemu_log_mask(LOG_GUEST_ERROR, "%s: invalid write offset 0x%08x\n",
        __func__, (unsigned int)offset);
    /* Address zero is the only legal spot to write */
    g_assert(0);
}

static const MemoryRegionOps fastl2vic_ops = {
    .write = fastl2vic_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .valid.unaligned = false,
};

static void fastl2vic_init(Object *obj)
{
    FastL2VICState *s = FASTL2VIC(obj);
    SysBusDevice *sbd = SYS_BUS_DEVICE(obj);

    memory_region_init_io(&s->iomem, obj, &fastl2vic_ops, s,
                          "fastl2vic", 0x10000);
    sysbus_init_mmio(sbd, &s->iomem);
}

static const VMStateDescription vmstate_fastl2vic = {
    .name = "fastl2vic",
    .version_id = 1,
    .minimum_version_id = 1,
};

static void fastl2vic_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->vmsd = &vmstate_fastl2vic;
}

static const TypeInfo fastl2vic_info = {
    .name          = "fastl2vic",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(FastL2VICState),
    .instance_init = fastl2vic_init,
    .class_init    = fastl2vic_class_init,
};

static void fastl2vic_register_types(void)
{
    type_register_static(&fastl2vic_info);
}

type_init(fastl2vic_register_types)

/* ----------------------------------------------------------------- */
/* Core-specific configuration settings are defined below this line. */
/* Config table values defined in include/hw/hexagon/machine_configs */
/* ----------------------------------------------------------------- */

static void v66g_1024_config_init(MachineState *machine)
{
    cfgTable = &v66g_1024_cfgtable;
    cfgExtensions = &v66g_1024_extensions;
    hexagon_common_init(machine, v66_rev);
}

static void v66g_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V66G_1024";
    mc->init = v66g_1024_config_init;
    mc->is_default = 0;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->default_cpus = mc->max_cpus = 4;
    mc->default_ram_size = 4 * GiB;
}


static void v66g_1024_linux_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v66g_1024_config_init(machine);
}

static void v66g_linux_init(ObjectClass *oc, void *data)
{
    v66g_1024_init(oc, data);

    MachineClass *mc = MACHINE_CLASS(oc);
    mc->init = v66g_1024_linux_init;
    mc->desc = "Hexagon Linux V66G_1024";
}

static void v68n_1024_config_init(MachineState *machine)

{
    cfgTable = &v68n_1024_cfgtable;
    cfgExtensions = &v68n_1024_extensions;
    hexagon_common_init(machine, v68_rev);
}

static void v68n_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V68N_1024";
    mc->init = v68n_1024_config_init;
    mc->is_default = 1;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->default_cpus = mc->max_cpus = 6;
    mc->default_ram_size = 4 * GiB;
}

static void v68g_1024_h2_init(MachineState *machine)
{
    syscfg_is_linux = true;
    v68n_1024_config_init(machine);
}

static void v68g_h2_init(ObjectClass *oc, void *data)
{
    v66g_1024_init(oc, data);

    MachineClass *mc = MACHINE_CLASS(oc);
    mc->init = v68g_1024_h2_init;
    mc->desc = "Hexagon H2 V68G_1024";

   /* TODO: Remove but, 4 is better tested */
    mc->default_cpus = mc->max_cpus = 4;
}


static void v69na_1024_config_init(MachineState *machine)
{
    cfgTable = &v69na_1024_cfgtable;
    cfgExtensions = &v69na_1024_extensions;
    hexagon_common_init(machine, v69_rev);
}

static void v69na_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V69NA_1024";
    mc->init = v69na_1024_config_init;
    mc->is_default = 0;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->default_cpus = mc->max_cpus = 6;
    mc->default_ram_size = 4 * GiB;
}

static void v73na_1024_config_init(MachineState *machine)
{
    cfgTable = &v73na_1024_cfgtable;
    cfgExtensions = &v73na_1024_extensions;
    hexagon_common_init(machine, v73_rev);
}

static void v73na_1024_linux_config_init(MachineState *machine)
{
    syscfg_is_linux = true;

    v73na_1024_config_init(machine);
}

static void v73na_1024_linux_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon Linux V73NA_1024";
    mc->init = v73na_1024_linux_config_init;
    mc->is_default = 0;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->default_cpus = mc->max_cpus = 6;
    mc->default_ram_size = 4 * GiB;
}

static void v73na_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V73NA_1024";
    mc->init = v73na_1024_config_init;
    mc->is_default = 0;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->default_cpus = mc->max_cpus = 6;
    mc->default_ram_size = 4 * GiB;
}

static const TypeInfo hexagon_machine_types[] = {
    {
        .name = MACHINE_TYPE_NAME("V66G_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v66g_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V68N_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v68n_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V69NA_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v69na_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V73NA_1024"),
        .parent = TYPE_MACHINE,
        .class_init = v73na_1024_init,
    }, {
        .name = MACHINE_TYPE_NAME("V73_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v73na_1024_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V66_Linux"),
        .parent = TYPE_MACHINE,
        .class_init = v66g_linux_init,
    }, {
        .name = MACHINE_TYPE_NAME("V68_H2"),
        .parent = TYPE_MACHINE,
        .class_init = v68g_h2_init,
    },
};

DEFINE_TYPES(hexagon_machine_types)
