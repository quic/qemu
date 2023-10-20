/*
 * Dummy mutex functions for when we are in single thread user space
 */

        .p2align 4
        .global __lockMutex
        .set    __lockMutex, lockMutex
        .type   lockMutex, @function
        .global __sys_Mtxlock
        .set    __sys_Mtxlock, sys_Mtxlock
        .global lockMutex
        .global sys_Mtxlock
lockMutex:
sys_Mtxlock:
        jumpr lr
.size lockMutex, . - lockMutex


        .p2align 4
        .global unlockMutex
        .global __unlockMutex
        .type unlockMutex, @function
        .set __unlockMutex, unlockMutex
        .global sys_Mtxunlock
        .set sys_Mtxunlock, unlockMutex
        .global __sys_Mtxunlock
        .set __sys_Mtxunlock, unlockMutex
unlockMutex:
        jumpr lr
.size unlockMutex, . - unlockMutex


        .p2align 4
        .global __sys_Mtxinit
        .global sys_Mtxinit
        .type sys_Mtxinit, @function
        .set __sys_Mtxinit, sys_Mtxinit
sys_Mtxinit:
        {
          r1 = #0
          memw (r0) = r1.new
          jumpr lr
        }
.size sys_Mtxinit, . - sys_Mtxinit


        .p2align 4
        .global __sys_Mtxdst
        .global sys_Mtxdst
        .type sys_Mtxdst, @function
        .set __sys_Mtxdst, sys_Mtxdst
sys_Mtxdst:
        jumpr lr
        .size sys_Mtxdst, . - sys_Mtxdst


        .p2align 4
        .global trylockMutex
        .type trylockMutex, @function
        .global __trylockMutex
        .set __trylockMutex, trylockMutex
trylockMutex:
        {
          r0 = #1
          jumpr lr
        }
.size trylockMutex, . - trylockMutex
