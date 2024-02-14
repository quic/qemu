#
# libqemu exported functions and types
#
# Copyright (c) 2019 Luc Michel <luc.michel@greensocs.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, see <http://www.gnu.org/licenses/>.
#

ExportedType('QemuError', 'Error')

PrivateInclude('libqemu/wrappers/iothread.h')
PrivateInclude('qemu/main-loop.h')
ExportedFct('qemu_mutex_lock_iothread', 'void', [], priv = 'libqemu_mutex_lock_iothread')
ExportedFct('qemu_mutex_unlock_iothread', 'void', [],
            priv = 'libqemu_mutex_unlock_iothread')

PrivateInclude('qom/object.h')

ExportedType('QemuObject', 'Object')
ExportedFct('object_new', 'Object *', [ 'const char *' ], iothread_locked = True)

ExportedFct('object_ref', 'void', [ 'Object *' ])

ExportedFct('object_unref', 'void', [ 'Object *' ], iothread_locked = True)

ExportedFct('object_property_set_str' , 'void',
        [ 'Object *', 'const char *', 'const char *', 'Error **' ], on_iothread = True)

ExportedFct('object_property_set_link', 'void',
        [ 'Object *', 'const char *'  , 'Object *'  , 'Error **' ], on_iothread = True)

ExportedFct('object_property_set_bool', 'void',
        [ 'Object *', 'const char *'  ,'bool'       , 'Error **' ], on_iothread = True)

ExportedFct('object_property_set_int' , 'void',
        [ 'Object *', 'const char *'  , 'int64_t'   , 'Error **' ], on_iothread = True)

ExportedFct('object_property_set_uint', 'void',
        [ 'Object *', 'const char *'  , 'uint64_t'  , 'Error **' ], on_iothread = True)

ExportedFct('object_property_add_child', 'void',
        [ 'Object *', 'const char *', 'Object *' ], iothread_locked = True)

ExportedFct('object_get_root', 'Object *', [])


PublicInclude('libqemu/wrappers/memory.h')
PublicInclude('exec/hwaddr.h')
PublicInclude('exec/memattrs.h')

PrivateInclude('exec/memory.h')

ExportedType('QemuMemoryRegion', 'MemoryRegion')
ExportedType('QemuMemoryRegionOps', 'MemoryRegionOps')

ExportedFct('memory_region_new', 'MemoryRegion *', [], priv = 'libqemu_memory_region_new')

ExportedFct('memory_region_init_io', 'void',
        [ 'MemoryRegion *', 'Object *', 'const MemoryRegionOps *',
            'void *', 'const char *', 'uint64_t' ], priv = 'libqemu_memory_region_init_io')

ExportedFct('memory_region_init_ram_ptr', 'void',
        [ 'MemoryRegion *', 'Object *', 'const char *', 'uint64_t', 'void *' ])

ExportedFct('memory_region_init_alias', 'void',
        [ 'MemoryRegion *', 'Object *', 'const char *', 'MemoryRegion *', 'hwaddr', 'uint64_t' ])

ExportedFct('memory_region_add_subregion', 'void',
        [ 'MemoryRegion *', 'hwaddr', 'MemoryRegion *' ], iothread_locked = True)

ExportedFct('memory_region_del_subregion', 'void',
        [ 'MemoryRegion *', 'MemoryRegion *' ], iothread_locked = True)

ExportedFct('memory_region_dispatch_read', 'MemTxResult',
        [ 'MemoryRegion *', 'hwaddr', 'uint64_t *', 'unsigned int', 'MemTxAttrs' ],
        priv = 'libqemu_memory_region_dispatch_read',
        iothread_locked = True)

ExportedFct('memory_region_dispatch_write', 'MemTxResult',
        [ 'MemoryRegion *', 'hwaddr', 'uint64_t', 'unsigned int', 'MemTxAttrs' ],
        priv = 'libqemu_memory_region_dispatch_write',
        iothread_locked = True)

ExportedFct('memory_region_size', 'uint64_t', [ 'MemoryRegion *' ])


ExportedFct('mr_ops_new', 'MemoryRegionOps *', [], priv = 'libqemu_mr_ops_new')

ExportedFct('mr_ops_free', 'void', [ 'MemoryRegionOps *' ], priv = 'libqemu_mr_ops_free')

ExportedFct('mr_ops_set_read_cb', 'void',
        [ 'MemoryRegionOps *', 'LibQemuMrReadCb' ], priv = 'libqemu_mr_ops_set_read_cb')

ExportedFct('mr_ops_set_write_cb', 'void',
        [ 'MemoryRegionOps *', 'LibQemuMrWriteCb' ], priv = 'libqemu_mr_ops_set_write_cb')

ExportedFct('mr_ops_set_max_access_size', 'void',
        [ 'MemoryRegionOps *', 'unsigned' ], priv = 'libqemu_mr_ops_max_access_size')

PrivateInclude('hw/irq.h')
PublicInclude('libqemu/wrappers/gpio.h')
ExportedType('QemuGpio', 'struct IRQState')

ExportedFct('gpio_new', 'struct IRQState *', [ 'LibQemuGpioHandlerFn', 'void *' ],
        priv = 'libqemu_gpio_new')
ExportedFct('gpio_set', 'void', [ 'struct IRQState *', 'int'],
        priv = 'qemu_set_irq', iothread_locked = True)


PrivateInclude('hw/qdev-core.h')
PrivateInclude('hw/qdev-properties.h')
ExportedType('QemuDevice', 'DeviceState')
ExportedType('QemuBus', 'BusState')

ExportedFct('qdev_get_child_bus', 'BusState *', [ 'DeviceState *', 'const char *' ])
ExportedFct('qdev_set_parent_bus', 'void', [ 'DeviceState *', 'BusState *' ])

