// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DSPTestText.h"

const char s_dsp_test_text[8434] = R"(
DSCR:	equ 0xffc9	; DSP DMA Control Reg
DSBL:	equ 0xffcb	; DSP DMA Block Length
DSPA:	equ 0xffcd	; DSP DMA DMEM Address
DSMAH:	equ	0xffce	; DSP DMA Mem Address H
DSMAL:	equ 0xffcf	; DSP DMA Mem Address L

ACSAH:	equ 0xffd4
ACSAL:	equ	0xffd5
ACEAH:	equ	0xffd6
ACEAL:	equ	0xffd7
ACCAH:	equ	0xffd8
ACCAL:	equ	0xffd9
AMDM:	equ	0xffef	; ARAM DMA Request Mask

DIRQ:	equ	0xfffb	; DSP Irq Request
DMBH:	equ	0xfffc	; DSP Mailbox H
DMBL:	equ	0xfffd	; DSP Mailbox L
CMBH:	equ	0xfffe	; CPU Mailbox H
CMBL:	equ 0xffff	; CPU Mailbox L

R00:	equ	0x00
R01:	equ	0x01
R02:	equ	0x02
R03:	equ	0x03
R04:	equ	0x04
R05:	equ	0x05
R06:	equ	0x06
R07:	equ	0x07
R08:	equ	0x08
R09:	equ	0x09
R0A:	equ	0x0a
R0B:	equ	0x0b
R0C:	equ	0x0c
R0D:	equ	0x0d
R0E:	equ	0x0e
R0F:	equ	0x0f
R10:	equ	0x10
R11:	equ	0x11
R12:	equ	0x12
R13:	equ	0x13
R14:	equ	0x14
R15:	equ	0x15
R16:	equ	0x16
R17:	equ	0x17
R18:	equ	0x18
R19:	equ	0x19
R1A:	equ	0x1a
R1B:	equ	0x1b
R1C:	equ	0x1c
R1D:	equ	0x1d
R1E:	equ	0x1e
R1F:	equ	0x1f

ACH0:	equ	0x10
ACH1:	equ	0x11
ACL0:	equ	0x1e
ACL1:	equ	0x1f

DSP_CR_IMEM:	equ		2
DSP_CR_TO_CPU:	equ		1


REGS_BASE:	equ		0x0f80
MEM_HI:			equ		0x0f7E
MEM_LO:			equ		0x0f7F


; Interrupt vectors 8 vectors, 2 opcodes each

	jmp		irq0
	jmp		irq1
	jmp		irq2
	jmp		irq3
	jmp		irq4
	jmp		irq5
	jmp		irq6
	jmp		irq7

; Main code at 0x10
	CW		0x1302
	CW		0x1303
	CW		0x1204
	CW		0x1305
	CW		0x1306

	s40
	lri		$r12, #0x00ff

main:

	cw		0x8900
	cw		0x8100

; get address of memory dump and copy it

	call	wait_for_dsp_mbox
	si		@DMBH, #0x8888
	si		@DMBL, #0xdead
	si		@DIRQ, #0x0001

	call	wait_for_cpu_mbox
	lrs		$ACL0, @CMBL
	andi	$acl1, #0x7fff

	sr		@MEM_HI, $ACL1
	sr		@MEM_LO, $ACL0

	lri		$r18, #0
	lri		$r19, #0		;(DSP_CR_IMEM | DSP_CR_TO_CPU)
	lri		$r1a, #0x2000
	lr		$r1c, @MEM_HI
	lr		$r1e, @MEM_LO
	call	do_dma


; get address of registers and DMA them to memory

	call	wait_for_dsp_mbox
	si		@DMBH, #0x8888
	si		@DMBL, #0xbeef
	si		@DIRQ, #0x0001

	call	wait_for_cpu_mbox
	lrs		$ACL0, @CMBL
	andi	$acl1, #0x7fff

	sr		@MEM_HI, $ACL1
	sr		@MEM_LO, $ACL0

	lri		$r18, #REGS_BASE
	lri		$r19, #0		;(DSP_CR_IMEM | DSP_CR_TO_CPU)
	lri		$r1a, #0x80
	lr		$r1c, @MEM_HI
	lr		$r1e, @MEM_LO
	call	do_dma



	lri		$r00, #REGS_BASE+1
	lrri	$r01, @$r00
	lrri	$r02, @$r00
	lrri	$r03, @$r00
	lrri	$r04, @$r00
	lrri	$r05, @$r00
	lrri	$r06, @$r00
	lrri	$r07, @$r00
	lrri	$r08, @$r00
	lrri	$r09, @$r00
	lrri	$r0a, @$r00
	lrri	$r0b, @$r00
	lrri	$r0c, @$r00
	lrri	$r0d, @$r00
	lrri	$r0e, @$r00
	lrri	$r0f, @$r00
	lrri	$r10, @$r00
	lrri	$r11, @$r00
	lrri	$r12, @$r00
	lrri	$r13, @$r00
	lrri	$r14, @$r00
	lrri	$r15, @$r00
	lrri	$r16, @$r00
	lrri	$r17, @$r00
	lrri	$r18, @$r00
	lrri	$r19, @$r00
	lrri	$r1a, @$r00
	lrri	$r1b, @$r00
	lrri	$r1c, @$r00
	lrri	$r1d, @$r00
	lrri	$r1e, @$r00
	lrri	$r1f, @$r00
	lr		$r00, @REGS_BASE

	nop
	nop
	nop
	nop




		cw 0x8600

		call	send_back

		JMP ende


