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

/*
 * This example tests the HVX scatter/gather instructions
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <hexagon_types.h>
#include <hexagon_protos.h>

int err;

#define VTCM_SIZE_KB        (2048)
#define VTCM_BYTES_PER_KB   (1024)

static char vtcm_buffer[VTCM_SIZE_KB * VTCM_BYTES_PER_KB]
    __attribute__((aligned(0x10000)));

/* define the number of rows/cols in a square matrix */
#define MATRIX_SIZE 64

/* define the size of the scatter buffer */
#define SCATTER_BUFFER_SIZE (MATRIX_SIZE * MATRIX_SIZE)

#define SCATTER16_BUF_SIZE  (2 * SCATTER_BUFFER_SIZE)
#define SCATTER32_BUF_SIZE  (4 * SCATTER_BUFFER_SIZE)

#define GATHER16_BUF_SIZE   (2 * MATRIX_SIZE)
#define GATHER32_BUF_SIZE   (4 * MATRIX_SIZE)

uintptr_t VTCM_BASE_ADDRESS;
uintptr_t VTCM_SCATTER16_ADDRESS;
uintptr_t VTCM_GATHER16_ADDRESS;
uintptr_t VTCM_SCATTER32_ADDRESS;
uintptr_t VTCM_GATHER32_ADDRESS;
uintptr_t VTCM_SCATTER16_32_ADDRESS;
uintptr_t VTCM_GATHER16_32_ADDRESS;

/* the vtcm base address */
unsigned char  *vtcm_base;

/* scatter gather 16 bit elements using 16 bit offsets */
unsigned short *vscatter16;
unsigned short *vgather16;
unsigned short vscatter16_ref[SCATTER_BUFFER_SIZE];
unsigned short vgather16_ref[MATRIX_SIZE];

/* scatter gather 32 bit elements using 32 bit offsets */
unsigned int   *vscatter32;
unsigned int   *vgather32;
unsigned int   vscatter32_ref[SCATTER_BUFFER_SIZE];
unsigned int   vgather32_ref[MATRIX_SIZE];

/* scatter gather 16 bit elements using 32 bit offsets */
unsigned short *vscatter16_32;
unsigned short *vgather16_32;
unsigned short vscatter16_32_ref[SCATTER_BUFFER_SIZE];
unsigned short vgather16_32_ref[MATRIX_SIZE];


/* declare the arrays of offsets */
unsigned short half_offsets[MATRIX_SIZE];
unsigned int   word_offsets[MATRIX_SIZE];

/* declare the arrays of values */
unsigned short half_values[MATRIX_SIZE];
unsigned short half_acc_values[MATRIX_SIZE];
unsigned short half_q_values[MATRIX_SIZE];
unsigned int   word_values[MATRIX_SIZE];
unsigned int   word_acc_values[MATRIX_SIZE];
unsigned int   word_q_values[MATRIX_SIZE];

/* declare the array of predicates */
unsigned short half_predicates[MATRIX_SIZE];
unsigned int word_predicates[MATRIX_SIZE];

/* make this big enough for all the intrinsics */
unsigned int region_len = 4 * SCATTER_BUFFER_SIZE - 1;

/* optionally add sync instructions */
#define SYNC_VECTOR 1

/* define a scratch area for debug and prefill */
#define SCRATCH_SIZE    0x8800

#define FILL_CHAR       '.'

/* fill vtcm scratch with ee */
void prefill_vtcm_scratch(void)
{
    memset((void *)VTCM_BASE_ADDRESS, FILL_CHAR, SCRATCH_SIZE * sizeof(char));
}

