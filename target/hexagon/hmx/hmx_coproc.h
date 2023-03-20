/*
 *  Copyright(c) 2023 Qualcomm Innovation Center, Inc. All Rights Reserved.
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

#ifndef HEXAGON_HMX_COPROC_H
#define HEXAGON_HMX_COPROC_H

#include <stdio.h>
#include <math.h>
#include "hmx/hmx_xlate_info.h"

#define VTCM_SIZE              0x40000LL
#define VTCM_OFFSET            0x200000LL
#define VSTORES_MAX 2
#define EXCEPTION_DETECTED     0
#define SLOTS_MAX              4
#define TYPE_LOAD 'L'
#define TYPE_STORE 'S'
#define TYPE_FETCH 'F'
#define TYPE_ICINVA 'I'
#define TYPE_DMA_FETCH 'D'
#define HMX_MAX_EGY_CYCLE      512
#define HMX_OUTPUT_DEPTH       32
#define HMX_BLOCK_SIZE         2048
#define HMX_V1                 0
#define HMX_SPATIAL_SIZE       6
#define HMX_CHANNEL_SIZE       5
#define HMX_ADDR_MASK          0xfffff800
#define HMX_BLOCK_BIT          11
#define HMXDEBUGLVL            2

#define sim_mem_read1(X, Y, ADDR)       hmx_mem_read1(X, ADDR)
#define sim_mem_read2(X, Y, ADDR)       hmx_mem_read2(X, ADDR)
#define sim_mem_read4(X, Y, ADDR)       hmx_mem_read4(X, ADDR)
#define sim_mem_read8(X, Y, ADDR)       hmx_mem_read8(X, ADDR)
#define sim_mem_write1(X, Y, addr, val) hmx_mem_write1(X, addr, val)
#define sim_mem_write2(X, Y, addr, val) hmx_mem_write2(X, addr, val)
#define sim_mem_write4(X, Y, addr, val) hmx_mem_write4(X, addr, val)

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
    access_type_hmx_swap_acc = 47,
    access_type_hmx_acc_cvt = 48,
    access_type_hmx_store_cvt_state = 49,
    access_type_hmx_poly_cvt = 50,
    access_type_udma_load = 51,
    access_type_udma_store = 52,
    access_type_unpause = 53,
    NUM_CORE_ACCESS_TYPES
};

struct Einfo {
  uint8_t valid;
  uint8_t type;
  uint8_t cause;
  uint8_t bvs:1;
  uint8_t bv0:1;       /* valid for badva0 */
  uint8_t bv1:1;       /* valid for badva1 */
  uint32_t badva0;
  uint32_t badva1;
  uint32_t elr;
  uint16_t diag;
  uint16_t de_slotmask;
};
typedef struct Einfo hex_exception_info;

typedef struct hmx_mem_access_info {
    int32_t dY;
    uint16_t blocks;
    uint32_t fx;
    uint32_t fy;
    uint32_t x_offset;
    uint32_t y_offset;
    uint32_t tile_x_mask;
    uint32_t tile_y_mask;
    uint32_t tile_y_inc;
    uint32_t y_start;
    uint32_t y_stop;
    uint32_t x_start;
    uint32_t x_stop;
    uint8_t y_tap;
    uint8_t x_tap;
    uint8_t ch_start;
    uint8_t ch_stop;
    uint8_t group_size;
    uint8_t group_count;
    uint8_t group_count_ratio;
    uint8_t block_type:3;
    uint8_t format:2;
    uint8_t acc_select:1;
    uint8_t acc_range:1;
    uint8_t flt:1;
    uint8_t x_dilate:1;
    uint8_t y_dilate:1;
    uint8_t deep:1;
    uint8_t wgt_deep:1;
    uint8_t drop:1;
    uint8_t batch:1;
    int8_t weight_count;
    uint8_t bias_32bit;
    uint8_t cvt_wr_only:1;
    uint8_t weight_bits;
    uint8_t enable16x16;
    uint8_t outputselect16x16;
    uint8_t act_reuse;
    uint32_t wgt_size;
    uint32_t wgtc_mode;
    uint32_t wgtc_global_density;
    uint16_t egy_mpy_acc[HMX_MAX_EGY_CYCLE];
    uint8_t egy_cvt;
} hmx_mem_access_info_t;

typedef struct {
    target_ulong vaddr;
    target_ulong bad_vaddr;
    uint64_t paddr;
    uint32_t range;
    uint32_t width;
    uint64_t stdata;
    int cancelled;
    int tnum;
    int size;
    enum mem_access_types type;
    unsigned char cdata[512];
    uint64_t lddata;
    uint8_t check_page_crosses;
    uint32_t page_cross_base;
    uint32_t page_cross_sum;
    uint16_t slot;
    xlate_info_t xlate_info;
    hmx_mem_access_info_t hmx_ma;
    uint8_t is_dealloc:1;
    uint8_t is_memop:1;
    uint8_t valid:1;
    uint8_t log_as_tag:1;
    uint8_t no_deriveumaptr:1;
} mem_access_info_t;

typedef struct SystemState {
    void *vtcm_haddr;
    hwaddr vtcm_base;
} system_t;

