/*
 *  Copyright(c) 2019-2020 Qualcomm Innovation Center, Inc. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include <math.h>
#include "qemu/osdep.h"
#include "macros.h"
#include "conv_emu.h"
#include "fma_emu.h"

#define DF_INF_EXP 0x7ff
#define DF_BIAS 1023

#define SF_INF_EXP 0xff
#define SF_BIAS 127

#define HF_INF_EXP 0x1f
#define HF_BIAS 15

#define WAY_BIG_EXP 4096

#define isz(X) (fpclassify(X) == FP_ZERO)

typedef union {
    double f;
    uint64_t i;
    struct {
        uint64_t mant:52;
        uint64_t exp:11;
        uint64_t sign:1;
    } x;
} df_t;

typedef union {
    float f;
    uint32_t i;
    struct {
        uint32_t mant:23;
        uint32_t exp:8;
        uint32_t sign:1;
    } x;
} sf_t;

typedef struct {
    union {
        uint64_t low;
        struct {
            uint32_t w0;
            uint32_t w1;
        };
    };
    union {
        uint64_t high;
        struct {
            uint32_t w3;
            uint32_t w2;
        };
    };
} int128_t;
typedef struct {
    int128_t mant;
    int32_t exp;
    uint8_t sign;
    uint8_t guard;
    uint8_t round;
    uint8_t sticky;
} xf_t;

static inline void xf_init(xf_t *p)
{
    p->mant.low = 0;
    p->mant.high = 0;
    p->exp = 0;
    p->sign = 0;
    p->guard = 0;
    p->round = 0;
    p->sticky = 0;
}

static inline uint64_t df_getmant(df_t a)
{
    int class = fpclassify(a.f);
    switch (class) {
    case FP_NORMAL:
    return a.x.mant | 1ULL << 52;
    case FP_ZERO:
        return 0;
    case FP_SUBNORMAL:
        return a.x.mant;
    default:
        return -1;
    };
}

static inline int32_t df_getexp(df_t a)
{
    int class = fpclassify(a.f);
    switch (class) {
    case FP_NORMAL:
        return a.x.exp;
    case FP_SUBNORMAL:
        return a.x.exp + 1;
    default:
        return -1;
    };
}

static inline uint64_t sf_getmant(sf_t a)
{
    int class = fpclassify(a.f);
    switch (class) {
    case FP_NORMAL:
        return a.x.mant | 1ULL << 23;
    case FP_ZERO:
        return 0;
    case FP_SUBNORMAL:
        return a.x.mant | 0ULL;
    default:
        return -1;
    };
}

static inline int32_t sf_getexp(sf_t a)
{
    int class = fpclassify(a.f);
    switch (class) {
    case FP_NORMAL:
        return a.x.exp;
    case FP_SUBNORMAL:
        return a.x.exp + 1;
    default:
        return -1;
    };
}

static inline int128_t int128_mul_6464(uint64_t ai, uint64_t bi)
{
    int128_t ret;
    int128_t a, b;
    uint64_t pp0, pp1a, pp1b, pp1s, pp2;

    a.high = b.high = 0;
    a.low = ai;
    b.low = bi;
    pp0 = (uint64_t)a.w0 * (uint64_t)b.w0;
    pp1a = (uint64_t)a.w1 * (uint64_t)b.w0;
    pp1b = (uint64_t)b.w1 * (uint64_t)a.w0;
    pp2 = (uint64_t)a.w1 * (uint64_t)b.w1;

    pp1s = pp1a + pp1b;
    if ((pp1s < pp1a) || (pp1s < pp1b)) {
        pp2 += (1ULL << 32);
    }
    ret.low = pp0 + (pp1s << 32);
    if ((ret.low < pp0) || (ret.low < (pp1s << 32))) {
        pp2 += 1;
    }
    ret.high = pp2 + (pp1s >> 32);

    return ret;
}

static inline int128_t int128_shl(int128_t a, uint32_t amt)
{
    int128_t ret;
    if (amt == 0) {
        return a;
    }
    if (amt > 128) {
        ret.high = 0;
        ret.low = 0;
        return ret;
    }
    if (amt >= 64) {
        amt -= 64;
        a.high = a.low;
        a.low = 0;
    }
    ret.high = a.high << amt;
    ret.high |= (a.low >> (64 - amt));
    ret.low = a.low << amt;
    return ret;
}

static inline int128_t int128_shr(int128_t a, uint32_t amt)
{
    int128_t ret;
    if (amt == 0) {
        return a;
    }
    if (amt > 128) {
        ret.high = 0;
        ret.low = 0;
        return ret;
    }
    if (amt >= 64) {
        amt -= 64;
        a.low = a.high;
        a.high = 0;
    }
    ret.low = a.low >> amt;
    ret.low |= (a.high << (64 - amt));
    ret.high = a.high >> amt;
    return ret;
}

static inline int128_t int128_add(int128_t a, int128_t b)
{
    int128_t ret;
    ret.low = a.low + b.low;
    if ((ret.low < a.low) || (ret.low < b.low)) {
        /* carry into high part */
        a.high += 1;
    }
    ret.high = a.high + b.high;
    return ret;
}

