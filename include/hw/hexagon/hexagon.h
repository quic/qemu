#ifndef HW_HEXAGON_H
#define HW_HEXAGON_H

#include "exec/memory.h"

struct hexagon_board_boot_info {
    uint64_t ram_size;
    const char *kernel_filename;
};
#endif
