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

#ifndef HEXAGON_MMVEC_H
#define HEXAGON_MMVEC_H


#include "mmvec_qfloat.h"


#define VECEXT 1


enum ext_mem_access_types {
	access_type_vload = NUM_CORE_ACCESS_TYPES,
	access_type_vstore,
	access_type_vload_nt,
	access_type_vstore_nt,
    access_type_vgather_load,
    access_type_vscatter_store,
    access_type_vscatter_release,
	access_type_vgather_release,
    access_type_vfetch,
	NUM_EXT_ACCESS_TYPES
};

// This cast is safe as long as the enums do not overlap
#define ext_ma_as_core_ma(access) ((enum mem_access_types)access)

#define MAX_VEC_SIZE_LOGBYTES 7
#define MAX_VEC_SIZE_BYTES  (1<<MAX_VEC_SIZE_LOGBYTES)
// #define VEC_CONTEXTS        2

#define NUM_ZREGS			8
#define NUM_VREGS           32
#define NUM_QREGS           4



typedef uint32_t ZRegMask; // at least NUM_VREGS bits
typedef uint32_t VRegMask; // at least NUM_VREGS bits
typedef uint32_t QRegMask; // at least NUM_QREGS bits

// Use software vector length?
#define VECTOR_SIZE_BYTE    (fVECSIZE())

typedef union {
	size8u_t ud[MAX_VEC_SIZE_BYTES/8];
	size8s_t  d[MAX_VEC_SIZE_BYTES/8];
	size4u_t uw[MAX_VEC_SIZE_BYTES/4];
	size4s_t  w[MAX_VEC_SIZE_BYTES/4];
	size2u_t uh[MAX_VEC_SIZE_BYTES/2];
	size2s_t  h[MAX_VEC_SIZE_BYTES/2];
	size1u_t ub[MAX_VEC_SIZE_BYTES/1];
	size1s_t  b[MAX_VEC_SIZE_BYTES/1];
    size4s_t qf32[MAX_VEC_SIZE_BYTES/4];
	size2s_t qf16[MAX_VEC_SIZE_BYTES/2];
	size4s_t sf[MAX_VEC_SIZE_BYTES/4];
	size2s_t hf[MAX_VEC_SIZE_BYTES/2];
	size2s_t bf[MAX_VEC_SIZE_BYTES/2];
} mmvector_t;
typedef mmvector_t MMVector;

typedef union {
	size8u_t ud[2*MAX_VEC_SIZE_BYTES/8];
	size8s_t  d[2*MAX_VEC_SIZE_BYTES/8];
	size4u_t uw[2*MAX_VEC_SIZE_BYTES/4];
	size4s_t  w[2*MAX_VEC_SIZE_BYTES/4];
	size2u_t uh[2*MAX_VEC_SIZE_BYTES/2];
	size2s_t  h[2*MAX_VEC_SIZE_BYTES/2];
	size1u_t ub[2*MAX_VEC_SIZE_BYTES/1];
	size1s_t  b[2*MAX_VEC_SIZE_BYTES/1];
    size4s_t qf32[MAX_VEC_SIZE_BYTES/4];
	size2s_t qf16[MAX_VEC_SIZE_BYTES/2];
	size4s_t sf[MAX_VEC_SIZE_BYTES/4];
	size2s_t hf[MAX_VEC_SIZE_BYTES/2];
	size2s_t bf[MAX_VEC_SIZE_BYTES/2];
	mmvector_t v[2];
} mmvector_pair_t;
typedef mmvector_pair_t MMVectorPair;

typedef union {
	size8u_t ud[4*MAX_VEC_SIZE_BYTES/8];
	size8s_t  d[4*MAX_VEC_SIZE_BYTES/8];
	size4u_t uw[4*MAX_VEC_SIZE_BYTES/4];
	size4s_t  w[4*MAX_VEC_SIZE_BYTES/4];
	size2u_t uh[4*MAX_VEC_SIZE_BYTES/2];
	size2s_t  h[4*MAX_VEC_SIZE_BYTES/2];
	size1u_t ub[4*MAX_VEC_SIZE_BYTES/1];
	size1s_t  b[4*MAX_VEC_SIZE_BYTES/1];
    size4s_t qf32[MAX_VEC_SIZE_BYTES/4];
	size2s_t qf16[MAX_VEC_SIZE_BYTES/2];
	size4s_t sf[MAX_VEC_SIZE_BYTES/4];
	size2s_t hf[MAX_VEC_SIZE_BYTES/2];
	size2s_t bf[MAX_VEC_SIZE_BYTES/2];
	mmvector_t v[4];
} mmvector_quad_t;

typedef union {
	size8u_t ud[MAX_VEC_SIZE_BYTES/8/8];
	size8s_t  d[MAX_VEC_SIZE_BYTES/8/8];
	size4u_t uw[MAX_VEC_SIZE_BYTES/4/8];
	size4s_t  w[MAX_VEC_SIZE_BYTES/4/8];
	size2u_t uh[MAX_VEC_SIZE_BYTES/2/8];
	size2s_t  h[MAX_VEC_SIZE_BYTES/2/8];
	size1u_t ub[MAX_VEC_SIZE_BYTES/1/8];
	size1s_t  b[MAX_VEC_SIZE_BYTES/1/8];
} mmqreg_t;
typedef mmqreg_t MMQReg;

typedef struct {
    mmvector_t data;
    DECLARE_BITMAP(mask, MAX_VEC_SIZE_BYTES);
    mmvector_pair_t offsets;
    paddr_t pa[MAX_VEC_SIZE_BYTES];
    int size;
    paddr_t pa_base;
    target_ulong va_base;
    target_ulong va[MAX_VEC_SIZE_BYTES];
    int oob_access;
    int op;
    int op_size;
} VTCMStoreLog;


typedef struct {
    int num;
    int size;
    int slot;
    int type;
    paddr_t pa[MAX_VEC_SIZE_BYTES];
    union {
        size1u_t b[MAX_VEC_SIZE_BYTES];
        size2u_t h[MAX_VEC_SIZE_BYTES/2];
        size4u_t w[MAX_VEC_SIZE_BYTES/4];
    };
} vtcm_storelog_verif_t;

typedef enum {
	EXT_DFL,
	EXT_NEW,
	EXT_TMP,
	EXT_REMAP
} VRegWriteType;

#define MMVECX_LOG_MEM(VA,PA,WIDTH,DATA,SLOT,LOGTYPE,TYPE)
#define MMVECX_LOG_MEM_LOAD(VA,PA,WIDTH,DATA,SLOT)
#define MMVECX_LOG_MEM_STORE(VA,PA,WIDTH,DATA,SLOT)
#define MMVECX_LOG_MEM_GATHER(VA,PA,WIDTH,DATA,SLOT)
#define MMVECX_LOG_MEM_SCATTER(VA,PA,WIDTH,DATA,SLOT)
#define MMVECX_CLEAR_LOG_MEM(LOGTYPE,SLOT)
#define MMVECX_CLEAR_LOG_MEM_STORES(SLOT)
#define MMVECX_CLEAR_LOG_MEM_LOADS(SLOT)

#define thread_t CPUHexagonState
static inline int mmvec_current_veclogsize(thread_t *thread)
{
	return MAX_VEC_SIZE_LOGBYTES;
}

#endif

