/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#ifndef INT16_EMU_H
#define INT16_EMU_H 1

#include "hex_arch_types.h"

extern size4u_t count_leading_ones_8(size8u_t src);
extern size16s_t cast8s_to_16s(size8s_t a);
extern size8s_t cast16s_to_8s(size16s_t a);
extern size4s_t cast16s_to_4s(size16s_t a);
extern size4u_t count_leading_zeros_16(size16s_t src);
extern size16s_t add128(size16s_t a, size16s_t b);
extern size16s_t sub128(size16s_t a, size16s_t b);
extern size16s_t shiftr128(size16s_t a, size4u_t n);
extern size16s_t shiftl128(size16s_t a, size4u_t n);
extern size16s_t and128(size16s_t a, size16s_t b);
extern size8s_t conv_round64(size8s_t a, size4u_t n);
extern size16s_t mult64_to_128(size8s_t X, size8s_t Y);

#endif
