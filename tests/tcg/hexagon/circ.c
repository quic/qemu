/*
 *  Copyright(c) 2019-2021 Qualcomm Innovation Center, Inc. All Rights Reserved.
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
 * This example shows how to perform circular addressing from C using
 * inline assembly.
 */

#include <stdio.h>
#ifdef __HEXMSGABI_3_SUPPORTED__

#define DEBUG          0

#define NBYTES         (1 << 8)
#define NHALFS         (NBYTES / sizeof(short))
#define NWORDS         (NBYTES / sizeof(int))
#define NDOBLS         (NBYTES / sizeof(long long))

long long     dbuf[NDOBLS] __attribute__((aligned(1 << 12))) = {0};
int           wbuf[NWORDS] __attribute__((aligned(1 << 12))) = {0};
short         hbuf[NHALFS] __attribute__((aligned(1 << 12))) = {0};
unsigned char bbuf[NBYTES] __attribute__((aligned(1 << 12))) = {0};

/*
 * Macros for performing circular load
 *     RES         result
 *     ADDR        address
 *     START       start address of buffer
 *     LEN         length of buffer (in bytes)
 *     INC         address increment (in bytes for IMM, elements for REG)
 */
#define CIRC_LOAD_W_IMM(RES, ADDR, START, LEN, INC) \
    __asm__( \
      "r4 = %4\n\t" \
      "m0 = r4\n\t" \
      "cs0 = %3\n\t" \
      "%0 = memw(%1++#" #INC ":circ(M0))\n\t" \
      : "=r"(RES), "=r"(ADDR) \
      : "1"(ADDR), "r"(START), "r"(LEN) \
      : "r4", "m0", "cs0")
#define CIRC_LOAD_W_REG(RES, ADDR, START, LEN, INC) \
    __asm__( \
      "r4 = %3\n\t" \
      "m1 = r4\n\t" \
      "cs1 = %4\n\t" \
      "%0 = memw(%1++I:circ(M1))\n\t" \
      : "=r"(RES), "=r"(ADDR) \
      : "1"(ADDR), "r"((((INC) & 0x7f) << 17) | ((LEN) & 0x1ffff)), "r"(START) \
      : "r4", "m1", "cs1")
#define CIRC_LOAD_UBH_REG(RES, ADDR, START, LEN, INC) \
    __asm__( \
      "r4 = %3\n\t" \
      "m1 = r4\n\t" \
      "cs1 = %4\n\t" \
      "%0 = memubh(%1++I:circ(M1))\n\t" \
      : "=r"(RES), "=r"(ADDR) \
      : "1"(ADDR), "r"((((INC) & 0x7f) << 17) | ((LEN) & 0x1ffff)), "r"(START) \
      : "r4", "m1", "cs1")
#define CIRC_LOAD_D_IMM(RES, ADDR, START, LEN, INC) \
    __asm__( \
      "r4 = %4\n\t" \
      "m0 = r4\n\t" \
      "cs0 = %3\n\t" \
      "%0 = memd(%1++#" #INC ":circ(M0))\n\t" \
      : "=r"(RES), "=r"(ADDR) \
      : "1"(ADDR), "r"(START), "r"(LEN) \
      : "r4", "m0", "cs0")

/*
 * Macros for performing circular store
 *     VAL         value to store
 *     ADDR        address
 *     START       start address of buffer
 *     LEN         length of buffer (in bytes)
 *     INC         address increment (in bytes for IMM, elements for REG)
 */
#define CIRC_STORE_B_IMM(VAL, ADDR, START, LEN, INC) \
    __asm__( \
      "r4 = %4\n\t" \
      "m0 = r4\n\t" \
      "cs0 = %2\n\t" \
      "memb(%0++#" #INC ":circ(M0)) = %3\n\t" \
      : "=r"(ADDR) \
      : "0"(ADDR), "r"(START), "r"(VAL), "r"(LEN) \
      : "r4", "m0", "cs0", "memory")
