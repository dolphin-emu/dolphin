#define _LANGUAGE_ASSEMBLY
#include "asm.h"

	//r3 = &fpscr
	//r4 = result
	.globl	TestFRES1
TestFRES1:
	lis			r9,Unit01@ha
	addi		r9,r9,Unit01@l
	lfs  fr0, 0(r9)
	fres fr0, fr0
	stfs fr0, 0(r4)
	fres fr0, fr0
	stfs fr0, 0(r5)
	mffs fr1
	stfd fr1, 0(r3)
	blr

	.section .data
	.balign 4
Unit01:
	.float	0.0, 1.0
NrmData:
	.float	0.5, 3.0
