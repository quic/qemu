#ifndef HEXAGON_SNAPSHOT_H
#define HEXAGON_SNAPSHOT_H

#include "qemu/osdep.h"
#include "qapi/error.h"
#include "cpu.h"

typedef enum { SNAPSHOT_TAKE = 0x1, SNAPSHOT_RESTORE = 0x2 } snapshot_operation;

void hex_snapshot_realize(CPUHexagonState *env, Error **errp);
void hex_snapshot_send_request(CPUHexagonState *env,
                               snapshot_operation operation);

#endif