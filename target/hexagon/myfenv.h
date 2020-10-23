/* Copyright (c) 2019 Qualcomm Innovation Center, Inc. All Rights Reserved. */

#ifndef MYFENV_H
#define MYFENV_H 1

#ifdef VCPP
#define WINDOWS 1
#endif

#if defined(SLOWLARIS)

/* Make our own! */

#include <ieeefp.h>


enum {
	FP_NAN,
	FP_INFINITE,
	FP_ZERO,
	FP_SUBNORMAL,
	FP_NORMAL
};

static inline int fpclassify(double d)
{
	switch (fpclass(d)) {
	case FP_SNAN:
	case FP_QNAN:
		return FP_NAN;
	case FP_NINF:
	case FP_PINF:
		return FP_INFINITE;
	case FP_NDENORM:
	case FP_PDENORM:
		return FP_SUBNORMAL;
	case FP_NZERO:
	case FP_PZERO:
		return FP_ZERO;
	case FP_NNORM:
	case FP_PNORM:
		return FP_NORMAL;
	}
	return 0;
}

static inline int isnan(double d)
{
	return unordered(d, d);
}

static inline int isinf(double d)
{
	return (fpclassify(d) == FP_INFINITE);
}

static inline int isfinite(double d)
{
	switch (fpclassify(d)) {
	case FP_NAN:
	case FP_INFINITE:
		return 0;
	default:
		return 1;
	}
}

#define FE_TONEAREST  FP_RN
#define FE_TOWARDZERO FP_RZ
#define FE_DOWNWARD   FP_RM
#define FE_UPWARD     FP_RP

#define fegetround fpgetround
#define fesetround fpsetround

#define FE_INVALID   FP_X_INV
#define FE_DIVBYZERO FP_X_DZ
#define FE_OVERFLOW  FP_X_OFL
#define FE_UNDERFLOW FP_X_UFL
#define FE_INEXACT   FP_X_IMP

#define FE_ALL_EXCEPT ( FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW | FE_UNDERFLOW | FE_INEXACT )

#define feraiseexcept(MASK) (fpsetsticky(MASK | fpgetsticky()))
#define feclearexcept(MASK) (fpsetsticky(fpgetsticky() & (~(MASK))))
#define fetestexcept(MASK) ( fpgetsticky() & MASK)

#define fma __builtin_fma

typedef int fexcept_t;

static inline int fegetexceptflag(fexcept_t * flags, unsigned int mask)
{
	*flags = (fpgetsticky() & mask);
	return 0;
}

static inline int fesetexceptflag(const fexcept_t * flags,
								  unsigned int mask)
{
	fpsetsticky((fpgetsticky() & ~mask) | (*flags & mask));
	return 0;
}

#elif defined(INTEL)
#include <mathimf.h>
#include <fenv.h>

#define bzero(x, y) memset(x, 0,y)

#elif defined(WIN32)
#include <math.h>

#define bzero(x, y) memset(x, 0,y)

typedef unsigned long fexcept_t;
typedef struct fenv_t {			/* FPP registers */
	fexcept_t _Fe_ctl, _Fe_stat, _Fe_pad[5];
} fenv_t;



#define FE_DOWNWARD     0x01
#define FE_TONEAREST    0x00
#define FE_TOWARDZERO   0x03
#define FE_UPWARD       0x02

#define _FE_EXCEPT_OFF	0
#define _FE_EXMASK_OFF	0
#define _FE_RND_OFF	10

#define _FE_AUTO_RAISE

#define _FE_DIVBYZERO   0x04
#define _FE_INEXACT     0x20
#define _FE_INVALID     0x01
#define _FE_OVERFLOW    0x08
#define _FE_UNDERFLOW   0x10

#define FE_DIVBYZERO	_FE_DIVBYZERO
#define FE_INEXACT	_FE_INEXACT
#define FE_INVALID	_FE_INVALID
#define FE_OVERFLOW	_FE_OVERFLOW
#define FE_UNDERFLOW	_FE_UNDERFLOW

#define FE_ALL_EXCEPT	(FE_DIVBYZERO | FE_INEXACT \
	| FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW)

#define _FE_RND_MASK	0x03u

#define FE_DFL_ENV	(&_CSTD _Fenv0)

extern int fesetround(int mode);
extern int fegetround(void);
extern int feraiseexcept(int exception);
extern int fetestexcept(int excepts);
extern int feclearexcept(int exception);
extern int fegetexceptflag(fexcept_t * flag, int excepts);
extern int fesetexceptflag(const fexcept_t * flag, int exception);

extern int fegetenv(fenv_t *);
extern int feholdexcept(fenv_t *);
extern int fesetenv(const fenv_t *);
extern int feupdateenv(const fenv_t *);
extern int _isnan(double);

#else

/* SANITY! */
#include <fenv.h>
#include <math.h>
#include <float.h>

#endif

#endif