#define CIRC_STORE_H_REG(VAL, ADDR, START, LEN, INC) \
    __asm__( \
      "r4 = %2\n\t" \
      "m1 = r4\n\t" \
      "cs1 = %3\n\t" \
      "memh(%0++I:circ(M1)) = %4\n\t" \
      : "=r"(ADDR) \
      : "0"(ADDR), \
        "r"((((INC) & 0x7f) << 17) | ((LEN) & 0x1ffff)), \
        "r"(START), \
        "r"(VAL) \
      : "r4", "m1", "cs1", "memory")
#define CIRC_STORE_WNEW_REG(VAL, ADDR, START, LEN, INC) \
    __asm__( \
      "r4 = %2\n\t" \
      "m1 = r4\n\t" \
      "cs1 = %3\n\t" \
      "{\n\t" \
          "r5 = %4\n\t" \
          "memw(%0++I:circ(M1)) = r5.new\n\t" \
      "}\n\t" \
      : "=r"(ADDR) \
      : "0"(ADDR), \
        "r"((((INC) & 0x7f) << 17) | ((LEN) & 0x1ffff)), \
        "r"(START), \
        "r"(VAL) \
      : "r4", "r5", "m1", "cs1", "memory")


int err;

void check_load(int i, long long result, int mod)
{
  if (result != (i % mod)) {
    printf("ERROR(%d): %lld != %d\n", i, result, i % mod);
    err++;
  }
}

void circ_test_load_d_imm(void)
{
  long long *p = dbuf;
  int size = 7;
  int i;
  for (i = 0; i < NDOBLS; i++) {
    long long element;
    CIRC_LOAD_D_IMM(element, p, dbuf, size * sizeof(long long), 8);
#if DEBUG
    printf("i = %2d, p = 0x%p, element = %lld\n", i, p, element);
#endif
    check_load(i, element, size);
  }

}

void circ_test_load_w_imm(void)
{
  int *p = wbuf;
  int size = 10;
  int i;
  for (i = 0; i < NWORDS; i++) {
    int element;
    CIRC_LOAD_W_IMM(element, p, wbuf, size * sizeof(int), 4);
#if DEBUG
    printf("i = %2d, p = 0x%p, element = %2d\n", i, p, element);
#endif
    check_load(i, element, size);
  }
}

void circ_test_load_w_reg(void)
{
  int *p = wbuf;
  int size = 13;
  int i;
  for (i = 0; i < NWORDS; i++) {
    int element;
    CIRC_LOAD_W_REG(element, p, wbuf, size * sizeof(int), 1);
#if DEBUG
    printf("i = %2d, p = 0x%p, element = %2d\n", i, p, element);
#endif
    check_load(i, element, size);
  }
}

static void check_ubh(int i, int result, int mod)
{
  int x = 2 * i % mod;
  int expect = ((x + 1) << 16) | x;
  if (result != expect) {
    printf("ERROR(%d): %08x != %08x\n", i, result, expect);
    err++;
  }
}

void circ_test_load_ubh_reg(void)
{
  unsigned char *p = bbuf;
  int size = 88;
  int i;
  for (i = 0; i < NBYTES; i++) {
    int element;
    CIRC_LOAD_UBH_REG(element, p, bbuf, size, 1);
#if DEBUG
    printf("i = %3d, p = 0x%p, element = 0x%08x\n", i, p, element);
#endif
    check_ubh(i, element, size);
  }
}

unsigned char circ_val_b(int i, int size)
{
  int mod = NBYTES % size;
  if (i < mod) {
    return (i + NBYTES - mod) & 0xff;
  } else {
    return (i + NBYTES - size - mod) & 0xff;
  }
}

void check_store_b(int size)
{
  int i;
  for (i = 0; i < size; i++) {
#if DEBUG
    printf("bbuf[%3d] = 0x%02x, guess = 0x%02x\n",
           i, bbuf[i], circ_val_b(i, size));
#endif
    if (bbuf[i] != circ_val_b(i, size)) {
        printf("ERROR(%3d): 0x%02x != 0x%02x\n",
               i, bbuf[i], circ_val_b(i, size));
        err++;
    }
  }
  for (i = size; i < NBYTES; i++) {
    if (bbuf[i] != i) {
        printf("ERROR(%3d): 0x%02x != 0x%02x\n", i, bbuf[i], i);
        err++;
    }
  }
}