ExportedFct('qdev_connect_gpio_out', 'void', [ 'DeviceState *', 'int', 'struct IRQState *' ])
ExportedFct('qdev_connect_gpio_out_named', 'void',
        [ 'DeviceState *', 'const char *', 'int', 'struct IRQState *' ])
ExportedFct('qdev_get_gpio_in', 'struct IRQState *', [ 'DeviceState *', 'int' ])
ExportedFct('qdev_get_gpio_in_named', 'struct IRQState *', [ 'DeviceState *', 'const char *', 'int' ])
ExportedFct('qdev_prop_set_chr', 'void', [ 'DeviceState *', 'const char *', 'Chardev *'])

PrivateInclude('hw/sysbus.h')
ExportedType('QemuSysBusDevice', 'SysBusDevice')

ExportedFct('sysbus_mmio_get_region', 'MemoryRegion *', [ 'SysBusDevice *', 'int' ])
ExportedFct('sysbus_connect_gpio_out', 'void', [ 'SysBusDevice *', 'int', 'struct IRQState *' ],
        priv = 'sysbus_connect_irq')

PublicInclude('libqemu/wrappers/cpu.h')
ExportedFct('cpu_loop', 'void', [ 'Object *' ], priv = 'libqemu_cpu_loop')
ExportedFct('cpu_loop_is_busy', 'bool', [ 'Object *' ], priv = 'libqemu_cpu_loop_is_busy')
ExportedFct('cpu_can_run', 'bool', [ 'Object *' ], priv = 'libqemu_cpu_can_run')
ExportedFct('cpu_register_thread', 'void', [ 'Object *' ], priv = 'libqemu_cpu_register_thread')
ExportedFct('cpu_kick', 'void', [ 'Object *' ], priv = 'qemu_cpu_kick')
ExportedFct('cpu_reset', 'void', [ 'Object *' ], priv = 'libqemu_cpu_reset')
ExportedFct('cpu_halt', 'void', [ 'Object *', 'bool', ], priv = 'libqemu_cpu_halt')
ExportedFct('cpu_set_soft_stopped', 'void', [ 'Object *', 'bool' ],
        priv = 'libqemu_cpu_set_soft_stopped')
ExportedFct('cpu_get_soft_stopped', 'bool', [ 'Object *', ],
        priv = 'libqemu_cpu_get_soft_stopped')
ExportedFct('current_cpu_get', 'Object *', [], priv = 'libqemu_current_cpu_get')
ExportedFct('current_cpu_set', 'void', [ 'Object *' ], priv = 'libqemu_current_cpu_set')
ExportedFct('async_safe_run_on_cpu', 'void', [ 'Object *', 'LibQemuAsyncCpuJobFn', 'void *' ],
        priv = 'libqemu_async_safe_run_on_cpu')

PrivateInclude('exec/gdbstub.h')
ExportedFct('gdbserver_start', 'void', ['const char *'], on_iothread = True)

PublicInclude('libqemu/wrappers/timer.h')
ExportedType('QemuTimer', 'QEMUTimer')
ExportedFct('clock_virtual_get_ns', 'int64_t', [ ], priv = 'libqemu_clock_virtual_get_ns')
ExportedFct('timer_new_virtual_ns', 'QemuTimer *', [ 'LibQemuTimerCb', 'void *' ],
        priv = 'libqemu_timer_new_virtual_ns')
ExportedFct('timer_free', 'void', [ 'QemuTimer *' ])
ExportedFct('timer_mod_ns', 'void', [ 'QemuTimer *', 'int64_t' ])
ExportedFct('timer_del', 'void', [ 'QemuTimer *' ])

PublicInclude('libqemu/wrappers/libqemu.h')
ExportedFct('set_cpu_end_of_loop_cb', 'void', [ 'LibQemuCpuEndOfLoopFn', 'void *' ],
        priv = 'libqemu_set_cpu_end_of_loop_cb')
ExportedFct('set_cpu_kick_cb', 'void', [ 'LibQemuCpuKickFn', 'void *' ],
        priv = 'libqemu_set_cpu_kick_cb')

PrivateInclude('libqemu/wrappers/target/arm.h', arch = 'aarch64')
ExportedFct('cpu_arm_set_cp15_cbar', 'void', [ 'Object *', 'uint64_t' ],
        priv = 'libqemu_cpu_arm_set_cp15_cbar', arch = 'aarch64')
ExportedFct('cpu_arm_add_nvic_link', 'void', [ 'Object *' ],
        priv = 'libqemu_cpu_arm_add_nvic_link', arch = 'aarch64')
ExportedFct('arm_nvic_add_cpu_link', 'void', [ 'Object *' ],
        priv = 'libqemu_arm_nvic_add_cpu_link', arch = 'aarch64')
ExportedFct('cpu_aarch64_set_aarch64_mode', 'void', [ 'Object *', 'bool' ],
        priv = 'libqemu_cpu_aarch64_set_aarch64_mode', arch = 'aarch64')
ExportedFct('cpu_arm_get_exclusive_val', 'uint64_t', [ 'Object *' ],
        priv = 'libqemu_cpu_arm_get_exclusive_val', arch = 'aarch64')

PrivateInclude('exec/ram_addr.h')
ExportedFct('tb_invalidate_phys_range', 'void', [ 'uint64_t', 'uint64_t' ])

PrivateInclude('libqemu/wrappers/chardev.h')
ExportedType('QemuChardev', 'Chardev')
ExportedFct('char_dev_new', 'Chardev *', [ 'const char *', 'const char *' ],
        priv = 'libqemu_char_dev_new')