struct ProcessorState;
typedef struct ThreadState {
    struct ProcessorState *processor_ptr;
    system_t *system_ptr;
    unsigned int threadId;
    uint8_t store_pending[VSTORES_MAX];          /* FIXME - Is this needed? */
    mem_access_info_t mem_access[SLOTS_MAX];
    unsigned int timing_on;
    uint64_t pktid;
    uint8_t bq_on;
    uint32_t reg_usr;
} thread_t;

/* one hmx unit shared among all threads */
struct HMX_State;
struct options_struct_S;
struct arch_proc_opt;
typedef struct ProcessorState {
    struct ThreadState * thread[1];
    struct options_struct_S *options;
    struct arch_proc_opt * arch_proc_options;
    struct HMX_State *shared_extptr;
} processor_t;

typedef void (*hmx_mac_fxp_callback_t)(void *sys, processor_t *proc, int pktid,
                                       int row_idx, int col_idx, int acc_idx,
                                       int in_idx, int wgt, int act, int acc,
                                       int x_tap, int y_tap, int block_idx,
                                       int deep_block_idx);
typedef void (*hmx_mac_flt_callback_t) (void *, ...);
typedef void (*hmx_cvt_fxp_state_callback_t) (system_t *sys,
                                              processor_t *proc,
                                              uint32_t pktid,
                                              uint32_t row_idx,
                                              uint32_t col_idx,
                                              uint32_t acc_idx,
                                              uint32_t val);
typedef void (*hmx_cvt_state_transfer_callback_t) (void *, ...);
typedef void (*hmx_cvt_state_write_callback_t)(system_t *sys,
                                               processor_t *proc,
                                               uint32_t pktid, uint32_t age,
                                               uint32_t row_idx,
                                               uint32_t col_idx,
                                               uint32_t acc_idx,
                                               uint32_t val);
typedef void (*hmx_wgt_decomp_callback_t)(system_t *sys, processor_t *proc,
                                          int block_idx, int pktid,
                                          int lane_idx, int vector_idx,
                                          uint64_t metadata_addr,
                                          int meta_addr_valid,
                                          uint16_t meta_16bits,
                                          uint64_t val_hi_8_bytes,
                                          uint64_t val_lo_8_bytes);
typedef struct options_struct_S {
    uint64_t l2tcm_base;
    hmx_mac_fxp_callback_t hmx_mac_fxp_callback;
    hmx_mac_flt_callback_t hmx_mac_flt_callback;
    hmx_cvt_fxp_state_callback_t hmx_cvt_fxp_state_callback;
    hmx_cvt_state_transfer_callback_t hmx_cvt_state_transfer_callback;
    hmx_cvt_state_write_callback_t hmx_cvt_state_write_callback;
    hmx_wgt_decomp_callback_t hmx_wgt_decomp_callback;
} options_struct;

typedef struct arch_proc_opt {
    int pmu_enable;
    int xfp_inexact_enable;
    int xfp_cvt_frac;
    int xfp_cvt_int;
    uint64_t vtcm_size;
    uint64_t vtcm_offset;
    int vtcm_original_mem_entries;
    int hmxdebuglvl;
    int hmx_output_depth;
    int hmx_spatial_size;
    int hmx_channel_size;
    int hmx_block_size;
    int hmx_mxmem_debug_acc_preload;
    int hmx_mac_channels;
    FILE *hmxmpytrace;
    FILE *hmxdebugfile;
    int hmx_mxmem_debug;
    FILE *hmxaccpreloadfile;
    int hmxarray_new;
    int hmx_v1;
    int hmx_power_config;
    int hmx_8x4_mpy_mode;
    int hmx_group_conv_mode;
    int QDSP6_MX_FP_PRESENT;
    int QDSP6_MX_RATE;
    int QDSP6_MX_CHANNELS;
    int QDSP6_MX_ROWS;
    int QDSP6_MX_COLS;
    int QDSP6_MX_CVT_MPY_SZ;
    int QDSP6_MX_SUB_COLS;
    int QDSP6_MX_ACCUM_WIDTH;
    int QDSP6_MX_CVT_WIDTH;
    int QDSP6_MX_FP_RATE;
    int QDSP6_MX_FP_ACC_INT;
    int QDSP6_MX_FP_ACC_FRAC;
    int QDSP6_MX_FP_ACC_EXP;
    int QDSP6_MX_FP_ROWS;
    int QDSP6_MX_FP_COLS;
    int QDSP6_MX_FP_ACC_NORM;
    int QDSP6_MX_PARALLEL_GRPS;
    int QDSP6_MX_NUM_BIAS_GRPS;
    int QDSP6_VX_VEC_SZ;
} arch_proc_opt_t;

enum phmx_e {
  phmx_nz_act_b,
  phmx_nz_wt_b,
  phmx_array_fxp_mpy0,
  phmx_array_fxp_mpy1,
  phmx_array_fxp_mpy2,
  phmx_array_fxp_mpy3,
  phmx_array_fxp_acc,
  phmx_array_flt_mpy0,
  phmx_array_flt_mpy1,
  phmx_array_flt_mpy2,
  phmx_array_flt_mpy3,
  phmx_array_flt_acc,
  phmx_array_fxp_cvt,
  phmx_array_flt_cvt,
};

typedef struct {
    int32_t opcode;
    void *vtcm_haddr;
    hwaddr vtcm_base;
    uint32_t reg_usr;
    int32_t slot;
    int32_t page_size;
    int32_t arg1;
    int32_t arg2;
} CoprocArgs;

void hmx_coproc(CoprocArgs args);

#endif