/* print vtcm scratch buffer */
void print_vtcm_scratch_16(void)
{
    unsigned short *vtmp = (unsigned short *)VTCM_BASE_ADDRESS;
    int i, j;

    printf("\n\nPrinting the vtcm scratch in half words");

    for (i = 0; i < SCRATCH_SIZE; i++) {
        if ((i % MATRIX_SIZE) == 0) {
            printf("\n");
        }
        for (j = 0; j < 2; j++) {
            printf("%c", (char)((vtmp[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
}

/* print vtcm scratch buffer */
void print_vtcm_scratch_32(void)
{
    unsigned int *vtmp = (unsigned int *)VTCM_BASE_ADDRESS;
    int i, j;

    printf("\n\nPrinting the vtcm scratch in words");

    for (i = 0; i < SCRATCH_SIZE; i++) {
        if ((i % MATRIX_SIZE) == 0) {
            printf("\n");
        }
        for (j = 0; j < 4; j++) {
            printf("%c", (char)((vtmp[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
}


/* create byte offsets to be a diagonal of the matrix with 16 bit elements */
void create_offsets_and_values_16(void)
{
    unsigned short half_element = 0;
    unsigned short half_q_element = 0;
    int i, j;
    char letter = 'A';
    char q_letter = '@';

    for (i = 0; i < MATRIX_SIZE; i++) {
        half_offsets[i] = i * (2 * MATRIX_SIZE + 2);

        half_element = 0;
        half_q_element = 0;
        for (j = 0; j < 2; j++) {
            half_element |= letter << j * 8;
            half_q_element |= q_letter << j * 8;
        }

        half_values[i] = half_element;
        half_acc_values[i] = ((i % 10) << 8) + (i % 10);
        half_q_values[i] = half_q_element;

        letter++;
        /* reset to 'A' */
        if (letter == 'M') {
            letter = 'A';
        }
    }
}

/* create a predicate mask for the half word scatter */
void create_preds_16()
{
    int i;

    for (i = 0; i < MATRIX_SIZE; i++) {
        half_predicates[i] = (i % 3 == 0 || i % 5 == 0) ? ~0 : 0;
    }
}


/* create byte offsets to be a diagonal of the matrix with 32 bit elements */
void create_offsets_and_values_32(void)
{
    unsigned int word_element = 0;
    unsigned int word_q_element = 0;
    int i, j;
    char letter = 'A';
    char q_letter = '&';

    for (i = 0; i < MATRIX_SIZE; i++) {
        word_offsets[i] = i * (4 * MATRIX_SIZE + 4);

        word_element = 0;
        word_q_element = 0;
        for (j = 0; j < 4; j++) {
            word_element |= letter << j * 8;
            word_q_element |= q_letter << j * 8;
        }

        word_values[i] = word_element;
        word_acc_values[i] = ((i % 10) << 8) + (i % 10);
        word_q_values[i] = word_q_element;

        letter++;
        /* reset to 'A' */
        if (letter == 'M') {
            letter = 'A';
        }
    }
}

/* create a predicate mask for the word scatter */
void create_preds_32()
{
    int i;

    for (i = 0; i < MATRIX_SIZE; i++) {
        word_predicates[i] = (i % 4 == 0 || i % 7 == 0) ? ~0 : 0;
    }
}


void dump_buf(char *str, void *addr, int element_size, int byte_len)
{
    unsigned short *sptr = addr;
    unsigned int *ptr = addr;
    int i;

    printf("\n\nBuffer: %s\n", str);
    for (i = 0 ; i < byte_len / element_size; ++ptr, ++sptr, ++i) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n");
        }
        if (element_size == 2) {
            printf("%c ", *sptr);
        } else if (element_size == 4) {
            printf("%4.4x ", *ptr);
        }
  }
}

/*
 * create byte offsets to be a diagonal of the matrix with 16 bit elements
 * and 32 bit offsets
 */
void create_offsets_and_values_16_32(void)
{
    unsigned int half_element = 0;
    unsigned short half_q_element = 0;
    int i, j;
    char letter = 'D';
    char q_letter = '$';

    for (i = 0; i < MATRIX_SIZE; i++) {
        word_offsets[i] = i * (2 * MATRIX_SIZE + 2);

        half_element = 0;
        half_q_element = 0;
        for (j = 0; j < 2; j++) {
            half_element |= letter << j * 8;
            half_q_element |= q_letter << j * 8;
        }

        half_values[i] = half_element;
        half_acc_values[i] = ((i % 10) << 8) + (i % 10);
        half_q_values[i] = half_q_element;

        letter++;
        /* reset to 'A' */
        if (letter == 'P') {
            letter = 'D';
        }
    }
}

void create_preds_16_32()
{
    int i;

    for (i = 0; i < MATRIX_SIZE; i++) {
        half_predicates[i] = (i % 2 == 0 || i % 13 == 0) ? ~0 : 0;
    }
}


#define SCATTER_RELEASE(ADDR) \
    asm volatile("vmem(%0 + #0):scatter_release\n" : : "r"(ADDR));

/* scatter the 16 bit elements using intrinsics */
void vector_scatter_16(void)
{
    /* copy the offsets and values to vectors */
    HVX_Vector offsets = *(HVX_Vector *)half_offsets;
    HVX_Vector values = *(HVX_Vector *)half_values;

    /* do the scatter */
    Q6_vscatter_RMVhV(VTCM_SCATTER16_ADDRESS, region_len, offsets, values);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter16);
    /*
     * This dummy load from vscatter16 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter16; vDummy = vDummy;
#endif
}

/* scatter-accumulate the 16 bit elements using intrinsics */
void vector_scatter_acc_16(void)
{
    /* copy the offsets and values to vectors */
    HVX_Vector offsets = *(HVX_Vector *)half_offsets;
    HVX_Vector values = *(HVX_Vector *)half_acc_values;

    /* do the scatter */
    Q6_vscatteracc_RMVhV(VTCM_SCATTER16_ADDRESS, region_len, offsets, values);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter16);
    /*
     * This dummy load from vscatter16 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter16; vDummy = vDummy;
#endif
}

/* scatter the 16 bit elements using intrinsics */
void vector_scatter_q_16(void)
{
    /* copy the offsets and values to vectors */
    HVX_Vector offsets = *(HVX_Vector *)half_offsets;
    HVX_Vector values = *(HVX_Vector *)half_q_values;
    HVX_Vector pred_reg = *(HVX_Vector *)half_predicates;
    HVX_VectorPred preds = Q6_Q_vand_VR(pred_reg, ~0);

    /* do the scatter */
    Q6_vscatter_QRMVhV(preds, VTCM_SCATTER16_ADDRESS, region_len, offsets,
                       values);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter16);
    /*
     * This dummy load from vscatter16 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter16; vDummy = vDummy;
#endif
}

/* scatter the 32 bit elements using intrinsics */
void vector_scatter_32(void)
{
    /* copy the offsets and values to vectors */
    HVX_Vector offsetslo = *(HVX_Vector *)word_offsets;
    HVX_Vector offsetshi = *(HVX_Vector *)&word_offsets[MATRIX_SIZE / 2];
    HVX_Vector valueslo = *(HVX_Vector *)word_values;
    HVX_Vector valueshi = *(HVX_Vector *)&word_values[MATRIX_SIZE / 2];

    /* do the scatter */
    Q6_vscatter_RMVwV(VTCM_SCATTER32_ADDRESS, region_len, offsetslo, valueslo);
    Q6_vscatter_RMVwV(VTCM_SCATTER32_ADDRESS, region_len, offsetshi, valueshi);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter32);
    /*
     * This dummy load from vscatter32 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter32; vDummy = vDummy;
#endif
}

/* scatter-acc the 32 bit elements using intrinsics */
void vector_scatter_acc_32(void)
{
    /* copy the offsets and values to vectors */
    HVX_Vector offsetslo = *(HVX_Vector *)word_offsets;
    HVX_Vector offsetshi = *(HVX_Vector *)&word_offsets[MATRIX_SIZE / 2];
    HVX_Vector valueslo = *(HVX_Vector *)word_acc_values;
    HVX_Vector valueshi = *(HVX_Vector *)&word_acc_values[MATRIX_SIZE / 2];

    /* do the scatter */
    Q6_vscatteracc_RMVwV(VTCM_SCATTER32_ADDRESS, region_len, offsetslo,
                         valueslo);
    Q6_vscatteracc_RMVwV(VTCM_SCATTER32_ADDRESS, region_len, offsetshi,
                         valueshi);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter32);
    /*
     * This dummy load from vscatter32 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter32; vDummy = vDummy;
#endif
}

/* scatter the 32 bit elements using intrinsics */
void vector_scatter_q_32(void)
{
    /* copy the offsets and values to vectors */
    HVX_Vector offsetslo = *(HVX_Vector *)word_offsets;
    HVX_Vector offsetshi = *(HVX_Vector *)&word_offsets[MATRIX_SIZE / 2];
    HVX_Vector valueslo = *(HVX_Vector *)word_q_values;
    HVX_Vector valueshi = *(HVX_Vector *)&word_q_values[MATRIX_SIZE / 2];
    HVX_Vector pred_reglo = *(HVX_Vector *)word_predicates;
    HVX_Vector pred_reghi = *(HVX_Vector *)&word_predicates[MATRIX_SIZE / 2];
    HVX_VectorPred predslo = Q6_Q_vand_VR(pred_reglo, ~0);
    HVX_VectorPred predshi = Q6_Q_vand_VR(pred_reghi, ~0);

    /* do the scatter */
    Q6_vscatter_QRMVwV(predslo, VTCM_SCATTER32_ADDRESS, region_len, offsetslo,
                       valueslo);
    Q6_vscatter_QRMVwV(predshi, VTCM_SCATTER32_ADDRESS, region_len, offsetshi,
                       valueshi);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter16);
    /*
     * This dummy load from vscatter16 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter16; vDummy = vDummy;
#endif
}

void print_vector(char *str, HVX_Vector *v)

{
    int i;
    unsigned char *ptr = (unsigned char *)v;

    printf("\n\nVector: %s\n", str);
    for (i = 0 ; i < sizeof(HVX_Vector) * 4; ++ptr, ++i) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n");
        }
        printf("%c ", *ptr);
    }
    printf("\n");
}

void print_vectorpair(char *str, HVX_VectorPair *v)

{
    int i;
    unsigned char *ptr = (unsigned char *)v;

    printf("\n\nVectorPair: %s\n", str);
    for (i = 0 ; i < sizeof(HVX_VectorPair); ++ptr, ++i) {
        if (i != 0 && (i % 16) == 0) {
            printf("\n");
        }
        printf("%c ", *ptr);
    }
    printf("\n");
}

/* scatter the 16 bit elements with 32 bit offsets using intrinsics */
void vector_scatter_16_32(void)
{
    /* get the word offsets in a vector pair */
    HVX_VectorPair offsets = *(HVX_VectorPair *)word_offsets;

    /* these values need to be shuffled for the RMWwV scatter */
    HVX_Vector values = *(HVX_Vector *)half_values;
    values = Q6_Vh_vshuff_Vh(values);

    /* do the scatter */
    Q6_vscatter_RMWwV(VTCM_SCATTER16_32_ADDRESS, region_len, offsets, values);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter16_32);
    /*
     * This dummy load from vscatter16_32 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter16_32; vDummy = vDummy;
#endif
}

/* scatter-acc the 16 bit elements with 32 bit offsets using intrinsics */
void vector_scatter_acc_16_32(void)
{
    /* get the word offsets in a vector pair */
    HVX_VectorPair offsets = *(HVX_VectorPair *)word_offsets;

    /* these values need to be shuffled for the RMWwV scatter */
    HVX_Vector values = *(HVX_Vector *)half_acc_values;
    values = Q6_Vh_vshuff_Vh(values);

    /* do the scatter */
    Q6_vscatteracc_RMWwV(VTCM_SCATTER16_32_ADDRESS, region_len, offsets,
                         values);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter16_32);
    /*
     * This dummy load from vscatter16_32 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter16_32; vDummy = vDummy;
#endif
}

/* scatter-acc the 16 bit elements with 32 bit offsets using intrinsics */
void vector_scatter_q_16_32(void)
{
    /* get the word offsets in a vector pair */
    HVX_VectorPair offsets = *(HVX_VectorPair *)word_offsets;

    /* these values need to be shuffled for the RMWwV scatter */
    HVX_Vector values = *(HVX_Vector *)half_q_values;
    values = Q6_Vh_vshuff_Vh(values);

    HVX_Vector pred_reg = *(HVX_Vector *)half_predicates;
    pred_reg = Q6_Vh_vshuff_Vh(pred_reg);
    HVX_VectorPred preds = Q6_Q_vand_VR(pred_reg, ~0);

    /* do the scatter */
    Q6_vscatter_QRMWwV(preds, VTCM_SCATTER16_32_ADDRESS, region_len, offsets,
                       values);

#if SYNC_VECTOR
    /* do the sync operation */
    SCATTER_RELEASE(vscatter16_32);
    /*
     * This dummy load from vscatter16_32 is to complete the synchronization.
     * Normally this load would be deferred as long as possible to
     * minimize stalls.
     */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vscatter16_32; vDummy = vDummy;
#endif
}

/* gather the elements from the scatter16 buffer */
void vector_gather_16(void)
{
    HVX_Vector *vgather = (HVX_Vector *)VTCM_GATHER16_ADDRESS;
    HVX_Vector offsets = *(HVX_Vector *)half_offsets;

    /* do the gather to the gather16 buffer */
    Q6_vgather_ARMVh(vgather, VTCM_SCATTER16_ADDRESS, region_len, offsets);


#if SYNC_VECTOR
    /* This dummy read of vgather will stall until completion */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vgather; vDummy = vDummy;
#endif
}

static unsigned short gather_q_16_init(void)
{
    char letter = '?';
    return letter | (letter << 8);
}

void vector_gather_q_16(void)
{
    HVX_Vector *vgather = (HVX_Vector *)VTCM_GATHER16_ADDRESS;
    HVX_Vector offsets = *(HVX_Vector *)half_offsets;
    HVX_Vector pred_reg = *(HVX_Vector *)half_predicates;
    HVX_VectorPred preds = Q6_Q_vand_VR(pred_reg, ~0);

    *vgather = Q6_Vh_vsplat_R(gather_q_16_init());
    /* do the gather to the gather16 buffer */
    Q6_vgather_AQRMVh(vgather, preds, VTCM_SCATTER16_ADDRESS, region_len,
                       offsets);

#if SYNC_VECTOR
    /* This dummy read of vgather will stall until completion */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vgather; vDummy = vDummy;
#endif
}


/* gather the elements from the scatter32 buffer */
void vector_gather_32(void)
{
    HVX_Vector *vgatherlo = (HVX_Vector *)VTCM_GATHER32_ADDRESS;
    HVX_Vector *vgatherhi =
        (HVX_Vector *)(VTCM_GATHER32_ADDRESS + (MATRIX_SIZE * 2));
    HVX_Vector offsetslo = *(HVX_Vector *)word_offsets;
    HVX_Vector offsetshi = *(HVX_Vector *)&word_offsets[MATRIX_SIZE / 2];

    /* do the gather to vgather */
    Q6_vgather_ARMVw(vgatherlo, VTCM_SCATTER32_ADDRESS, region_len, offsetslo);
    Q6_vgather_ARMVw(vgatherhi, VTCM_SCATTER32_ADDRESS, region_len, offsetshi);

#if SYNC_VECTOR
    /* This dummy read of vgatherhi will stall until completion */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vgatherhi; vDummy = vDummy;
#endif
}

static unsigned int gather_q_32_init(void)
{
    char letter = '?';
    return letter | (letter << 8) | (letter << 16) | (letter << 24);
}

void vector_gather_q_32(void)
{
    HVX_Vector *vgatherlo = (HVX_Vector *)VTCM_GATHER32_ADDRESS;
    HVX_Vector *vgatherhi =
        (HVX_Vector *)(VTCM_GATHER32_ADDRESS + (MATRIX_SIZE * 2));
    HVX_Vector offsetslo = *(HVX_Vector *)word_offsets;
    HVX_Vector offsetshi = *(HVX_Vector *)&word_offsets[MATRIX_SIZE / 2];
    HVX_Vector pred_reglo = *(HVX_Vector *)word_predicates;
    HVX_VectorPred predslo = Q6_Q_vand_VR(pred_reglo, ~0);
    HVX_Vector pred_reghi = *(HVX_Vector *)&word_predicates[MATRIX_SIZE / 2];
    HVX_VectorPred predshi = Q6_Q_vand_VR(pred_reghi, ~0);

    *vgatherlo = Q6_Vh_vsplat_R(gather_q_32_init());
    *vgatherhi = Q6_Vh_vsplat_R(gather_q_32_init());
    /* do the gather to vgather */
    Q6_vgather_AQRMVw(vgatherlo, predslo, VTCM_SCATTER32_ADDRESS, region_len,
                      offsetslo);
    Q6_vgather_AQRMVw(vgatherhi, predshi, VTCM_SCATTER32_ADDRESS, region_len,
                      offsetshi);

#if SYNC_VECTOR
    /* This dummy read of vgatherhi will stall until completion */
    volatile HVX_Vector vDummy = *(HVX_Vector *)vgatherhi; vDummy = vDummy;
#endif
}

/* gather the elements from the scatter16_32 buffer */
void vector_gather_16_32(void)
{
    /* get the vtcm address to gather from */
    HVX_Vector *vgather = (HVX_Vector *)VTCM_GATHER16_32_ADDRESS;

    /* get the word offsets in a vector pair */
    HVX_VectorPair offsets = *(HVX_VectorPair *)word_offsets;

    /* do the gather to vgather */
    Q6_vgather_ARMWw(vgather, VTCM_SCATTER16_32_ADDRESS, region_len, offsets);

    /* the read of gather will stall until completion */
    volatile HVX_Vector values = *(HVX_Vector *)vgather;

    /* deal the elements to get the order back */
    values = Q6_Vh_vdeal_Vh(values);

    /* write it back to vtcm address */
    *(HVX_Vector *)vgather = values;
}

void vector_gather_q_16_32(void)
{
    /* get the vtcm address to gather from */
    HVX_Vector *vgather = (HVX_Vector *)VTCM_GATHER16_32_ADDRESS;

    /* get the word offsets in a vector pair */
    HVX_VectorPair offsets = *(HVX_VectorPair *)word_offsets;
    HVX_Vector pred_reg = *(HVX_Vector *)half_predicates;
    pred_reg = Q6_Vh_vshuff_Vh(pred_reg);
    HVX_VectorPred preds = Q6_Q_vand_VR(pred_reg, ~0);

   *vgather = Q6_Vh_vsplat_R(gather_q_16_init());
   /* do the gather to vgather */
   Q6_vgather_AQRMWw(vgather, preds, VTCM_SCATTER16_32_ADDRESS, region_len,
                      offsets);

    /* the read of gather will stall until completion */
    volatile HVX_Vector values = *(HVX_Vector *)vgather;

    /* deal the elements to get the order back */
    values = Q6_Vh_vdeal_Vh(values);

    /* write it back to vtcm address */
    *(HVX_Vector *)vgather = values;
}


static void check_buffer(const char *name, void *c, void *r, size_t size)
{
    char *check = (char *)c;
    char *ref = (char *)r;
    int i;
    for (i = 0; i < size; i++) {
        if (check[i] != ref[i]) {
            printf("Error %s [%d]: 0x%x (%c) != 0x%x (%c)\n", name, i,
                   check[i], check[i], ref[i], ref[i]);
            err++;
        }
    }
}


/*
 * These scalar functions are the C equivalents of the vector functions that
 * use HVX
 */

/* scatter the 16 bit elements using C */
void scalar_scatter_16(unsigned short *vscatter16)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vscatter16[half_offsets[i] / 2] = half_values[i];
    }
}

void check_scatter_16()
{
    memset(vscatter16_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned short));
    scalar_scatter_16(vscatter16_ref);
    check_buffer("check_scatter_16", vscatter16, vscatter16_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned short));
}

/* scatter the 16 bit elements using C */
void scalar_scatter_acc_16(unsigned short *vscatter16)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vscatter16[half_offsets[i] / 2] += half_acc_values[i];
    }
}

/* scatter the 16 bit elements using C */
void scalar_scatter_q_16(unsigned short *vscatter16)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; i++) {
        if (half_predicates[i]) {
            vscatter16[half_offsets[i] / 2] = half_q_values[i];
        }
    }

}


void check_scatter_acc_16()
{
    memset(vscatter16_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned short));
    scalar_scatter_16(vscatter16_ref);
    scalar_scatter_acc_16(vscatter16_ref);
    check_buffer("check_scatter_acc_16", vscatter16, vscatter16_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned short));
}

