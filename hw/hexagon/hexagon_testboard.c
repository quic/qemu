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
#include "qapi/error.h"
#include "cpu.h"
#include "net/net.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "exec/address-spaces.h"
#include "elf.h"
#include "hw/hexagon/hexagon.h"
#include "qemu/error-report.h"

static const struct MemmapEntry {
    hwaddr base;
    hwaddr size;
} hexagon_board_memmap[] = {
    [HEXAGON_LPDDR]        =    {  0x00000000,       0x0 },
    [HEXAGON_IOMEM]        =    {  0x1f000000,   0x10000 },
    [HEXAGON_CONFIG_TABLE] =    {  0xde000000,     0x400 },
    [HEXAGON_VTCM]         =    {  0xd8000000,  0x400000 },
    [HEXAGON_L2CFG]        =    {  0xd81a0000,       0x0 },
    [HEXAGON_TCM]          =    {  0xd8400000,  0x100000 },
};

/* Board init.  */
static struct hexagon_board_boot_info hexagon_binfo;

static void hexagon_load_kernel(CPUHexagonState *env)

{
    uint64_t pentry;
    long kernel_size;

    kernel_size = load_elf(hexagon_binfo.kernel_filename,
        NULL,
        NULL,
        NULL,
        &pentry,
        NULL,
        NULL,
        NULL,
        0,
        EM_HEXAGON, 0, 0);

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

    HexagonCPU *cpu = HEXAGON_CPU(cpu_create(machine->cpu_type));
    CPUHexagonState *env = &cpu->env;

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


//    MemoryRegion *iomem = g_new(MemoryRegion, 1);
//    memory_region_init_io(iomem, NULL, &hexagon_qemu_ops,
//                          NULL, "hexagon-qemu", 0x10000);
//    memory_region_add_subregion(address_space, 0x1f000000, iomem);

    struct hexagon_config_table config_table;
    memset(&config_table, 0x0, sizeof(config_table));

    assert(machine->smp.cores == 1);
    config_table.core_id = 0;
    config_table.core_count = machine->smp.cores;
    config_table.thread_enable_mask = ~(machine->smp.threads);
    config_table.coproc_present = HEXAGON_COPROC_HVX;
    config_table.ext_contexts = HEXAGON_DEFAULT_HVX_CONTEXTS;
    config_table.l2tcm_base = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_TCM].base);
    config_table.vtcm_base = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_VTCM].base);
    config_table.vtcm_size_kb = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_VTCM].size / 1024);
    config_table.l2cfg_base = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_L2CFG].base);
    config_table.l2tag_size = HEXAGON_DEFAULT_L2_TAG_SIZE;
    config_table.jtlb_size_entries = HEXAGON_DEFAULT_TLB_ENTRIES;
    config_table.v2x_mode = HEXAGON_HVX_VEC_LEN_V2X_1 | HEXAGON_HVX_VEC_LEN_V2X_2;
    config_table.hvx_vec_log_length = HEXAGON_HVX_DEFAULT_VEC_LOG_LEN_BYTES;

    rom_add_blob_fixed_as("config_table.rom", &config_table, sizeof(config_table),
        memmap[HEXAGON_CONFIG_TABLE].base, &address_space_memory);

    hexagon_binfo.ram_size = machine->ram_size;
    hexagon_binfo.kernel_filename = machine->kernel_filename;

#if 0
    printf(
        "config_table.coproc_present @0x%lx\n"
        "config_table.ext_contexts @0x%lx\n"
        "config_table.l2tcm_base @0x%lx\n"
        "config_table.vtcm_base @0x%lx\n"
        "config_table.v2x_mode @0x%lx\n"
        "config_table.hvx_vec_log_length @0x%lx\n"
        "config_table.axi3_lowaddr @0x%lx\n"
            ,
        offsetof(struct hexagon_config_table,coproc_present),
        offsetof(struct hexagon_config_table,ext_contexts),
        offsetof(struct hexagon_config_table,l2tcm_base),
        offsetof(struct hexagon_config_table,vtcm_base),
        offsetof(struct hexagon_config_table,v2x_mode),
        offsetof(struct hexagon_config_table,hvx_vec_log_length),
        offsetof(struct hexagon_config_table,axi3_lowaddr)
        );
#endif

    if (machine->kernel_filename) {
        hexagon_load_kernel(env);
    } else {
        env->gpr[HEX_REG_PC] = 0x0; /* 0 or 0x20;*/
    }

    env->sreg[HEX_SREG_EVB] = 0x0;
    env->sreg[HEX_SREG_CFGBASE] = HEXAGON_CFG_ADDR_BASE(memmap[HEXAGON_CONFIG_TABLE].base);
    env->sreg[HEX_SREG_REV] = 0x86d8;
}

static void hexagonboard_init(MachineState *machine)

{
    printf("%s\n", __FUNCTION__);
    hexagon_testboard_init(machine, 845);
}

static void hex_machine_init(MachineClass *mc)

{
    printf("%s\n", __FUNCTION__);
    mc->desc = "a minimal Hexagon board";
    mc->init = hexagonboard_init;
    mc->is_default = 0;
    mc->default_cpu_type = HEXAGON_CPU_TYPE_NAME("v67");
}

DEFINE_MACHINE("hexagon_testboard", hex_machine_init)