;	call dump_memory
;	call	send_back


; 0x041e
;

    cw 0x00de
    cw 0x03f1
    call	send_back

    cw 0x0200
    cw 0x0a60
    call	send_back

    cw 0x1c7e
    call	send_back

    cw 0x8100
    call	send_back

    cw 0x8900
    call	send_back

    cw 0x009f
    cw 0x00a0
    call	send_back

    cw 0x00de
    cw 0x03f1
    call	send_back

    cw 0x5d00
    call	send_back

    cw 0x0e50
    call	send_back

    cw 0x0750
    call	send_back

    cw 0x0270
    call	send_back

    cw 0x5d00
    call	send_back

    cw 0x00da
    cw 0x03f2
    call	send_back

    cw 0x8600
    call	send_back

    JGE g_0c4d
;		cw 0x0290
;   cw 0x0c4d
;   call	send_back    JX0

    cw 0x00de
    cw 0x03f3
    call	send_back

    cw 0x5c00
    call	send_back

 		JLE g_0c38
;   cw 0x0293
;   cw 0x0c38 				JX3
;		call	send_back

    JMP g_0c52

;    cw 0x029f
;    cw 0x0c52
;		call	send_back

g_0c38:
    cw 0x00db
    cw 0x03f7
    call	send_back

    cw 0x009e
    cw 0x8000
    call	send_back

    cw 0x4600
    call	send_back

    JMP g_0c44
;    cw 0x029f
;		 cw 0x0c44
;    call	send_back

g_0c3f:
    cw 0x00db
    cw 0x03f7
    call	send_back

    cw 0x009e
    cw 0x8000
    call	send_back

    cw 0x5600
    call	send_back

g_0c44:
    cw 0x00fe
    cw 0x03f5
    call	send_back

    cw 0x1fda
    call	send_back

    cw 0x7c00
    call	send_back

    cw 0x1f5e
    call	send_back

    cw 0x00fe
    cw 0x03f2
    call	send_back

 	JMP g_0c52
;    cw 0x029f
;    cw 0x0c52
;    call	send_back

g_0c4d:

    cw 0x00de
    cw 0x03f4
    call	send_back

    cw 0x5d00
    call	send_back

 	JLE g_0c3f
;    cw 0x0293
;    cw 0x0c3f
;    call	send_back

g_0c52:
    cw 0x8900
    call	send_back

    cw 0x00dd
    cw 0x03f5
    call	send_back

    cw 0x1501
    call	send_back

    cw 0x8100
    call	send_back

    cw 0x00dc
    cw 0x03f6
    call	send_back

    cw 0x008b
    cw 0x009f
    call	send_back

    cw 0x0080
    cw 0x0a00
    call	send_back

    cw 0x0900
    call	send_back

    BLOOPI #0x50, g_0c65
;    cw 0x1150
;    cw 0x0c65
;		call	send_back


    cw 0x1878
    call	send_back

    cw 0x4c00
    call	send_back

    cw 0x1cfe
    call	send_back

    cw 0x001f
    call	send_back

    cw 0x1fd9
    call	send_back
g_0c65:
    cw 0x1b18
    call	send_back

    cw 0x009f
    cw 0x0a60
    call	send_back

    cw 0x1fc3
    call	send_back

    cw 0x5c00
    call	send_back

    cw 0x00fe
    cw 0x03f1
    call	send_back

    cw 0x00fc
    cw 0x03f6
    call	send_back

    cw 0x008b
    cw 0xffff
    call	send_back



ende:

	nop
	nop
	nop
	nop
	nop
	nop
	nop



dead_loop:
	jmp		dead_loop

do_dma:
	sr		@DSMAH, $r1c
	sr		@DSMAL, $r1e
	sr		@DSPA, $r18
	sr		@DSCR, $r19
	sr		@DSBL, $r1a
wait_dma:
	LRS		$ACL1, @DSCR
	andcf	$acl1, #0x0004
	JLZ		wait_dma
	RET


wait_for_dsp_mbox:
	lrs		$ACL1, @DMBH
	andcf	$acl1, #0x8000
	jlz		wait_for_dsp_mbox
	ret

wait_for_cpu_mbox:
	lrs		$ACL1, @cmbh
	andcf	$acl1, #0x8000
	jlnz	wait_for_cpu_mbox
	ret