void check_scatter_q_16()
{
    memset(vscatter16_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned short));
    scalar_scatter_16(vscatter16_ref);
    scalar_scatter_acc_16(vscatter16_ref);
    scalar_scatter_q_16(vscatter16_ref);
    check_buffer("check_scatter_q_16", vscatter16, vscatter16_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned short));
}

/* scatter the 32 bit elements using C */
void scalar_scatter_32(unsigned int *vscatter32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vscatter32[word_offsets[i] / 4] = word_values[i];
    }
}

/* scatter the 32 bit elements using C */
void scalar_scatter_acc_32(unsigned int *vscatter32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vscatter32[word_offsets[i] / 4] += word_acc_values[i];
    }
}

/* scatter the 32 bit elements using C */
void scalar_scatter_q_32(unsigned int *vscatter32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; i++) {
        if (word_predicates[i]) {
            vscatter32[word_offsets[i] / 4] = word_q_values[i];
        }
    }
}

void check_scatter_32()
{
    memset(vscatter32_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned int));
    scalar_scatter_32(vscatter32_ref);
    check_buffer("check_scatter_32", vscatter32, vscatter32_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned int));
}

void check_scatter_acc_32()
{
    memset(vscatter32_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned int));
    scalar_scatter_32(vscatter32_ref);
    scalar_scatter_acc_32(vscatter32_ref);
    check_buffer("check_scatter_acc_32", vscatter32, vscatter32_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned int));
}

