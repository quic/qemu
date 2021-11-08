/*
 * Copyright (c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
 */

#ifndef INT16_EMU_H
#define INT16_EMU_H 1

extern uint32_t count_leading_ones_8(uint64_t src);
extern size16s_t cast8s_to_16s(int64_t a);
extern int64_t cast16s_to_8s(size16s_t a);
extern int32_t cast16s_to_4s(size16s_t a);
extern uint32_t count_leading_zeros_16(size16s_t src);
extern size16s_t add128(size16s_t a, size16s_t b);
extern size16s_t sub128(size16s_t a, size16s_t b);
extern size16s_t shiftr128(size16s_t a, uint32_t n);
extern size16s_t shiftl128(size16s_t a, uint32_t n);
extern size16s_t and128(size16s_t a, size16s_t b);
extern int64_t conv_round64(int64_t a, uint32_t n);
extern size16s_t mult64_to_128(int64_t X, int64_t Y);

#endif
