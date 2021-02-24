#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define HEXAGON_UDMA_DM0_STATUS_IDLE             0x00000000
#define HEXAGON_UDMA_DM0_STATUS_RUN              0x00000001
#define HEXAGON_UDMA_DM0_STATUS_ERROR            0x00000002
#define HEXAGON_UDMA_DESC_DSTATE_INCOMPLETE      0
#define HEXAGON_UDMA_DESC_DSTATE_COMPLETE        1
#define HEXAGON_UDMA_DESC_ORDER_NOORDER          0
#define HEXAGON_UDMA_DESC_ORDER_ORDER            1
#define HEXAGON_UDMA_DESC_BYPASS_OFF             0
#define HEXAGON_UDMA_DESC_BYPASS_ON              1
#define HEXAGON_UDMA_DESC_COMP_NONE              0
#define HEXAGON_UDMA_DESC_COMP_DLBC              1
#define HEXAGON_UDMA_DESC_DESCTYPE_TYPE0         0
#define HEXAGON_UDMA_DESC_DESCTYPE_TYPE1         1

typedef struct hexagon_udma_descriptor_type0_s
{
    void *next;
    unsigned int length:24;
    unsigned int desctype:2;
    unsigned int dstcomp:1;
    unsigned int srccomp:1;
    unsigned int dstbypass:1;
    unsigned int srcbypass:1;
    unsigned int order:1;
    unsigned int dstate:1;
    void *src;
    void *dst;
} hexagon_udma_descriptor_type0_t;

typedef struct hexagon_udma_descriptor_type1_s
{
    void *next;
    unsigned int length:24;
    unsigned int desctype:2;
    unsigned int dstcomp:1;
    unsigned int srccomp:1;
    unsigned int dstbypass:1;
    unsigned int srcbypass:1;
    unsigned int order:1;
    unsigned int dstate:1;
    void *src;
    void *dst;
    unsigned int allocation:28;
    unsigned int padding:4;
    unsigned int roiwidth:16;
    unsigned int roiheight:16;
    unsigned int srcstride:16;
    unsigned int dststride:16;
    unsigned int srcwidthoffset:16;
    unsigned int dstwidthoffset:16;
} hexagon_udma_descriptor_type1_t;

#define WINDOW_X_SIZE 1024
#define WINDOW_Y_SIZE 1024

#define FULL_SIZE     (WINDOW_Y_SIZE * WINDOW_X_SIZE)
#define HALF_SIZE     (FULL_SIZE / 2)
#define QUARTER_SIZE  (FULL_SIZE / 4)
#define EIGHTH_SIZE   (FULL_SIZE / 8)
#define DMA_XFER_SIZE EIGHTH_SIZE

#define asm_dmastart()    \
    __asm__ __volatile__ (  \
    "    r0 = %0\n"   \
    "    dmstart(r0)\n"    \
    : : "r" (desc0_1) : "r0"      \
    );

void do_dmastart(void *desc0_1)

{
  asm_dmastart();
}


hexagon_udma_descriptor_type0_t fill_descriptor0(
  void *src,
  void *dst,
  int length,
  hexagon_udma_descriptor_type0_t *next)

{
  hexagon_udma_descriptor_type0_t desc0;

  memset(&desc0, 0, sizeof(hexagon_udma_descriptor_type0_t));
  desc0.next = next;
  desc0.order = HEXAGON_UDMA_DESC_ORDER_NOORDER;
  desc0.srcbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
  desc0.dstbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
  desc0.srccomp = HEXAGON_UDMA_DESC_COMP_NONE;
  desc0.dstcomp = HEXAGON_UDMA_DESC_COMP_NONE;
  desc0.desctype = HEXAGON_UDMA_DESC_DESCTYPE_TYPE0;
  desc0.length = length;
  desc0.src = src;
  desc0.dst = dst;
  printf("fill desc: src %p, dst %p, len %u, next %p\n", src, dst, length, next);

  return desc0;
}

hexagon_udma_descriptor_type1_t fill_descriptor1(
  void *src,
  void *dst,
  int length,
  int roiheight,
  int roiwidth,
  int src_stride,
  int dst_stride,
  int src_wo,
  int dst_wo,
  hexagon_udma_descriptor_type1_t *next)