void check_scatter_q_32()
{
    memset(vscatter32_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned int));
    scalar_scatter_32(vscatter32_ref);
    scalar_scatter_acc_32(vscatter32_ref);
    scalar_scatter_q_32(vscatter32_ref);
    check_buffer("check_scatter_q_32", vscatter32, vscatter32_ref,
                  SCATTER_BUFFER_SIZE * sizeof(unsigned int));
}

/* scatter the 32 bit elements using C */
void scalar_scatter_16_32(unsigned short *vscatter16_32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vscatter16_32[word_offsets[i] / 2] = half_values[i];
    }
}

/* scatter the 32 bit elements using C */
void scalar_scatteracc_16_32(unsigned short *vscatter16_32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vscatter16_32[word_offsets[i] / 2] += half_acc_values[i];
    }
}

void scalar_scatter_q_16_32(unsigned short *vscatter16_32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; i++) {
        if (half_predicates[i]) {
            vscatter16_32[word_offsets[i] / 2] = half_q_values[i];
        }
    }
}

void check_scatter_16_32()
{
    memset(vscatter16_32_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned short));
    scalar_scatter_16_32(vscatter16_32_ref);
    check_buffer("check_scatter_16_32", vscatter16_32, vscatter16_32_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned short));
}

void check_scatter_acc_16_32()
{
    memset(vscatter16_32_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned short));
    scalar_scatter_16_32(vscatter16_32_ref);
    scalar_scatteracc_16_32(vscatter16_32_ref);
    check_buffer("check_scatter_acc_16_32", vscatter16_32, vscatter16_32_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned short));
}