void circ_test_store_b_imm(void)
{
  unsigned int size = 59;
  unsigned char *p = bbuf;
  unsigned int val = 0;
  int i;
  for (i = 0; i < NBYTES; i++) {
    CIRC_STORE_B_IMM(val & 0xff, p, bbuf, size, 1);
    val++;
  }
  check_store_b(size);
}

short circ_val_h(int i, int size)
{
  int mod = NHALFS % size;
  if (i < mod) {
    return (i + NHALFS - mod) & 0xffff;
  } else {
    return (i + NHALFS - size - mod) & 0xffff;
  }
}

void check_store_h(int size)
{
  int i;
  for (i = 0; i < size; i++) {
#if DEBUG
    printf("hbuf[%3d] = 0x%02x, guess = 0x%02x\n",
           i, hbuf[i], circ_val_h(i, size));
#endif
    if (hbuf[i] != circ_val_h(i, size)) {
        printf("ERROR(%3d): 0x%02x != 0x%02x\n",
               i, hbuf[i], circ_val_h(i, size));
        err++;
    }
  }
  for (i = size; i < NHALFS; i++) {
    if (hbuf[i] != i) {
        printf("ERROR(%3d): 0x%02x != 0x%02x\n", i, hbuf[i], i);
        err++;
    }
  }
}

void circ_test_store_h_reg(void)
{
  short *p = hbuf;
  unsigned int size = 44;
  unsigned int val = 0;
  int i;
  for (i = 0; i < NHALFS; i++) {
    CIRC_STORE_H_REG(val & 0xffff, p, hbuf, size * sizeof(short), 1);
    val++;
  }
  check_store_h(size);
}

int circ_val_w(int i, int size)
{
  int mod = NWORDS % size;
  if (i < mod) {
    return i + NWORDS - mod;
  } else {
    return i + NWORDS - size - mod;
  }
}

void check_store_w(int size)
{
  int i;
  for (i = 0; i < size; i++) {
#if DEBUG
    printf("wbuf[%3d] = 0x%02x, guess = 0x%02x\n",
           i, wbuf[i], circ_val_w(i, size));
#endif
    if (wbuf[i] != circ_val_w(i, size)) {
        printf("ERROR(%3d): 0x%02x != 0x%02x\n",
               i, wbuf[i], circ_val_w(i, size));
        err++;
    }
  }
  for (i = size; i < NWORDS; i++) {
    if (wbuf[i] != i) {
        printf("ERROR(%3d): 0x%02x != 0x%02x\n", i, wbuf[i], i);
        err++;
    }
  }
}

void circ_test_store_wnew_reg(void)
{
  unsigned int size = 27;
  int *p = wbuf;
  unsigned int val = 0;
  int i;
  for (i = 0; i < NWORDS; i++) {
    CIRC_STORE_WNEW_REG(val, p, wbuf, size * sizeof(int), 1);
    val++;
  }
  check_store_w(size);
}


int main()
{
  int i;
  for (i = 0; i < NDOBLS; i++) {
    dbuf[i] = i;
  }
  for (i = 0; i < NWORDS; i++) {
    wbuf[i] = i;
  }
  for (i = 0; i < NHALFS; i++) {
    hbuf[i] = i;
  }
  for (i = 0; i < NBYTES; i++) {
    bbuf[i] = i;
  }

#if DEBUG
  printf("NBYTES = %d\n", NBYTES);
  printf("Address of dbuf = 0x%p\n", dbuf);
  printf("Address of wbuf = 0x%p\n", wbuf);
  printf("Address of hbuf = 0x%p\n", hbuf);
  printf("Address of bbuf = 0x%p\n", bbuf);
#endif
  circ_test_load_w_imm();
  circ_test_load_d_imm();
  circ_test_load_w_reg();
  circ_test_load_ubh_reg();

  circ_test_store_b_imm();
  circ_test_store_h_reg();
  circ_test_store_wnew_reg();

  puts(err ? "FAIL" : "PASS");
  return 0;
}
#else
int main () {puts ("NOT RUN"); return 0;}
#endif
