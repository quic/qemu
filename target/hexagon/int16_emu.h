/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#ifndef INT16_EMU_H
#define INT16_EMU_H 1

#ifndef CONFIG_USER_ONLY
#include "qemu/osdep.h"
#include "exec/exec-all.h"
#include "migration/vmstate.h"
#include "qapi/error.h"
#include "qemu/log.h"
#include "qemu/qemu-print.h"
#include "cpu.h"
#include "arch.h"
#endif

size4u_t count_leading_zeros_16(size16s_t src);

//size16s_t add128(size16s_t a, size16s_t b);
//size16s_t sub128(size16s_t a, size16s_t b);
//size16s_t shiftr128(size16s_t a, size4u_t n);
//size16s_t shiftl128(size16s_t a, size4u_t n);
//size16s_t and128(size16s_t a, size16s_t b);
//size16s_t cast8s_to_16s(size8s_t a);
//size8s_t cast16s_to_8s(size16s_t a);
//size4s_t cast16s_to_4s(size16s_t a);

size16s_t mult64_to_128(size8s_t X, size8s_t Y);

#endif