void check_scatter_q_16_32()
{
    memset(vscatter16_32_ref, FILL_CHAR,
           SCATTER_BUFFER_SIZE * sizeof(unsigned short));
    scalar_scatter_16_32(vscatter16_32_ref);
    scalar_scatteracc_16_32(vscatter16_32_ref);
    scalar_scatter_q_16_32(vscatter16_32_ref);
    check_buffer("check_scatter_q_16_32", vscatter16_32, vscatter16_32_ref,
                 SCATTER_BUFFER_SIZE * sizeof(unsigned short));
}

/* gather the elements from the scatter buffer using C */
void scalar_gather_16(unsigned short *vgather16)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vgather16[i] = vscatter16[half_offsets[i] / 2];
    }
}

void scalar_gather_q_16(unsigned short *vgather16)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        if (half_predicates[i]) {
            vgather16[i] = vscatter16[half_offsets[i] / 2];
        }
    }
}

void check_gather_16()
{
      memset(vgather16_ref, 0, MATRIX_SIZE * sizeof(unsigned short));
      scalar_gather_16(vgather16_ref);
      check_buffer("check_gather_16", vgather16, vgather16_ref,
                   MATRIX_SIZE * sizeof(unsigned short));
}

void check_gather_q_16()
{
    memset(vgather16_ref, gather_q_16_init(),
           MATRIX_SIZE * sizeof(unsigned short));
    scalar_gather_q_16(vgather16_ref);
    check_buffer("check_gather_q_16", vgather16, vgather16_ref,
                 MATRIX_SIZE * sizeof(unsigned short));
}

