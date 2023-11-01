#ifndef QCT_SHMEM_H
#define QCT_SHMEM_H

#include "qemu/osdep.h"
#include "hw/sysbus.h"
#include "qemu/typedefs.h"
#include "qom/object.h"

#define TYPE_QCT_SHMEM "qct-shmem"
OBJECT_DECLARE_SIMPLE_TYPE(QCTShmemState, QCT_SHMEM)

struct QCTShmemState {
    SysBusDevice parent_obj;

    HostMemoryBackend *hostmem;
    MemoryRegion *iomem;
};

#endif /* QCT_SHMEM_H */