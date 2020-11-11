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
#include "internal.h"

hexagon_config_table cfgTable;
hexagon_config_extensions cfgExtensions;

const rev_features_t rev_features_v68 = {
};

// mgl fix
const options_struct options_struct_v68 = {
    .l2tcm_base  = 0,  /* FIXME - Should be l2tcm_base ?? */
    .hmx_mac_fxp_callback = (void *)0,
    .hmx_mac_flt_callback = (void *)0,
};

const arch_proc_opt_t arch_proc_opt_v68 = {
    .hmx_output_depth = HMX_OUTPUT_DEPTH,
    .hmx_block_size = HMX_BLOCK_SIZE,
    .hmx_v1 = HMX_V1,
    .dmadebug_verbosity = 0,
    .hmx_spatial_size = HMX_SPATIAL_SIZE,
    .hmx_channel_size = HMX_CHANNEL_SIZE,
    .hmx_addr_mask = HMX_ADDR_MASK,
    .hmx_block_bit = HMX_BLOCK_BIT,
    .hmxdebuglvl = HMXDEBUGLVL,
    .hmxaccpreloadfile = NULL,
    .hmxdebugfile = NULL,
    .pmu_enable = 0,
    .vtcm_size = VTCM_SIZE,
    .vtcm_offset = VTCM_OFFSET,
    .dmadebugfile = NULL,
    .QDSP6_MX_FP_ACC_INT   = 18,
    .QDSP6_MX_FP_ACC_FRAC  = 46,
    .QDSP6_MX_CHANNELS     = 32,
    .QDSP6_MX_FP_RATE      = 2,
    .QDSP6_MX_RATE         = 4,
    .QDSP6_DMA_PRESENT     = 1,
    .QDSP6_MX_CVT_MPY_SZ   = 10,
    .QDSP6_MX_FP_PRESENT   = 1,
    .QDSP6_MX_FP_ROWS      = 32,
    .QDSP6_MX_ROWS         = 64,
    .QDSP6_MX_COLS         = 32,
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
    env->gpr[HEX_REG_PC] = pentry;
}