/* gather the elements from the scatter buffer using C */
void scalar_gather_32(unsigned int *vgather32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vgather32[i] = vscatter32[word_offsets[i] / 4];
    }
}

void scalar_gather_q_32(unsigned int *vgather32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        if (word_predicates[i]) {
            vgather32[i] = vscatter32[word_offsets[i] / 4];
        }
    }
}


void check_gather_32(void)
{
    memset(vgather32_ref, 0, MATRIX_SIZE * sizeof(unsigned int));
    scalar_gather_32(vgather32_ref);
    check_buffer("check_gather_32", vgather32, vgather32_ref,
                 MATRIX_SIZE * sizeof(unsigned int));
}

void check_gather_q_32(void)
{
    memset(vgather32_ref, gather_q_32_init(),
           MATRIX_SIZE * sizeof(unsigned int));
    scalar_gather_q_32(vgather32_ref);
    check_buffer("check_gather_q_32", vgather32,
                 vgather32_ref, MATRIX_SIZE * sizeof(unsigned int));
}

/* gather the elements from the scatter buffer using C */
void scalar_gather_16_32(unsigned short *vgather16_32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        vgather16_32[i] = vscatter16_32[word_offsets[i] / 2];
    }
}

void scalar_gather_q_16_32(unsigned short *vgather16_32)
{
    int i;

    for (i = 0; i < MATRIX_SIZE; ++i) {
        if (half_predicates[i]) {
            vgather16_32[i] = vscatter16_32[word_offsets[i] / 2];
        }
    }

}

