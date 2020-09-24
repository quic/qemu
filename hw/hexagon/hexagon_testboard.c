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
#include "exec/address-spaces.h"
#include "hw/boards.h"
#include "hw/hexagon/hexagon.h"
#include "hw/hexagon/qtimer.h"
#include "hw/intc/l2vic.h"
#include "hw/loader.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "qemu/log.h"
#include "elf.h"
#include "cpu.h"
#include "hex_mmu.h"
#include "include/migration/cpu.h"
#include "include/sysemu/sysemu.h"
#include "internal.h"

static const struct MemmapEntry {
    hwaddr base;
    hwaddr size;
} hexagon_board_memmap[] = {
        [HEXAGON_LPDDR] = { 0x00000000, 0x0 },
        [HEXAGON_IOMEM] = { 0x1f000000, 0x10000 },
        [HEXAGON_CONFIG_TABLE] = { 0xde000000, 0x400 },
        [HEXAGON_VTCM] = { 0xd8000000, 0x400000 },
        [HEXAGON_L2CFG] = { 0xd81a0000, 0x0 },
        [HEXAGON_FASTL2VIC] = { 0xd81e0000, 0x10000 },
        [HEXAGON_TCM] = { 0xd8400000, 0x100000 },
        [HEXAGON_CSR1] = { 0xfab00000, 0x0 },
        [HEXAGON_L2VIC] = { 0xfab10000, 0x1000 },
        [HEXAGON_QTMR_RG0] = { 0xfab21000, 0x1000 },
        [HEXAGON_QTMR_RG1] = { 0xfab22000, 0x1000 },
        [HEXAGON_CSR2] = { 0xfab40000, 0x0 },
        [HEXAGON_QTMR2] = { 0xfab60000, 0x0 },
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

static void hexagon_testboard_init(MachineState *machine, int board_id)
{
    const struct MemmapEntry *memmap = hexagon_board_memmap;
    printf("cpu_type = %s\nfilename = %s\nram_size = %lu/0x%lx\n",
        machine->cpu_type,
        machine->kernel_filename,
        machine->ram_size, machine->ram_size);

    DeviceState *dev;

    HexagonCPU *cpu = HEXAGON_CPU(cpu_create(machine->cpu_type));
    CPUHexagonState *env = &cpu->env;
    HEX_DEBUG_LOG("%s: first cpu at 0x%p, env %p\n",
        __FUNCTION__, cpu, env);

    env->processor_ptr = g_malloc(sizeof(Processor));
    env->processor_ptr->runnable_threads_max = 4;
    env->processor_ptr->thread_system_mask = 0xf;
    env->g_sreg = g_malloc0(sizeof(target_ulong) * NUM_SREGS);
    hex_mmu_init(env);

    MemoryRegion *address_space = get_system_memory();

    MemoryRegion *config_table_rom = g_new(MemoryRegion, 1);
    memory_region_init_rom(config_table_rom, NULL, "config_table.rom", memmap[HEXAGON_CONFIG_TABLE].size,
        &error_fatal);
    memory_region_add_subregion(address_space, memmap[HEXAGON_CONFIG_TABLE].base, config_table_rom);

    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "lpddr4.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    MemoryRegion *vtcm = g_new(MemoryRegion, 1);
    memory_region_init_ram(vtcm, NULL, "vtcm.ram", memmap[HEXAGON_VTCM].size,
        &error_fatal);
    memory_region_add_subregion(address_space, memmap[HEXAGON_VTCM].base, vtcm);

    MemoryRegion *tcm = g_new(MemoryRegion, 1);
    memory_region_init_ram(tcm, NULL, "tcm.ram", memmap[HEXAGON_TCM].size,
        &error_fatal);
    memory_region_add_subregion(address_space, memmap[HEXAGON_TCM].base, tcm);

    dev = sysbus_create_varargs("l2vic", memmap[HEXAGON_L2VIC].base,
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

    sysbus_create_varargs("fastl2vic", memmap[HEXAGON_FASTL2VIC].base, NULL);

    /* This is tightly with the IRQ selected must match the value below
     * or the interrupts will not be seen
     */
    sysbus_create_varargs("qutimer", 0xfab20000,
                          NULL);
    sysbus_create_varargs("hextimer", memmap[HEXAGON_QTMR_RG0].base,
                          qdev_get_gpio_in(dev, 3), NULL);
    sysbus_create_varargs("hextimer", memmap[HEXAGON_QTMR_RG1].base,
                          qdev_get_gpio_in(dev, 4), NULL);

    struct hexagon_config_table config_table;
    memset(&config_table, 0x0, sizeof(config_table));

    machine->smp.max_cpus = THREADS_MAX;

    assert(machine->smp.cores == 1);
    config_table.core_id = 0;
    config_table.core_count = machine->smp.cores;
    config_table.thread_enable_mask =
        ((uint64_t) 1 << machine->smp.threads) - 1;
    config_table.coproc_present = HEXAGON_COPROC_HVX;
    config_table.ext_contexts = HEXAGON_DEFAULT_HVX_CONTEXTS;
    config_table.l2tcm_base = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_TCM].base);
    config_table.subsystem_base = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_CSR1].base);
    config_table.vtcm_base = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_VTCM].base);
    config_table.vtcm_size_kb = HEXAGON_CFG_ADDR_BASE(
        memmap[HEXAGON_VTCM].size / 1024);
    config_table.l2cfg_base = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_L2CFG].base);
    config_table.l2tag_size = HEXAGON_DEFAULT_L2_TAG_SIZE;
    config_table.jtlb_size_entries = HEXAGON_DEFAULT_TLB_ENTRIES;
    config_table.v2x_mode =
        HEXAGON_HVX_VEC_LEN_V2X_1 | HEXAGON_HVX_VEC_LEN_V2X_2;
    config_table.hvx_vec_log_length = HEXAGON_HVX_DEFAULT_VEC_LOG_LEN_BYTES;
    config_table.fastl2vic_base =
        HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_FASTL2VIC].base);
    config_table.l1d_size_kb = HEXAGON_DEFAULT_L1D_SIZE_KB;
    config_table.l1i_size_kb = HEXAGON_DEFAULT_L1I_SIZE_KB;
    config_table.l1d_write_policy = HEXAGON_DEFAULT_L1D_WRITE_POLICY;
    config_table.tiny_core = 0; /* Or '1' to signal support for audio ext? */
    config_table.l2line_size = HEXAGON_V67_L2_LINE_SIZE_BYTES;

    rom_add_blob_fixed_as("config_table.rom", &config_table,
        sizeof(config_table), memmap[HEXAGON_CONFIG_TABLE].base,
        &address_space_memory);

    hexagon_binfo.ram_size = machine->ram_size;
    hexagon_binfo.kernel_filename = machine->kernel_filename;

    env->cmdline = machine->kernel_cmdline;

    if (machine->kernel_filename) {
        hexagon_load_kernel(env);
    }

    env->g_sreg[HEX_SREG_EVB] = 0x0;
    env->g_sreg[HEX_SREG_CFGBASE] =
        HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_CONFIG_TABLE].base);
    env->g_sreg[HEX_SREG_REV] = 0x8d68; //0x86d8;
    env->g_sreg[HEX_SREG_MODECTL] = 0x1;
}