static inline int128_t int128_sub(int128_t a, int128_t b, int borrow)
{
    int128_t ret;
    ret.low = a.low - b.low;
    if (ret.low > a.low) {
        /* borrow into high part */
        a.high -= 1;
    }
    ret.high = a.high - b.high;
    if (borrow == 0) {
        return ret;
    } else {
        a.high = 0;
        a.low = 1;
        return int128_sub(ret, a, 0);
    }
}

static inline int int128_gt(int128_t a, int128_t b)
{
    if (a.high == b.high) {
        return a.low > b.low;
    }
    return a.high > b.high;
}

static inline xf_t xf_norm_left(xf_t a)
{
    a.exp--;
    a.mant = int128_shl(a.mant, 1);
    a.mant.low |= a.guard;
    a.guard = a.round;
    a.round = a.sticky;
    return a;
}

static inline xf_t xf_norm_right(xf_t a, int amt)
{
    if (amt > 130) {
        a.sticky |=
            a.round | a.guard | (a.mant.low != 0) | (a.mant.high != 0);
        a.guard = a.round = a.mant.high = a.mant.low = 0;
        a.exp += amt;
        return a;

    }
    while (amt >= 64) {
        a.sticky |= a.round | a.guard | (a.mant.low != 0);
        a.guard = (a.mant.low >> 63) & 1;
        a.round = (a.mant.low >> 62) & 1;
        a.mant.low = a.mant.high;
        a.mant.high = 0;
        a.exp += 64;
        amt -= 64;
    }
    while (amt > 0) {
        a.exp++;
        a.sticky |= a.round;
        a.round = a.guard;
        a.guard = a.mant.low & 1;
        a.mant = int128_shr(a.mant, 1);
        amt--;
    }
    return a;
}


/*
 * On the add/sub, we need to be able to shift out lots of bits, but need a
 * sticky bit for what was shifted out, I think.
 */
static xf_t xf_add(xf_t a, xf_t b);

static inline xf_t xf_sub(xf_t a, xf_t b, int negate)
{
    xf_t ret;
    xf_init(&ret);
    int borrow;

    if (a.sign != b.sign) {
        b.sign = !b.sign;
        return xf_add(a, b);
    }
    if (b.exp > a.exp) {
        /* small - big == - (big - small) */
        return xf_sub(b, a, !negate);
    }
    if ((b.exp == a.exp) && (int128_gt(b.mant, a.mant))) {
        /* small - big == - (big - small) */
        return xf_sub(b, a, !negate);
    }

    while (a.exp > b.exp) {
        /* Try to normalize exponents: shrink a exponent and grow mantissa */
        if (a.mant.high & (1ULL << 62)) {
            /* Can't grow a any more */
            break;
        } else {
            a = xf_norm_left(a);
        }
    }

    while (a.exp > b.exp) {
        /* Try to normalize exponents: grow b exponent and shrink mantissa */
        /* Keep around shifted out bits... we might need those later */
        b = xf_norm_right(b, a.exp - b.exp);
    }

    if ((int128_gt(b.mant, a.mant))) {
        return xf_sub(b, a, !negate);
    }

    /* OK, now things should be normalized! */
    ret.sign = a.sign;
    ret.exp = a.exp;
    assert(!int128_gt(b.mant, a.mant));
    borrow = (b.round << 2) | (b.guard << 1) | b.sticky;
    ret.mant = int128_sub(a.mant, b.mant, (borrow != 0));
    borrow = 0 - borrow;
    ret.guard = (borrow >> 2) & 1;
    ret.round = (borrow >> 1) & 1;
    ret.sticky = (borrow >> 0) & 1;
    if (negate) {
        ret.sign = !ret.sign;
    }
    return ret;
}

