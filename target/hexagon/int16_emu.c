/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#include <stdio.h>
#include <assert.h>

#include "int16_emu.h"

#ifdef STANDALONE

size4u_t count_leading_ones_8(size8u_t src)
{
	int ret;
	for (ret = 0; src & 0x8000000000000000LL; src <<= 1) {
		ret++;
	}
	return ret;
}
#endif

size16s_t cast8s_to_16s(size8s_t a)
{
	size16s_t result = {.hi = 0, .lo = 0};
	result.lo = a;
	if (a < 0)
		result.hi = -1;
	return result;
}
size8s_t cast16s_to_8s(size16s_t a)
{
	return a.lo;
}

size4s_t cast16s_to_4s(size16s_t a)
{
	return (size4s_t)a.lo;
}

size4u_t count_leading_zeros_16(size16s_t src) {
	if (src.hi == 0) {
		 return 64 + count_leading_ones_8(~ ((size8u_t)src.lo));
	}
	return count_leading_ones_8(~((size8u_t)src.hi));
}



size16s_t add128(size16s_t a, size16s_t b)
{
	size16s_t result = {.hi = 0, .lo = 0};
	result.lo = a.lo+b.lo;
	result.hi = a.hi+b.hi;

	if (result.lo < b.lo)
		result.hi++;

	return result;
}
size16s_t sub128(size16s_t a, size16s_t b)
{
	size16s_t result = {.hi = 0, .lo = 0};
	result.lo = a.lo-b.lo;
	result.hi = a.hi-b.hi;
	if (result.lo > a.lo)
		result.hi--;

	return result;
}

size16s_t shiftr128(size16s_t a, size4u_t n)
{
	size16s_t result = {.hi = 0, .lo = 0};
	if (n == 0) {
		result.lo = a.lo;
		result.hi = a.hi;
	} else if (n > 127) {
		result.hi = a.hi >> 63;
		result.lo = result.hi;
	} else if (n > 63) {
		result.lo = a.hi >> (n-64);
		result.hi = a.hi >> 63;
	} else {
		result.lo = (a.lo >> n) | (a.hi << (64-n));
		result.hi = a.hi >> n;
	}
	return result;
}

size16s_t shiftl128(size16s_t a, size4u_t n)
{
	size16s_t result = {.hi = 0, .lo = 0};
	if (n == 0) {
		result.lo = a.lo;
		result.hi = a.hi;
	} else if (n > 127) {
		result.lo = 0;
		result.hi = 0;
	} else if (n > 63) {
		result.lo = 0;
		result.hi = a.lo << (n-64);
	} else {
		result.lo = a.lo << n;
		result.hi = (a.hi << n) | (a.lo>>(64-n));
	}
	return result;
}

size16s_t and128(size16s_t a, size16s_t b)
{
	size16s_t result = {.hi = 0, .lo = 0};
	result.lo = a.lo & b.lo;
	result.hi = a.hi & b.hi;
	return result;
}

size8s_t conv_round64(size8s_t a, size4u_t n)
{
	size8s_t val=a;
#if 0
	if (n == 0) {
		val = a;
		val = val >> n;
	} else if ((a & (size8s_t)((1LL << (n - 1)) - 1LL)) == 0) { /* N-1..0 all zero? */
		  // val = (a + (size8s_t) (((size8s_t) ((1LL << n) & a)) >> 1));  /* Add LSB from int part */
		size8s_t b = (size8s_t) ((1LL << n) & a) >> 1;
		size16s_t r = add128(a, b);
		val =  shift128(r, n);
	} else {
		  // val = (a + (1LL << (n - 1)));
		size8s_t b = (size8s_t) (1LL << (n - 1));
		size16s_t r = add128(a, b);
		val =  shift128(r, n);
	}
	#endif
	return val;
}



size16s_t mult64_to_128(size8s_t X, size8s_t Y) {

	size16s_t result = {.hi = 0, .lo = 0};

	// Split input into 32-bit words
    size8u_t X_lo32 = X & 0xFFFFFFFF;
	size8s_t X_hi32 = X >> 32;
    size8u_t Y_lo32 = Y & 0xFFFFFFFF;
	size8s_t Y_hi32 = Y >> 32;

	// 4 Products
	size16s_t XY_lo = {.hi = 0, .lo = X_lo32 * Y_lo32};
	size16s_t XY_mid0 = cast8s_to_16s(X_lo32 * Y_hi32);
	size16s_t XY_mid1 = cast8s_to_16s(X_hi32 * Y_lo32);
	size16s_t XY_hi = cast8s_to_16s(X_hi32 * Y_hi32);

	//printf("in: %llx %llx\n", X, Y );
	////printf("0: %llx %llx\n", XY_lo.hi, XY_lo.lo );
	//printf("1: %llx %llx\n", XY_mid1.hi, XY_mid1.lo );
	//printf("2: %llx %llx\n", XY_mid0.hi, XY_mid0.lo );
	//printf("3: %llx %llx\n", XY_hi.hi, XY_hi.lo );

	result = add128(XY_lo, shiftl128(XY_mid0, 32));
	//printf("R: %llx %llx\n", result.hi, result.lo );
	result = add128(result, shiftl128(XY_mid1, 32));
	//printf("R: %llx %llx\n", result.hi, result.lo );
    result = add128(result, shiftl128(XY_hi, 64));
	//printf("R: %llx %llx\n", result.hi, result.lo );

	return result;
}

