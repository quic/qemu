#include <stdio.h>

void inst_test()
{
    asm volatile("dczeroa(r0)\n\t"
                 "dccleanidx(r0)\n\t"
                 "dcinvidx(r0)\n\t"
                 "r1 = dctagr(r0)\n\t"
                 "dctagw(r0, r1)\n\t"
                 "dcfetch(r0)\n\t"
                 "dccleaninvidx(r0)\n\t"
                 "trace(r0)\n\t"
                 "pause(#1)\n\t");
    printf("Executed monitor mode instructions\n");
}

int main(int argc, const char *argv[])
{
    inst_test();
    printf("Hello, World: (argc: %d)\n", argc);
    for (int i = 0; i < argc; i++) {
        printf("\t> '%s'\n", argv[i]);
    }
}