static xf_t xf_add(xf_t a, xf_t b)
{
    xf_t ret;
    xf_init(&ret);
    if (a.sign != b.sign) {
        b.sign = !b.sign;
        return xf_sub(a, b, 0);
    }
    if (b.exp > a.exp) {
        /* small + big ==  (big + small) */
        return xf_add(b, a);
    }
    if ((b.exp == a.exp) && int128_gt(b.mant, a.mant)) {
        /* small + big ==  (big + small) */
        return xf_add(b, a);
    }

    while (a.exp > b.exp) {
        /* Try to normalize exponents: shrink a exponent and grow mantissa */
        if (a.mant.high & (1ULL << 62)) {
            /* Can't grow a any more */
            break;
        } else {
            a = xf_norm_left(a);
        }
    }

    while (a.exp > b.exp) {
        /* Try to normalize exponents: grow b exponent and shrink mantissa */
        /* Keep around shifted out bits... we might need those later */
        b = xf_norm_right(b, a.exp - b.exp);
    }

    /* OK, now things should be normalized! */
    if (int128_gt(b.mant, a.mant)) {
        return xf_add(b, a);
    };
    ret.sign = a.sign;
    ret.exp = a.exp;
    assert(!int128_gt(b.mant, a.mant));
    ret.mant = int128_add(a.mant, b.mant);
    ret.guard = b.guard;
    ret.round = b.round;
    ret.sticky = b.sticky;
    return ret;
}

/* Return an infinity with the same sign as a */
static inline df_t infinite_df_t(xf_t a)
{
    df_t ret;
    ret.x.sign = a.sign;
    ret.x.exp = DF_INF_EXP;
    ret.x.mant = 0ULL;
    return ret;
}

/* Return a maximum finite value with the same sign as a */
static inline df_t maxfinite_df_t(xf_t a)
{
    df_t ret;
    ret.x.sign = a.sign;
    ret.x.exp = DF_INF_EXP - 1;
    ret.x.mant = 0x000fffffffffffffULL;
    return ret;
}

static inline df_t f2df_t(double in)
{
    df_t ret;
    ret.f = in;
    return ret;
}

/* Return an infinity with the same sign as a */
static inline sf_t infinite_sf_t(xf_t a)
{
    sf_t ret;
    ret.x.sign = a.sign;
    ret.x.exp = SF_INF_EXP;
    ret.x.mant = 0ULL;
    return ret;
}

/* Return a maximum finite value with the same sign as a */
static inline sf_t maxfinite_sf_t(xf_t a)
{
    sf_t ret;
    ret.x.sign = a.sign;
    ret.x.exp = SF_INF_EXP - 1;
    ret.x.mant = 0x007fffffUL;
    return ret;
}

static inline sf_t f2sf_t(float in)
{
    sf_t ret;
    ret.f = in;
    return ret;
}