static void hexagonboard_init(MachineState *machine)
{
    hexagon_testboard_init(machine, 845);
}

static void hex_class_init(ObjectClass *oc, void *data)
{
    MachineClass *mc = MACHINE_CLASS(oc);

    mc->desc = "a minimal Hexagon board";
    mc->init = hexagonboard_init;
    mc->is_default = 0;
    mc->block_default_type = IF_SCSI;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
    mc->max_cpus = 4;
}

static const TypeInfo hexagon_testboard_type = {
        .name = MACHINE_TYPE_NAME("hexagon_testboard"),
        .parent = TYPE_MACHINE,
        .class_init = hex_class_init,
};

static void hexagon_machine_init(void)
{
    type_register_static(&hexagon_testboard_type);
}

void hexagon_read_timer(uint32_t *low, uint32_t *high)
{
    const struct MemmapEntry *memmap = hexagon_board_memmap;
    const hwaddr low_addr = memmap[HEXAGON_QTMR_RG0].base + QTMR_CNTPCT_LO;
    const hwaddr high_addr = memmap[HEXAGON_QTMR_RG0].base + QTMR_CNTPCT_HI;

    cpu_physical_memory_read(low_addr, low, sizeof(*low));
    cpu_physical_memory_read(high_addr, high, sizeof(*high));
}

type_init(hexagon_machine_init)


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
        const struct MemmapEntry *memmap = hexagon_board_memmap;

        uint32_t cmd = (val >> 16) & 0x3;
        uint32_t irq = val & 0x3ff;
        uint32_t slice = (irq / 8);
        val = 1 << (irq % 32);

        if (cmd == 0x0) {
            const hwaddr addr = memmap[HEXAGON_L2VIC].base +
                                L2VIC_INT_ENABLE_SETn;
            cpu_physical_memory_write(addr + slice, &val, size);
            return;
        } else if (cmd == 0x1) {
            const hwaddr addr = memmap[HEXAGON_L2VIC].base +
                                L2VIC_INT_ENABLE_CLEARn;
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
