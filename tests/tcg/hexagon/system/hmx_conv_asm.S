//void hmx_conv_asm(unsigned char * out, int output_shift, unsigned char * activations_start, unsigned int activations_range , unsigned char * weights_start, unsigned int weights_range)

// Macros for "hidden" opcodes that print register contents 
// Matrix Multiply opcode macros until assembler works

#define PV32(VSRC) .word (0x1DFFE020 +VSRC)	 
#define PV8(VSRC) .word (0x1DFFE160 +VSRC)
#define PV8D(VSRC) .word (0x1DFFE0A0 +VSRC)

#define PVACC .word (0x9200CFFF)
#define PR(SRC) .word (0x1DFFE100 + SRC)


//10010010000000000000000011100100
#define MX_ACT(RS,RT) .word (0x920040E4 + (RS<<16) + (RT<<8))
//10010010000000000010000011100000
#define MX_WEI(RS,RT) .word (0x9200E0E0 + (RS<<16) + (RT<<8))

//////////////////////////////////////////////////////////////////////////////////////////////


   .text
   .falign
   .globl hmx_conv_spatial_major_asm
   .type   hmx_conv_spatial_major_asm, @function
   
hmx_conv_spatial_major_asm:
	{	R3:2 = memd(R0+#0); // Activation Range, Start, 
		R5:4 = memd(R0+#8); // Weights Range, Start, 
	}
	{	R7:6 = memd(R0+#16)	// cvt_range, Output Address
		R8 = memw(R0+#24)	// bias
	}
	{	mxclracc
	}
	{ 	bias = mxmem(R8); 
	}
	{ 	activation.ub = mxmem(R2,R3);
		weight.b = mxmem(R4,R5);
	}
	{ 	mxmem(R6,R7):before:retain:sat.ub=acc;
	}
	{ 	mxmem(R6,R7):after:sat.ub=acc;
	}
	r0 = memb(r6);
	jumpr r31;

.size hmx_conv_spatial_major_asm, .-hmx_conv_spatial_major_asm
	
	
   .text
   .falign
   .globl hmx_conv_spatial_major_deep_asm
   .type   hmx_conv_spatial_major_deep_asm, @function
   
hmx_conv_spatial_major_deep_asm:
	{	R3:2 = memd(R0+#0); // Activation Range, Start, 
		R5:4 = memd(R0+#8); // Weights Range, Start, 
	}
	{	R7:6 = memd(R0+#16)	// cvt_range, Output Address
		R8 = memw(R0+#24)	// bias
	}
	{	mxclracc
	}
	{ 	bias = mxmem(R8); 
	}
	{ 	activation.ub = mxmem(R2,R3):deep;
		weight.b = mxmem(R4,R5);
	}

	{ 	mxmem(R6,R7):before:retain:sat.ub=acc;
	}
	{ 	mxmem(R6,R7):after:sat.ub=acc;
	}
	r0 = memb(r6);
	jumpr r31;

.size hmx_conv_spatial_major_deep_asm, .-hmx_conv_spatial_major_deep_asm
	


   .text
   .falign
   .globl hmx_conv_channel_major_asm
   .type   hmx_conv_channel_major_asm, @function
   
hmx_conv_channel_major_asm:
	{	R3:2 = memd(R0+#0); // Activation Range, Start, 
		R5:4 = memd(R0+#8); // Weights Range, Start, 
	}
	{	R7:6 = memd(R0+#16)	// cvt_range, Output Address
		R8 = memw(R0+#24)	// bias
	}
	{	mxclracc
	}
	{ 	bias = mxmem(R8); 
	}
	{ 	activation.ub = mxmem(R2,R3):cm;
		weight.b = mxmem(R4,R5);
	}
	{ 	mxmem(R6,R7):before:retain:cm:sat.ub=acc;
	}
	{ 	mxmem(R6,R7):after:cm:sat.ub=acc;
	}
	r0 = memb(r6);
	jumpr r31;
.size hmx_conv_channel_major_asm, .-hmx_conv_channel_major_asm
	
	
   .text
   .falign
   .globl hmx_conv_channel_major_deep_asm
   .type   hmx_conv_channel_major_deep_asm, @function
   
hmx_conv_channel_major_deep_asm:
	{	R3:2 = memd(R0+#0); // Activation Range, Start, 
		R5:4 = memd(R0+#8); // Weights Range, Start, 
	}
	{	R7:6 = memd(R0+#16)	// cvt_range, Output Address
		R8 = memw(R0+#24)	// bias
	}
	{	mxclracc
	}
	{ 	bias = mxmem(R8); 
	}
	{ 	activation.ub = mxmem(R2,R3):deep:cm;
		weight.b = mxmem(R4,R5);
	}

	{ 	mxmem(R6,R7):before:retain:cm:sat.ub=acc;
	}
	{ 	mxmem(R6,R7):after:cm:sat.ub=acc;
	}
	r0 = memb(r6);
	jumpr r31;
.size hmx_conv_channel_major_deep_asm, .-hmx_conv_channel_major_deep_asm

//////////////////////////////////////////////////////////////////////////////////////////////

   .text
   .p2align 2
   .p2align 4,,15
   .globl hmx_assembler_test
   .type   hmx_assembler_test, @function
// Parameters passed in r0-r5   
hmx_assembler_test:
#ifdef DEBUG
	bias = mxmem(R8); 

	activation.ub=mxmem(R6,R7)
	activation.ub=mxmem(R6,R7):cm

	activation.ub=mxmem(R6,R7):single
	activation.ub=mxmem(R6,R7):single:cm

	activation.ub=mxmem(R6,R7):above
	activation.ub=mxmem(R6,R7):above:cm
	
	activation.ub=mxmem(R6,R7):deep
	activation.ub=mxmem(R6,R7):deep:cm

	activation.ub=mxmem(R6,R7):dilate
	activation.ub=mxmem(R6,R7):dilate:cm
	


		
	weight.b=mxmem(R4,R5);
	

	weight.b=mxmem(R4,R5):deep;
	weight.b=mxmem(R4,R5):drop;
	weight.b=mxmem(R4,R5):after;
	weight.b=mxmem(R4,R5):single;



	weight.ubit=mxmem(R4,R5);
	weight.sbit=mxmem(R4,R5);
	weight.c=mxmem(R4,R5);
	weight.sc=mxmem(R4,R5);
	weight.n=mxmem(R4,R5);

	
	
	mxmem(R6,R7):after.ub=acc;
	mxmem(R6,R7):after:sat.ub=acc;
	
	mxmem(R6,R7):after:cm.ub=acc;
	mxmem(R6,R7):after:cm:sat.ub=acc;
	
	mxmem(R6,R7):after:retain.ub=acc;
	mxmem(R6,R7):after:retain:sat.ub=acc;
	mxmem(R6,R7):after:retain:cm.ub=acc;
	mxmem(R6,R7):after:retain:cm:sat.ub=acc;

	mxmem(R6,R7):before.ub=acc;
	mxmem(R6,R7):before:sat.ub=acc;
	
	mxmem(R6,R7):before:cm.ub=acc;
	mxmem(R6,R7):before:cm:sat.ub=acc;
	
	mxmem(R6,R7):before:retain.ub=acc;
	mxmem(R6,R7):before:retain:sat.ub=acc;
	mxmem(R6,R7):before:retain:cm.ub=acc;
	mxmem(R6,R7):before:retain:cm:sat.ub=acc;
#endif	
	
	jumpr r31;
.size hmx_assembler_test, .-hmx_assembler_test
	

