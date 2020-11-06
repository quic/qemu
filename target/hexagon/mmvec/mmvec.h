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

enum mem_access_types {
    access_type_INVALID = 0,
    access_type_unknown = 1,
    access_type_load = 2,
    access_type_store = 3,
    access_type_fetch = 4,
    access_type_dczeroa = 5,
    access_type_dccleana = 6,
    access_type_dcinva = 7,
    access_type_dccleaninva = 8,
    access_type_icinva = 9,
    access_type_ictagr = 10,
    access_type_ictagw = 11,
    access_type_icdatar = 12,
    access_type_dcfetch = 13,
    access_type_l2fetch = 14,
    access_type_l2cleanidx = 15,
    access_type_l2cleaninvidx = 16,
    access_type_l2tagr = 17,
    access_type_l2tagw = 18,
    access_type_dccleanidx = 19,
    access_type_dcinvidx = 20,
    access_type_dccleaninvidx = 21,
    access_type_dctagr = 22,
    access_type_dctagw = 23,
    access_type_k0unlock = 24,
    access_type_l2locka = 25,
    access_type_l2unlocka = 26,
    access_type_l2kill = 27,
    access_type_l2gclean = 28,
    access_type_l2gcleaninv = 29,
    access_type_l2gunlock = 30,
    access_type_synch = 31,
    access_type_isync = 32,
    access_type_pause = 33,
    access_type_load_phys = 34,
    access_type_load_locked = 35,
    access_type_store_conditional = 36,
    access_type_barrier = 37,
#ifdef CLADE
    access_type_clade = 38,
#endif
    access_type_memcpy_load = 39,
    access_type_memcpy_store = 40,
#ifdef CLADE2
    access_type_clade2 = 41,
#endif
    access_type_hmx_load_act = 42,
    access_type_hmx_load_wei = 43,
    access_type_hmx_load_bias = 44,
    access_type_hmx_store = 45,
    access_type_hmx_store_bias = 46,
    access_type_udma_load = 47,
    access_type_udma_store = 48,

    NUM_CORE_ACCESS_TYPES
};

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


#define MAX_VEC_SIZE_LOGBYTES 7
#define MAX_VEC_SIZE_BYTES  (1 << MAX_VEC_SIZE_LOGBYTES)

#define NUM_VREGS           32
#define NUM_QREGS           4

typedef uint32_t VRegMask; /* at least NUM_VREGS bits */
typedef uint32_t QRegMask; /* at least NUM_QREGS bits */

#define VECTOR_SIZE_BYTE    (fVECSIZE())

typedef union {
    size8u_t ud[MAX_VEC_SIZE_BYTES / 8];
    size8s_t  d[MAX_VEC_SIZE_BYTES / 8];
    size4u_t uw[MAX_VEC_SIZE_BYTES / 4];
    size4s_t  w[MAX_VEC_SIZE_BYTES / 4];
    size2u_t uh[MAX_VEC_SIZE_BYTES / 2];
    size2s_t  h[MAX_VEC_SIZE_BYTES / 2];
    size1u_t ub[MAX_VEC_SIZE_BYTES / 1];
    size1s_t  b[MAX_VEC_SIZE_BYTES / 1];
    size4s_t qf32[MAX_VEC_SIZE_BYTES/4];
	size2s_t qf16[MAX_VEC_SIZE_BYTES/2];
    size4s_t sf[MAX_VEC_SIZE_BYTES / 4];
    size2s_t hf[MAX_VEC_SIZE_BYTES / 2];
} mmvector_t;

typedef union {
    size8u_t ud[2 * MAX_VEC_SIZE_BYTES / 8];
    size8s_t  d[2 * MAX_VEC_SIZE_BYTES / 8];
    size4u_t uw[2 * MAX_VEC_SIZE_BYTES / 4];
    size4s_t  w[2 * MAX_VEC_SIZE_BYTES / 4];
    size2u_t uh[2 * MAX_VEC_SIZE_BYTES / 2];
    size2s_t  h[2 * MAX_VEC_SIZE_BYTES / 2];
    size1u_t ub[2 * MAX_VEC_SIZE_BYTES / 1];
    size1s_t  b[2 * MAX_VEC_SIZE_BYTES / 1];
    size4s_t qf32[MAX_VEC_SIZE_BYTES/4];
	size2s_t qf16[MAX_VEC_SIZE_BYTES/2];
    size4s_t sf[MAX_VEC_SIZE_BYTES / 4];
    size2s_t hf[MAX_VEC_SIZE_BYTES / 2];
    mmvector_t v[2];
} mmvector_pair_t;
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
	mmvector_t v[4];
} mmvector_quad_t;

typedef union {
    size8u_t ud[MAX_VEC_SIZE_BYTES / 8 / 8];
    size8s_t  d[MAX_VEC_SIZE_BYTES / 8 / 8];
    size4u_t uw[MAX_VEC_SIZE_BYTES / 4 / 8];
    size4s_t  w[MAX_VEC_SIZE_BYTES / 4 / 8];
    size2u_t uh[MAX_VEC_SIZE_BYTES / 2 / 8];
    size2s_t  h[MAX_VEC_SIZE_BYTES / 2 / 8];
    size1u_t ub[MAX_VEC_SIZE_BYTES / 1 / 8];
    size1s_t  b[MAX_VEC_SIZE_BYTES / 1 / 8];
    size4s_t qf32[MAX_VEC_SIZE_BYTES/4];
	size2s_t qf16[MAX_VEC_SIZE_BYTES/2];
    size4s_t sf[MAX_VEC_SIZE_BYTES / 4];
    size2s_t hf[MAX_VEC_SIZE_BYTES / 2];
} mmqreg_t;

typedef struct {
    mmvector_t data;
    mmvector_t mask;
    mmvector_pair_t offsets;
    int size;
    target_ulong va_base;
    target_ulong va[MAX_VEC_SIZE_BYTES];
    int oob_access;
    int op;
    int op_size;
} vtcm_storelog_t;


/* Types of vector register assignment */
typedef enum {
    EXT_DFL,      /* Default */
    EXT_NEW,      /* New - value used in the same packet */
    EXT_TMP       /* Temp - value used but not stored to register */
} vector_dst_type_t;

#endif