irq0:
	lri		$acl0, #0x0000
	jmp		irq
irq1:
	lri		$acl0, #0x0001
	jmp		irq
irq2:
	lri		$acl0, #0x0002
	jmp		irq

irq3:
	lri		$acl0, #0x0003
	jmp		irq
irq4:
	lri		$acl0, #0x0004
	jmp		irq
irq5:
;	jmp finale
	s40
	mrr		$r0d, $r1c
	mrr		$r0d, $r1e
	clr		$acc0
	mrr		$r1e, $r0d
	mrr		$r1c, $r0d
	nop
	nop
	nop
	nop
	nop
	nop
	rti

	lri		$acl0, #0x0005
	jmp		irq
irq6:
	lri		$acl0, #0x0006
	jmp		irq
irq7:
	lri		$acl0, #0x0007
	jmp		irq

irq:
	lrs		$ACL1, @DMBH
	andcf	$acl1, #0x8000
	jlz		irq
	si		@DMBH, #0x8BAD
	sr		@DMBL, $r0b
;sr		@DMBL, $acl0
	si		@DIRQ, #0x0001
	halt




send_back:

	; store registers to reg table
	sr		@REGS_BASE,  $r00
	lri		$r00, #(REGS_BASE + 1)
	srri	@$r00, $r01
	srri	@$r00, $r02
	srri	@$r00, $r03
	srri	@$r00, $r04
	srri	@$r00, $r05
	srri	@$r00, $r06
	srri	@$r00, $r07
	srri	@$r00, $r08
	srri	@$r00, $r09
	srri	@$r00, $r0a
	srri	@$r00, $r0b
	srri	@$r00, $r0c
	srri	@$r00, $r0d
	srri	@$r00, $r0e
	srri	@$r00, $r0f
	srri	@$r00, $r10
	srri	@$r00, $r11
	srri	@$r00, $r12
	srri	@$r00, $r13
	srri	@$r00, $r14
	srri	@$r00, $r15
	srri	@$r00, $r16
	srri	@$r00, $r17
	srri	@$r00, $r18
	srri	@$r00, $r19
	srri	@$r00, $r1a
	srri	@$r00, $r1b
	srri	@$r00, $r1c
	srri	@$r00, $r1d
	srri	@$r00, $r1e
	srri	@$r00, $r1f


	lri		$r18, #0x0000
	lri		$r19, #1		;(DSP_CR_IMEM | DSP_CR_TO_CPU)
	lri		$r1a, #0x200
	lr		$r1c, @MEM_HI
	lr		$r1e, @MEM_LO

	lri		$r01, #8+8

	bloop	$r01, dma_copy
	call	do_dma
	addi	$r1e, #0x200
	mrr		$r1f, $r18
	addi	$r1f, #0x100
	mrr		$r18, $r1f
	nop
dma_copy:
	nop

	call	wait_for_dsp_mbox
	si		@DMBH, #0x8888
	si		@DMBL, #0xfeeb
	si		@DIRQ, #0x0001

; wait for answer before we execute the next op
	call	wait_for_cpu_mbox
	lrs		$ACL0, @CMBL
	andi	$acl1, #0x7fff



	lri		$r00, #REGS_BASE+1
	lrri	$r01, @$r00
	lrri	$r02, @$r00
	lrri	$r03, @$r00
	lrri	$r04, @$r00
	lrri	$r05, @$r00
	lrri	$r06, @$r00
	lrri	$r07, @$r00
	lrri	$r08, @$r00
	lrri	$r09, @$r00
	lrri	$r0a, @$r00
	lrri	$r0b, @$r00
	lrri	$r0c, @$r00
	lrri	$r0d, @$r00
	lrri	$r0e, @$r00
	lrri	$r0f, @$r00
	lrri	$r10, @$r00
	lrri	$r11, @$r00
	lrri	$r12, @$r00
	lrri	$r13, @$r00
	lrri	$r14, @$r00
	lrri	$r15, @$r00
	lrri	$r16, @$r00
	lrri	$r17, @$r00
	lrri	$r18, @$r00
	lrri	$r19, @$r00
	lrri	$r1a, @$r00
	lrri	$r1b, @$r00
	lrri	$r1c, @$r00
	lrri	$r1d, @$r00
	lrri	$r1e, @$r00
	lrri	$r1f, @$r00
	lr		$r00, @REGS_BASE

	ret

send_back_16:

	cw		0x8e00
	call	send_back
	cw		0x8f00

	ret


dump_memory:

	lri		$r02, #0x0000
	lri		$acl0, #0x1000

	lri		$r01, #0x1000
	bloop	$r01, _fill_loop2

	mrr $r03, $acl0
	cw 0x80f0

	mrr $r1f, $r00
	mrr $r00, $r02
	srri	@$r00, $r1b
	mrr $r02, $r00
	mrr $r00, $r1f

	addis	$AC0.M, #0x1

_fill_loop2:
	nop


ret
)";
