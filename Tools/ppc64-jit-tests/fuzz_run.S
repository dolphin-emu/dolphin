# Runs a generated program with the state at r3, the program at r4: loads every register but r1
# and r31 from the state, calls the program with r31 pointing at the state, and stores every
# register back. The layout is State in fuzz.c.
	.set IN_GPR, 0
	.set IN_PS0, 128
	.set IN_PS1, 384
	.set IN_CR, 512
	.set IN_XER, 516
	.set IN_CTR, 520
	.set IN_FPSCR, 528
	.set OUT_GPR, 544
	.set OUT_PS0, 672
	.set OUT_PS1, 928
	.set OUT_CR, 1184
	.set OUT_XER, 1188
	.set OUT_CTR, 1192
	.set OUT_FPSCR, 1200
	.set ZERO, 1208
	.set OUT_EXCEPTIONS, 1216
	.set OUT_DAR, 1220
	.set OUT_DSISR, 1224

	.section .text
	.globl FuzzRun
FuzzRun:
	stwu 1, -256(1)
	mflr 0
	stw 0, 260(1)
	stw 2, 8(1)
	stmw 13, 12(1)
	.irp i, 14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	stfd \i, 88+8*(\i-14)(1)
	.endr

	li 0, 0
	mtspr 912, 0		# GQR0: floats, for the loads below
	mtsprg1 0		# The exception handlers' counts and records
	mtsprg2 0
	mtsprg3 0
	mr 31, 3
	mtlr 4
	lfd 0, IN_FPSCR(31)
	mtfsf 0xff, 0
	# ps1 from a single, ps0 from a double.
	.irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	psq_l \i, IN_PS1+4*\i(31), 1, 0
	ps_merge00 \i, \i, \i
	lfd \i, IN_PS0+8*\i(31)
	.endr
	lwz 0, IN_CR(31)
	mtcrf 0xff, 0
	lwz 0, IN_XER(31)
	mtxer 0
	lwz 0, IN_CTR(31)
	mtctr 0
	.irp i, 0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	lwz \i, IN_GPR+4*\i(31)
	.endr

	blrl

	.irp i, 0,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
	stw \i, OUT_GPR+4*\i(31)
	.endr
	mfcr 0
	stw 0, OUT_CR(31)
	mfxer 0
	stw 0, OUT_XER(31)
	mfctr 0
	stw 0, OUT_CTR(31)
	mfsprg1 0
	stw 0, OUT_EXCEPTIONS(31)
	mfsprg2 0
	stw 0, OUT_DAR(31)
	mfsprg3 0
	stw 0, OUT_DSISR(31)
	.irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	stfd \i, OUT_PS0+8*\i(31)
	.endr
	.irp i, 0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	ps_merge11 0, \i, \i
	stfd 0, OUT_PS1+8*\i(31)
	.endr
	mffs 0
	stfd 0, OUT_FPSCR(31)
	lfd 0, ZERO(31)
	mtfsf 0xff, 0

	.irp i, 14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
	lfd \i, 88+8*(\i-14)(1)
	.endr
	lmw 13, 12(1)
	lwz 2, 8(1)
	lwz 0, 260(1)
	mtlr 0
	addi 1, 1, 256
	blr
