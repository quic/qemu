#include "hw/misc/qct-shmem.h"

#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "sysemu/hostmem.h"

static void qct_shmem_realize(DeviceState *dev, Error **errp)
{
    QCTShmemState *s = QCT_SHMEM(dev);

    if (!s->hostmem) {
        error_setg(errp, "You must specify a 'memdev'");
        return;
    } else if (host_memory_backend_is_mapped(s->hostmem)) {
        error_setg(errp, "can't use already busy memdev: %s",
                   object_get_canonical_path_component(OBJECT(s->hostmem)));
        return;
    }

    s->iomem = host_memory_backend_get_memory(s->hostmem);
    host_memory_backend_set_mapped(s->hostmem, true);
    vmstate_register_ram(s->iomem, dev);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), s->iomem);
}

static Property qct_qtimer_properties[] = {
    DEFINE_PROP_LINK("memdev", QCTShmemState, hostmem, TYPE_MEMORY_BACKEND,
                     HostMemoryBackend *),
    DEFINE_PROP_END_OF_LIST(),
};

static const VMStateDescription vmstate_qct_shmem = {
    .name = "qct-shmem",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]){ VMSTATE_END_OF_LIST() }
};

static void qct_shmem_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *k = DEVICE_CLASS(klass);

    device_class_set_props(k, qct_qtimer_properties);
    k->realize = qct_shmem_realize;
    k->vmsd = &vmstate_qct_shmem;
}

static const TypeInfo qct_shmem_info = {
    .name = TYPE_QCT_SHMEM,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(QCTShmemState),
    .class_init = qct_shmem_class_init,
};

static void qct_shmem_register_types(void)
{
    type_register_static(&qct_shmem_info);
}

type_init(qct_shmem_register_types)