{
  hexagon_udma_descriptor_type1_t desc1;

  memset(&desc1, 0, sizeof(hexagon_udma_descriptor_type1_t));
  desc1.next = next;
  desc1.order = HEXAGON_UDMA_DESC_ORDER_NOORDER;
  desc1.srcbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
  desc1.dstbypass = HEXAGON_UDMA_DESC_BYPASS_OFF;
  desc1.srccomp = HEXAGON_UDMA_DESC_COMP_NONE;
  desc1.dstcomp = HEXAGON_UDMA_DESC_COMP_NONE;
  desc1.desctype = HEXAGON_UDMA_DESC_DESCTYPE_TYPE1;
  desc1.length = length;
  desc1.roiwidth = roiwidth;
  desc1.roiheight = roiheight;
  desc1.srcstride = src_stride;
  desc1.dststride = dst_stride;
  desc1.dstwidthoffset = dst_wo;
  desc1.srcwidthoffset = src_wo;
  desc1.src = src;
  desc1.dst = dst;
  printf("fill desc: src %p, dst %p, len %u, next %p\n", src, dst, length, next);

  return desc1;
}

#define ALIGN       (1024 * 32)
#define DESC_ALIGN  (1024 * 8)

hexagon_udma_descriptor_type0_t *alloc_descriptor()

{
  uint8_t *ptr;
  ptr = calloc(DESC_ALIGN*2, 1);
  ptr += DESC_ALIGN;
  ptr = (unsigned char *)((uintptr_t)ptr & (~(DESC_ALIGN - 1)));
  printf("desc0_1 at 0x%p\n", ptr);
  return (hexagon_udma_descriptor_type0_t *)ptr;
}


int main(int argc, char **argv)

{
  const char *ofname = "memory.dat";
  unsigned char *memory;
  int do_file_op = 0;

  if (argc == 2 && strcasecmp(argv[1], "-file") == 0)
    do_file_op = 1;

  if (do_file_op)
    (void)remove(ofname);

  /* allocate, align and init data area */
  memory = calloc(FULL_SIZE+ALIGN, 1);
  if (!memory) {
    printf("out of memory: data area\n");
    exit(-2);
  }
  memory += ALIGN;
  memory = (unsigned char *)((uintptr_t)memory & (~(ALIGN - 1)));
  unsigned char *src1 = memory;
  unsigned char *src2 = memory + QUARTER_SIZE;
  memset(src1, 0xAA, DMA_XFER_SIZE);  /* fill source memory area 1 */
  memset(src2, 0xBB, DMA_XFER_SIZE);  /* fill source memory area 2 : different value */
  printf("calloc: memory at %p: src1 %p: src2 %p\n", memory, src1, src2);

  /* now allocate and init descriptors */
  hexagon_udma_descriptor_type0_t *desc0_1, *desc0_2;
  desc0_1 = alloc_descriptor();
  desc0_2 = alloc_descriptor();
  if (!desc0_1 || !desc0_2) {
    printf("out of memory: descriptors\n");
    exit(-2);
  }
  printf("aligned:\n\tdesc0_1 at %p, desc0_2 at %p\n", desc0_1, desc0_2);
  unsigned char *dst1 = memory + HALF_SIZE;
  unsigned char *dst2 = memory + HALF_SIZE + QUARTER_SIZE;
  *desc0_1 = fill_descriptor0(src1, dst1, DMA_XFER_SIZE, desc0_2);  /* chain two descriptors together */
  *desc0_2 = fill_descriptor0(src2, dst2, DMA_XFER_SIZE, NULL);     /* end of chain */

  /* kick off dma */
  do_dmastart(desc0_1);

  /* validate transfer is correct */
  int fail = 0;
  if (memcmp(src1, dst1, DMA_XFER_SIZE) != 0) {
    printf("first dma transfer failed\n");
    fail = 1;
  }
  if (memcmp(src2, dst2, DMA_XFER_SIZE) != 0) {
    printf("second dma transfer failed\n");
    fail = 1;
  }
  if (fail) {
    printf("udma test: FAIL\n");
    exit(-3);
  }
  else
    printf("udma test: PASS\n");

  /* write memory to output file */
  if (do_file_op) {
    int ofno;

    printf("writing to: %s\n", ofname);
    ofno = open(ofname, O_CREAT | O_TRUNC | O_WRONLY, S_IRWXU | S_IRWXG | S_IRWXO);
    if (!ofno) {
      printf("can't open file: %s\n", ofname);
      exit(-4);
    }

    if (write(ofno, memory, FULL_SIZE) != FULL_SIZE) {
      printf("can't write file: %s\n", ofname);
      exit(-4);
    }
    close(ofno);
    printf("%s created successfully!\n", ofname);
  }

  exit(0);
}