#define GEN_XF_ROUND(TYPE, MANTBITS, INF_EXP) \
static inline TYPE xf_round_##TYPE(xf_t a) \
{ \
    TYPE ret; \
    ret.i = 0; \
    ret.x.sign = a.sign; \
    if ((a.mant.high == 0) && (a.mant.low == 0) \
        && ((a.guard | a.round | a.sticky) == 0)) { \
        /* result zero */ \
        switch (fegetround()) { \
        case FE_DOWNWARD: \
            return f2##TYPE(-0.0); \
        default: \
            return f2##TYPE(0.0); \
        } \
    } \
    /* Normalize right */ \
    /* We want MANTBITS bits of mantissa plus the leading one. */ \
    /* That means that we want MANTBITS+1 bits, or 0x000000000000FF_FFFF */ \
    /* So we need to normalize right while the high word is non-zero and \
    * while the low word is nonzero when masked with 0xffe0_0000_0000_0000 */ \
    while ((a.mant.high != 0) || ((a.mant.low >> (MANTBITS + 1)) != 0)) { \
        a = xf_norm_right(a, 1); \
    } \
    /* \
     * OK, now normalize left \
     * We want to normalize left until we have a leading one in bit 24 \
     * Theoretically, we only need to shift a maximum of one to the left if we \
     * shifted out lots of bits from B, or if we had no shift / 1 shift sticky \
     * shoudl be 0  \
     */ \
    while ((a.mant.low & (1ULL << MANTBITS)) == 0) { \
        a = xf_norm_left(a); \
    } \
    /* \
     * OK, now we might need to denormalize because of potential underflow. \
     * We need to do this before rounding, and rounding might make us normal \
     * again \
     */ \
    while (a.exp <= 0) { \
        a = xf_norm_right(a, 1 - a.exp); \
        /* \
         * Do we have underflow? \
         * That's when we get an inexact answer because we ran out of bits \
         * in a denormal. \
         */ \
        if (a.guard || a.round || a.sticky) { \
            feraiseexcept(FE_UNDERFLOW); \
        } \
    } \
    /* OK, we're relatively canonical... now we need to round */ \
    if (a.guard || a.round || a.sticky) { \
        feraiseexcept(FE_INEXACT); \
        switch (fegetround()) { \
        case FE_TOWARDZERO: \
            /* Chop and we're done */ \
            break; \
        case FE_UPWARD: \
            if (a.sign == 0) { \
                a.mant.low += 1; \
            } \
            break; \
        case FE_DOWNWARD: \
            if (a.sign != 0) { \
                a.mant.low += 1; \
            } \
            break; \
        default: \
            if (a.round || a.sticky) { \
                /* round up if guard is 1, down if guard is zero */ \
                a.mant.low += a.guard; \
            } else if (a.guard) { \
                /* exactly .5, round up if odd */ \
                a.mant.low += (a.mant.low & 1); \
            } \
            break; \
        } \
    } \
    /* \
     * OK, now we might have carried all the way up. \
     * So we might need to shr once \
     * at least we know that the lsb should be zero if we rounded and \
     * got a carry out... \
     */ \
    if ((a.mant.low >> (MANTBITS + 1)) != 0) { \
        a = xf_norm_right(a, 1); \
    } \
    /* Overflow? */ \
    if (a.exp >= INF_EXP) { \
        /* Yep, inf result */ \
        feraiseexcept(FE_OVERFLOW); \
        feraiseexcept(FE_INEXACT); \
        switch (fegetround()) { \
        case FE_TOWARDZERO: \
            return maxfinite_##TYPE(a); \
        case FE_UPWARD: \
            if (a.sign == 0) { \
                return infinite_##TYPE(a); \
            } else { \
                return maxfinite_##TYPE(a); \
            } \
        case FE_DOWNWARD: \
            if (a.sign != 0) { \
                return infinite_##TYPE(a); \
            } else { \
                return maxfinite_##TYPE(a); \
            } \
        default: \
            return infinite_##TYPE(a); \
        } \
    } \
    /* Underflow? */ \
    if (a.mant.low & (1ULL << MANTBITS)) { \
        /* Leading one means: No, we're normal. So, we should be done... */ \
        ret.x.exp = a.exp; \
        ret.x.mant = a.mant.low; \
        return ret; \
    } \
    if (a.exp != 1) { \
        printf("a.exp == %d\n", a.exp); \
    } \
    assert(a.exp == 1); \
    ret.x.exp = 0; \
    ret.x.mant = a.mant.low; \
    return ret; \
}

GEN_XF_ROUND(df_t, fDF_MANTBITS(), DF_INF_EXP)
GEN_XF_ROUND(sf_t, fSF_MANTBITS(), SF_INF_EXP)

static inline double special_fma(df_t a, df_t b, df_t c)
{
    df_t ret;
    ret.i = 0;

    /*
     * If A multiplied by B is an exact infinity and C is also an infinity
     * but with the opposite sign, FMA returns NaN and raises invalid.
     */
    if (fISINFPROD(a.f, b.f) && isinf(c.f)) {
        if ((a.x.sign ^ b.x.sign) != c.x.sign) {
            ret.i = fDFNANVAL();
            feraiseexcept(FE_INVALID);
            return ret.f;
        }
    }
    if ((isinf(a.f) && isz(b.f)) || (isz(a.f) && isinf(b.f))) {
        ret.i = fDFNANVAL();
        feraiseexcept(FE_INVALID);
        return ret.f;
    }
    /*
     * If none of the above checks are true and C is a NaN,
     * a NaN shall be returned
     * If A or B are NaN, a NAN shall be returned.
     */
    if (isnan(a.f) || isnan(b.f) || (isnan(c.f))) {
        if (isnan(a.f) && (fGETBIT(51, a.i) == 0)) {
            feraiseexcept(FE_INVALID);
        }
        if (isnan(b.f) && (fGETBIT(51, b.i) == 0)) {
            feraiseexcept(FE_INVALID);
        }
        if (isnan(c.f) && (fGETBIT(51, c.i) == 0)) {
            feraiseexcept(FE_INVALID);
        }
        ret.i = fDFNANVAL();
        return ret.f;
    }
    /*
     * We have checked for adding opposite-signed infinities.
     * Other infinities return infinity with the correct sign
     */
    if (isinf(c.f)) {
        ret.x.exp = DF_INF_EXP;
        ret.x.mant = 0;
        ret.x.sign = c.x.sign;
        return ret.f;
    }
    if (isinf(a.f) || isinf(b.f)) {
        ret.x.exp = DF_INF_EXP;
        ret.x.mant = 0;
        ret.x.sign = (a.x.sign ^ b.x.sign);
        return ret.f;
    }
    g_assert_not_reached();
}

