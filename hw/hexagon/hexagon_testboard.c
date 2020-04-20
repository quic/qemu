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

/* Board init.  */
static struct hexagon_board_boot_info hexagon_binfo;

static void hexagon_load_kernel(CPUHexagonState *env)

{
    uint64_t pentry;
    long kernel_size;

    printf("%s\n", __FUNCTION__);
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
    printf("load_elf() : failed\n");
    if (kernel_size <= 0) {
        error_report("no kernel file '%s'",
            hexagon_binfo.kernel_filename);
        exit(1);
    }
    env->gpr[HEX_REG_PC] = pentry;
}

static void hexagon_testboard_init(MachineState *machine, int board_id)
{
    printf("%s\n", __FUNCTION__);
    HexagonCPU *cpu;
    CPUHexagonState *env;

    printf("cpu_type = %s\nfilename = %s\nram_size = %lu/0x%lx\n",
        machine->cpu_type,
        machine->kernel_filename,
        machine->ram_size, machine->ram_size);

    cpu = HEXAGON_CPU(cpu_create(machine->cpu_type));
    env = &cpu->env;

    MemoryRegion *address_space = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    memory_region_init_ram(sram, NULL, "lpddr4.ram",
        machine->ram_size, &error_fatal);
    memory_region_add_subregion(address_space, 0x0, sram);

    /* vtcm 0xd8000000 for 4MB */
    MemoryRegion *vtcm = g_new(MemoryRegion, 1);
    memory_region_init_ram(vtcm, NULL, "vtcm.ram", 1024 * 1024 * 4,
        &error_fatal);
    memory_region_add_subregion(address_space, 0xd8000000, vtcm);

    /* tcm at 0xd8400000 for 1mb */
    MemoryRegion *tcm = g_new(MemoryRegion, 1);
    memory_region_init_ram(tcm, NULL, "tcm.ram", 1024 * 1024,
        &error_fatal);
    memory_region_add_subregion(address_space, 0xd8400000, tcm);

//    MemoryRegion *iomem = g_new(MemoryRegion, 1);
//    memory_region_init_io(iomem, NULL, &hexagon_qemu_ops,
//                          NULL, "hexagon-qemu", 0x10000);
//    memory_region_add_subregion(address_space, 0x1f000000, iomem);

    hexagon_binfo.ram_size = machine->ram_size;
    hexagon_binfo.kernel_filename = machine->kernel_filename;

    if (machine->kernel_filename) {
        hexagon_load_kernel(env);
    } else {
        env->gpr[HEX_REG_PC] = 0x0; /* 0 or 0x20;*/
    }

    env->sreg[HEX_SREG_EVB] = 0x0;
    env->sreg[HEX_SREG_CFGBASE] = 0xde00;
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