void check_gather_16_32(void)
{
    memset(vgather16_32_ref, 0, MATRIX_SIZE * sizeof(unsigned short));
    scalar_gather_16_32(vgather16_32_ref);
    check_buffer("check_gather_16_32", vgather16_32, vgather16_32_ref,
                 MATRIX_SIZE * sizeof(unsigned short));
}

void check_gather_q_16_32(void)
{
    memset(vgather16_32_ref, gather_q_16_init(),
           MATRIX_SIZE * sizeof(unsigned short));
    scalar_gather_q_16_32(vgather16_32_ref);
    check_buffer("check_gather_q_16_32", vgather16_32, vgather16_32_ref,
                 MATRIX_SIZE * sizeof(unsigned short));
}

/* These functions print the buffers to the display */

/* print scatter16 buffer */
void print_scatter16_buffer(void)
{
    printf("\n\nPrinting the 16 bit scatter buffer");
    int i, j;

    for (i = 0; i < SCATTER_BUFFER_SIZE; i++) {
        if ((i % MATRIX_SIZE) == 0) {
            printf("\n");
        }
        for (j = 0; j < 2; j++) {
            printf("%c", (char)((vscatter16[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
    printf("\n");
}

/* print the gather 16 buffer */
void print_gather_result_16(void)
{
    printf("\n\nPrinting the 16 bit gather result\n");
    int i, j;

    for (i = 0; i < MATRIX_SIZE; i++) {
        for (j = 0; j < 2; j++) {
            printf("%c", (char)((vgather16[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
    printf("\n");
}

/* print the scatter32 buffer */
void print_scatter32_buffer(void)
{
    printf("\n\nPrinting the 32 bit scatter buffer");
    int i, j;

    for (i = 0; i < SCATTER_BUFFER_SIZE; i++) {
        if ((i % MATRIX_SIZE) == 0) {
            printf("\n");
        }
        for (j = 0; j < 4; j++) {
            printf("%c", (char)((vscatter32[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
    printf("\n");
}


/* print the gather 32 buffer */
void print_gather_result_32(void)
{
    printf("\n\nPrinting the 32 bit gather result\n");
    int i, j;

    for (i = 0; i < MATRIX_SIZE; i++) {
        for (j = 0; j < 4; j++) {
            printf("%c", (char)((vgather32[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
    printf("\n");
}

/* print the scatter16_32 buffer */
void print_scatter16_32_buffer(void)
{
    printf("\n\nPrinting the 16_32 bit scatter buffer");
    int i, j;

    for (i = 0; i < SCATTER_BUFFER_SIZE; i++) {
        if ((i % MATRIX_SIZE) == 0) {
            printf("\n");
        }
        for (j = 0; j < 2; j++) {
            printf("%c", (unsigned char)((vscatter16_32[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
    printf("\n");
}

/* print the gather 16_32 buffer */
void print_gather_result_16_32(void)
{
    printf("\n\nPrinting the 16_32 bit gather result\n");
    int i, j;

    for (i = 0; i < MATRIX_SIZE; i++) {
        for (j = 0; j < 2; j++) {
            printf("%c", (unsigned char)((vgather16_32[i] >> j * 8) & 0xff));
        }
        printf(" ");
    }
    printf("\n");
}

static uintptr_t get_vtcm_base(void)
{
    return (uintptr_t)vtcm_buffer;
}

/*
 * set up the tcm address translation
 * Note: This method is only for the standalone environment
 * SDK users should use the "VTCM Manager" to use VTCM
 */
void setup_tcm(void)
{
    VTCM_BASE_ADDRESS = get_vtcm_base();

    VTCM_SCATTER16_ADDRESS = VTCM_BASE_ADDRESS;
    VTCM_GATHER16_ADDRESS = VTCM_BASE_ADDRESS + SCATTER16_BUF_SIZE;
    VTCM_SCATTER32_ADDRESS = VTCM_GATHER16_ADDRESS + GATHER16_BUF_SIZE;
    VTCM_GATHER32_ADDRESS = VTCM_SCATTER32_ADDRESS + SCATTER32_BUF_SIZE;
    VTCM_SCATTER16_32_ADDRESS = VTCM_GATHER32_ADDRESS + GATHER32_BUF_SIZE;
    VTCM_GATHER16_32_ADDRESS = VTCM_SCATTER16_32_ADDRESS + SCATTER16_BUF_SIZE;

    /* the vtcm base address */
    vtcm_base = (unsigned char  *)VTCM_BASE_ADDRESS;

    /* scatter gather 16 bit elements using 16 bit offsets */
    vscatter16 = (unsigned short *)VTCM_SCATTER16_ADDRESS;
    vgather16 = (unsigned short *)VTCM_GATHER16_ADDRESS;

    /* scatter gather 32 bit elements using 32 bit offsets */
    vscatter32 = (unsigned int   *)VTCM_SCATTER32_ADDRESS;
    vgather32 = (unsigned int   *)VTCM_GATHER32_ADDRESS;

    /* scatter gather 16 bit elements using 32 bit offsets */
    vscatter16_32 = (unsigned short *)VTCM_SCATTER16_32_ADDRESS;
    vgather16_32 = (unsigned short *)VTCM_GATHER16_32_ADDRESS;
}


int main()
{
    setup_tcm();

    prefill_vtcm_scratch();

    /* 16 bit elements with 16 bit offsets */
    create_offsets_and_values_16();
    create_preds_16();

    vector_scatter_16();
#if PRINT_DATA
    print_scatter16_buffer();
#endif
    check_scatter_16();

    vector_gather_16();
#if PRINT_DATA
    print_gather_result_16();
#endif
    check_gather_16();

    vector_gather_q_16();
#if PRINT_DATA
    print_gather_result_16();
#endif
    check_gather_q_16();

    vector_scatter_acc_16();
#if PRINT_DATA
    print_scatter16_buffer();
#endif
    check_scatter_acc_16();

    vector_scatter_q_16();
#if PRINT_DATA
    print_scatter16_buffer();
#endif
    check_scatter_q_16();

    /* 32 bit elements with 32 bit offsets */
    create_offsets_and_values_32();
    create_preds_32();

    vector_scatter_32();
#if PRINT_DATA
    print_scatter32_buffer();
#endif
    check_scatter_32();


    vector_gather_32();
#if PRINT_DATA
    print_gather_result_32();
#endif
    check_gather_32();

    vector_gather_q_32();
#if PRINT_DATA
    print_gather_result_32();
#endif
    check_gather_q_32();

    vector_scatter_acc_32();
#if PRINT_DATA
    print_scatter32_buffer();
#endif
    check_scatter_acc_32();

    vector_scatter_q_32();
#if PRINT_DATA
    print_scatter32_buffer();
#endif
    check_scatter_q_32();

    /* 16 bit elements with 32 bit offsets */
    create_offsets_and_values_16_32();
    create_preds_16_32();

    vector_scatter_16_32();
#if PRINT_DATA
    print_scatter16_32_buffer();
#endif
    check_scatter_16_32();

    vector_gather_16_32();
#if PRINT_DATA
    print_gather_result_16_32();
#endif
    check_gather_16_32();


    vector_gather_q_16_32();
#if PRINT_DATA
    print_gather_result_16_32();
#endif
    check_gather_q_16_32();

    vector_scatter_acc_16_32();
#if PRINT_DATA
    print_scatter16_32_buffer();
#endif
    check_scatter_acc_16_32();

    vector_scatter_q_16_32();
#if PRINT_DATA
    print_scatter16_32_buffer();
#endif
    check_scatter_q_16_32();


    puts(err ? "FAIL" : "PASS");
    return err;
}