extern dma_t *dma_adapter_init(processor_t *proc, int dmanum);
static void hexagon_common_init(MachineState *machine)
{
    machine->enable_graphics = 0;

    DeviceState *dev;

    HexagonCPU *cpu = HEXAGON_CPU(cpu_create(machine->cpu_type));
    CPUHexagonState *env = &cpu->env;
    HEX_DEBUG_LOG("%s: first cpu at 0x%p, env %p\n",
        __FUNCTION__, cpu, env);

    env->processor_ptr = (processor_t *)&ProcessorStateV68;
    env->processor_ptr->thread[env->threadId = 0] = env;
    //env->processor_ptr = g_malloc(sizeof(Processor));
//    env->processor_ptr->runnable_threads_max = 4;
//    env->processor_ptr->thread_system_mask = 0xf;
    env->g_sreg = g_malloc0(sizeof(target_ulong) * NUM_SREGS);
    hex_mmu_init(env);

    MemoryRegion *address_space = get_system_memory();

    MemoryRegion *config_table_rom = g_new(MemoryRegion, 1);
    memory_region_init_rom(config_table_rom, NULL, "config_table.rom",
                           sizeof(cfgTable), &error_fatal);
    memory_region_add_subregion(address_space, cfgExtensions.cfgbase,
                                config_table_rom);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "lpddr4.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    MemoryRegion *vtcm = g_new(MemoryRegion, 1);
    memory_region_init_ram(vtcm, NULL, "vtcm.ram", cfgTable.vtcm_size_kb * 1024,
        &error_fatal);
    memory_region_add_subregion(address_space, cfgTable.vtcm_base, vtcm);

    /* Test region for cpz addresses above 32-bits */
    MemoryRegion *cpz = g_new(MemoryRegion, 1);
    memory_region_init_ram(cpz, NULL, "cpz.ram", 0x10000000, &error_fatal);
    memory_region_add_subregion(address_space, 0x910000000, cpz);

    /* Skip if the core doesn't allocate space for TCM */
    if (cfgExtensions.l2tcm_size) {
        MemoryRegion *tcm = g_new(MemoryRegion, 1);
        memory_region_init_ram(tcm, NULL, "tcm.ram", cfgExtensions.l2tcm_size,
            &error_fatal);
        memory_region_add_subregion(address_space, cfgTable.l2tcm_base, tcm);
    }

    dev = sysbus_create_varargs("l2vic", cfgExtensions.l2vic_base,
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

    sysbus_create_varargs("fastl2vic", cfgTable.fastl2vic_base, NULL);

    /* This is tightly with the IRQ selected must match the value below
     * or the interrupts will not be seen
     */
    sysbus_create_varargs("qutimer", 0xfab20000,
                          NULL);
    sysbus_create_varargs("hextimer", cfgExtensions.qtmr_rg0,
                          qdev_get_gpio_in(dev, 3), NULL);
    sysbus_create_varargs("hextimer", cfgExtensions.qtmr_rg1,
                          qdev_get_gpio_in(dev, 4), NULL);

    hexagon_config_table config_table = cfgTable;

    machine->smp.max_cpus = THREADS_MAX;

    config_table.l2tcm_base = HEXAGON_CFG_ADDR_BASE(cfgTable.l2tcm_base);
    config_table.subsystem_base = HEXAGON_CFG_ADDR_BASE(cfgExtensions.csr_base);
    config_table.vtcm_base = HEXAGON_CFG_ADDR_BASE(cfgTable.vtcm_base);
    config_table.l2cfg_base = HEXAGON_CFG_ADDR_BASE(cfgTable.l2cfg_base);
    config_table.fastl2vic_base =
                             HEXAGON_CFG_ADDR_BASE(cfgTable.fastl2vic_base);

    rom_add_blob_fixed_as("config_table.rom", &config_table,
        sizeof(config_table), cfgExtensions.cfgbase, &address_space_memory);

    hexagon_binfo.ram_size = machine->ram_size;
    hexagon_binfo.kernel_filename = machine->kernel_filename;

    GString *argv = g_string_new(machine->kernel_filename);
    g_string_append(argv, " ");
    g_string_append(argv, machine->kernel_cmdline);
    env->cmdline = g_string_free(argv, false);

    if (machine->kernel_filename) {
        hexagon_load_kernel(env);
    }

    env->g_sreg[HEX_SREG_EVB] = 0x0;
    env->g_sreg[HEX_SREG_CFGBASE] =
                                 HEXAGON_CFG_ADDR_BASE(cfgExtensions.cfgbase);
    env->g_sreg[HEX_SREG_REV] = 0x8d68;
    env->g_sreg[HEX_SREG_MODECTL] = 0x1;

    env->processor_ptr->dma[env->threadId] =
                          dma_adapter_init(env->processor_ptr, env->threadId);
    env->processor_ptr->shared_extptr = hmx_ext_palloc(env->processor_ptr, 0);
}

void hexagon_read_timer(uint32_t *low, uint32_t *high)
{
    const hwaddr low_addr  = cfgExtensions.qtmr_rg0 + QTMR_CNTPCT_LO;
    const hwaddr high_addr = cfgExtensions.qtmr_rg0 + QTMR_CNTPCT_HI;

    cpu_physical_memory_read(low_addr, low, sizeof(*low));
    cpu_physical_memory_read(high_addr, high, sizeof(*high));
}

void hexagon_set_l2vic_pending(uint32_t int_num)
{
    uint64_t val;
    uint32_t slice = (int_num / 32) * 4;
    val = 1 << (int_num % 32);
    target_ulong vidval, origval;

    const hwaddr addr = cfgExtensions.l2vic_base + L2VIC_INT_PENDINGn;
    cpu_physical_memory_read(addr + slice, &vidval, 4);
    origval = vidval;
    vidval |= val;
    if (origval == vidval) {
        HEX_DEBUG_LOG("%s, slice = %d, int_num = %d\n",
                      __func__, slice / 4, int_num);
        HEX_DEBUG_LOG("val = 0x%lx, vidval = 0x%x\n", val, vidval);
        HEX_DEBUG_LOG("Double pend of int#:%ld\n", find_first_bit(&val, 32));
    }
    cpu_physical_memory_write(addr + slice, &vidval, 4);
}


void hexagon_clear_l2vic_pending(uint32_t int_num)
{
    uint32_t val;
    target_ulong vidval;
    uint32_t slice = (int_num / 32) * 4;
    val = 1 << (int_num % 32);
    const hwaddr addr = cfgExtensions.l2vic_base + L2VIC_INT_PENDINGn;
    cpu_physical_memory_read(addr + slice, &vidval, 4);
    vidval &= ~val;
    cpu_physical_memory_write(addr + slice, &vidval, 4);
}


uint32_t hexagon_find_l2vic_pending(void)
{
    const hwaddr pend = cfgExtensions.l2vic_base + L2VIC_INT_PENDINGn;
    uint64_t val;
    cpu_physical_memory_read(pend, &val, 8);
    uint32_t inum = find_first_bit(&val, 64);

    if (inum == 64) {
        cpu_physical_memory_read(pend + 8, &val, 8);
        inum = find_first_bit(&val, 64);
        if (inum == 64) {
            inum = 0;
        } else {
            inum += 64;
        }
    }
    /* Verify that stat matches which indicates a truly pending interrupt */
    if (inum != 64) {
        const hwaddr stat = cfgExtensions.l2vic_base + L2VIC_INT_STATUSn;
        cpu_physical_memory_read(stat, &val, 8);
        uint32_t stat_num = find_first_bit(&val, 64);
        if (stat_num == 64) {
            cpu_physical_memory_read(stat + 8, &val, 8);
            stat_num = find_first_bit(&val, 64);
            if (stat_num == 64) {
                inum = 0;
            } else {
                stat_num += 64;
            }
        }
        if (inum != stat_num) {
            HEX_DEBUG_LOG("inum = %d\n", inum);
            HEX_DEBUG_LOG("stat_num = %d\n", stat_num);
           /*
            * If stat and pending don't match it means,
            * hexagon_clear_l2vic_pending has kicked off the pending
            * interrupt but it has not finished.  In this case just
            * return 0?
            */
            return 0;
        }
    }
    return (inum == 64) ? 0 : inum;
}


#define TYPE_FASTL2VIC "fastl2vic"
#define FASTL2VIC(obj) OBJECT_CHECK(FastL2VICState, (obj), TYPE_FASTL2VIC)

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

        if (cmd == 0x0) {
            const hwaddr addr = cfgExtensions.l2vic_base
                                + L2VIC_INT_ENABLE_SETn;
            cpu_physical_memory_write(addr + slice, &val, size);
            return;
        } else if (cmd == 0x1) {
            const hwaddr addr = cfgExtensions.l2vic_base
                                + L2VIC_INT_ENABLE_CLEARn;
            return cpu_physical_memory_write(addr + slice, &val, size);
        } else if (cmd == 2) {
            g_assert(0);
        }
        /* RESERVED */
        else if ((val & 0xff0000) == 0x110000) {
            g_assert(0);
        }
    }
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
    memcpy(&cfgTable, v66g_1024_cfgtable, sizeof(cfgTable));
    memcpy(&cfgExtensions, v66g_1024_extensions, sizeof(cfgExtensions));
    hexagon_common_init(machine);
}

static void v66g_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V66G_1024";
    mc->init = v66g_1024_config_init;
    mc->is_default = 0;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->max_cpus = 4;
    mc->default_ram_size = 4 * GiB;
}


static void v68n_1024_config_init(MachineState *machine)

{
    memcpy(&cfgTable, v68n_1024_cfgtable, sizeof(cfgTable));
    memcpy(&cfgExtensions, v68n_1024_extensions, sizeof(cfgExtensions));
    hexagon_common_init(machine);
}

static void v68n_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V68N_1024";
    mc->init = v68n_1024_config_init;
    mc->is_default = 1;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->max_cpus = 6;
    mc->default_ram_size = 4 * GiB;
}


static void v69na_1024_config_init(MachineState *machine)
{
    memcpy(&cfgTable, v69na_1024_cfgtable, sizeof(cfgTable));
    memcpy(&cfgExtensions, v69na_1024_extensions, sizeof(cfgExtensions));
    hexagon_common_init(machine);
}

static void v69na_1024_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "Hexagon V69NA_1024";
    mc->init = v69na_1024_config_init;
    mc->is_default = 0;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->max_cpus = 6;
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
    },
};

DEFINE_TYPES(hexagon_machine_types)