static inline float special_fmaf(sf_t a, sf_t b, sf_t c)
{
    df_t aa, bb, cc;
    aa.f = a.f;
    bb.f = b.f;
    cc.f = c.f;
    return special_fma(aa, bb, cc);
}

float internal_fmafx(float a_in, float b_in, float c_in, int scale)
{
    sf_t a, b, c;
    xf_t prod;
    xf_t acc;
    xf_t result;
    xf_init(&prod);
    xf_init(&acc);
    xf_init(&result);
    a.f = a_in;
    b.f = b_in;
    c.f = c_in;

    if (isinf(a.f) || isinf(b.f) || isinf(c.f)) {
        return special_fmaf(a, b, c);
    }
    if (isnan(a.f) || isnan(b.f) || isnan(c.f)) {
        return special_fmaf(a, b, c);
    }
    if ((scale == 0) && (isz(a.f) || isz(b.f))) {
        return a.f * b.f + c.f;
    }

    /* (a * 2**b) * (c * 2**d) == a*c * 2**(b+d) */
    prod.mant = int128_mul_6464(sf_getmant(a), sf_getmant(b));

    /*
     * Note: extracting the mantissa into an int is multiplying by
     * 2**23, so adjust here
     */
    prod.exp = sf_getexp(a) + sf_getexp(b) - SF_BIAS - 23;
    prod.sign = a.x.sign ^ b.x.sign;
    if (isz(a.f) || isz(b.f)) {
        prod.exp = -2 * WAY_BIG_EXP;
    }
    if ((scale > 0) && (fpclassify(c.f) == FP_SUBNORMAL)) {
        acc.mant = int128_mul_6464(0, 0);
        acc.exp = -WAY_BIG_EXP;
        acc.sign = c.x.sign;
        acc.sticky = 1;
        result = xf_add(prod, acc);
    } else if (!isz(c.f)) {
        acc.mant = int128_mul_6464(sf_getmant(c), 1);
        acc.exp = sf_getexp(c);
        acc.sign = c.x.sign;
        result = xf_add(prod, acc);
    } else {
        result = prod;
    }
    result.exp += scale;
    return xf_round_sf_t(result).f;
}


float internal_fmaf(float a_in, float b_in, float c_in)
{
    return internal_fmafx(a_in, b_in, c_in, 0);
}

float internal_mpyf(float a_in, float b_in)
{
    if (isz(a_in) || isz(b_in)) {
        return a_in * b_in;
    }
    return internal_fmafx(a_in, b_in, 0.0, 0);
}

static inline double internal_mpyhh_special(double a, double b)
{
    return a * b;
}

double internal_mpyhh(double a_in, double b_in,
                      unsigned long long int accumulated)
{
    df_t a, b;
    xf_t x;
    unsigned long long int prod;
    unsigned int sticky;

    a.f = a_in;
    b.f = b_in;
    sticky = accumulated & 1;
    accumulated >>= 1;
    xf_init(&x);
    if (isz(a_in) || isnan(a_in) || isinf(a_in)) {
        return internal_mpyhh_special(a_in, b_in);
    }
    if (isz(b_in) || isnan(b_in) || isinf(b_in)) {
        return internal_mpyhh_special(a_in, b_in);
    }
    x.mant = int128_mul_6464(accumulated, 1);
    x.sticky = sticky;
    prod = fGETUWORD(1, df_getmant(a)) * fGETUWORD(1, df_getmant(b));
    x.mant = int128_add(x.mant, int128_mul_6464(prod, 0x100000000ULL));
    x.exp = df_getexp(a) + df_getexp(b) - DF_BIAS - 20;
    if (!isnormal(a_in) || !isnormal(b_in)) {
        /* crush to inexact zero */
        x.sticky = 1;
        x.exp = -4096;
    }
    x.sign = a.x.sign ^ b.x.sign;
    return xf_round_df_t(x).f;
}

float conv_df_to_sf(double in_f)
{
    xf_t x;
    df_t in;
    if (isz(in_f) || isnan(in_f) || isinf(in_f)) {
        return in_f;
    }
    xf_init(&x);
    in.f = in_f;
    x.mant = int128_mul_6464(df_getmant(in), 1);
    x.exp = df_getexp(in) - DF_BIAS + SF_BIAS - 52 + 23;
    x.sign = in.x.sign;
    return xf_round_sf_t(x).f;
}

