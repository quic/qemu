#include <stdio.h>

/*
 *  Force an unaligned load
 */
static inline int unaligned_load(void *p)
{
  int ret;
  asm volatile("%0 = memw(%1)\n\t"
               :"=r"(ret)
               : "r"(p)
               );
  return ret;
}

int array[2] = { 0xf7e6d5c4, 0xb3a29180 };

int main()
{
    int x = unaligned_load(((char*)array) + 1);
    printf("x = 0x%08x\n", x);
    return 0;
}
