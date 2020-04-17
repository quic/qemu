	.text
	.global __start
__start:
    allocframe(r29,#0):raw
    r0=#0x0200
    evb=r0
    dealloc_return

	/* Output a single character to serial port */
	.global __sys_outc
__sys_outc:
	{
        r0=#0x03
        r1=memb(sp)
	}
	{
        trap0(#0)
	}

	.org 0x0200

 	.type .EventVectors, @function

.EventVectors:
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte
	rte

	.size .EventVectors, . - .EventVectors
