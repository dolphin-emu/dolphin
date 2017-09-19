; This is the trojan program we send to the DSP from DSPSpy to figure it out.
REGS_BASE:	equ	0x0f80
MEM_HI:		equ	0x0f7E
MEM_LO:		equ	0x0f7F


; Interrupt vectors 8 vectors, 2 opcodes each
	jmp		irq0
	jmp		irq1
	jmp		irq2
	jmp		irq3
	jmp		irq4
	jmp		irq5
	jmp		irq6
	jmp		irq7

; Main code (and normal entrypoint) at 0x10
	sbset	#0x02
	sbset	#0x03
	sbclr	#0x04
	sbset	#0x05
	sbset	#0x06

	s16
	lri		$CR, #0x00ff

	clr		$acc1
	clr		$acc0

; get address of memory dump and copy it to DRAM
	call	0x807e
	si		@DMBH, #0x8888
	si		@DMBL, #0xdead
	si		@DIRQ, #0x0001

	call	0x8078
	andi	$ac0.m, #0x7fff
	lrs		$ac1.m, @CMBL

	sr		@MEM_HI, $ac0.m
	sr		@MEM_LO, $ac1.m

	lri		$ax0.l, #0
	lri		$ax1.l, #0		;(DSP_CR_IMEM | DSP_CR_TO_CPU)
	lri		$ax0.h, #0x2000
	lr		$ac0.l, @MEM_HI
	lr		$ac0.m, @MEM_LO
	call	do_dma


; get address of registers and DMA them to ram
	call	0x807e
	si		@DMBH, #0x8888
	si		@DMBL, #0xbeef
	si		@DIRQ, #0x0001

	call	0x8078
	andi	$ac0.m, #0x7fff
	lrs		$ac1.m, @CMBL

	sr		@MEM_HI, $ac0.m
	sr		@MEM_LO, $ac1.m

	lri		$ax0.l, #REGS_BASE
	lri		$ax1.l, #0		;(DSP_CR_IMEM | DSP_CR_TO_CPU)
	lri		$ax0.h, #0x80
	lr		$ac0.l, @MEM_HI
	lr		$ac0.m, @MEM_LO
	call	do_dma

; Read in all the registers from RAM
	lri		$ar0, #REGS_BASE+1
	lrri	$ar1, @$ar0
	lrri	$ar2, @$ar0
	lrri	$ar3, @$ar0
	lrri	$ix0, @$ar0
	lrri	$ix1, @$ar0
	lrri	$ix2, @$ar0
	lrri	$ix3, @$ar0
	lrri	$wr0, @$ar0
	lrri	$wr1, @$ar0
	lrri	$wr2, @$ar0
	lrri	$wr3, @$ar0
	lrri	$st0, @$ar0
	lrri	$st1, @$ar0
	lrri	$st2, @$ar0
	lrri	$st3, @$ar0
	lrri	$ac0.h, @$ar0
	lrri	$ac1.h, @$ar0
	lrri	$cr, @$ar0
	lrri	$sr, @$ar0
	lrri	$prod.l, @$ar0
	lrri	$prod.m1, @$ar0
	lrri	$prod.h, @$ar0
	lrri	$prod.m2, @$ar0
	lrri	$ax0.l, @$ar0
	lrri	$ax1.l, @$ar0
	lrri	$ax0.h, @$ar0
	lrri	$ax1.h, @$ar0
	lrri	$ac0.l, @$ar0
	lrri	$ac1.l, @$ar0
	lrri	$ac0.m, @$ar0
	lrri	$ac1.m, @$ar0
	lr		$ar0, @REGS_BASE

	jmp		start_of_test

; This is where we jump when we're done testing, see above.
; We just fall into a loop, playing dead until someone resets the DSP.
end_of_test:
	nop
	jmp		end_of_test

; Utility function to do DMA.
do_dma:
	sr		@DSMAH, $ac0.l
	sr		@DSMAL, $ac0.m
	sr		@DSPA, $ax0.l
	sr		@DSCR, $ax1.l
	sr		@DSBL, $ax0.h	; This kicks off the DMA.
	call	0x863d			; Wait for DMA to complete by watching a bit in DSCR.
	ret

; IRQ handlers. Just send back exception# and die
irq0:
	lri		$ac0.m, #0x0000
	jmp		irq
irq1:
	lri		$ac0.m, #0x0001
	jmp		irq
irq2:
	lri		$ac0.m, #0x0002
	jmp		irq
irq3:
	lri		$ac0.m, #0x0003
	jmp		irq
irq4:
	lri		$ac0.m, #0x0004
	jmp		irq
irq5:
	lrs		$ac0.m, @DMBH
	andcf		$ac0.m, #0x8000
	jlz		irq5
	si		@DMBH, #0x8005
	si		@DMBL, #0x0000
	si		@DIRQ, #0x0001
	lri		$ac0.m, #0xbbbb
	sr		@0xffda, $ac0.m ; pred scale
	sr		@0xffdb, $ac0.m ; yn1
	lr		$ix2, @ARAM
	sr		@0xffdc, $ac0.m ; yn2
	rti
irq6:
	lri		$ac0.m, #0x0006
	jmp		irq
irq7:
	lri		$ac0.m, #0x0007

irq:
	lrs		$ac1.m, @DMBH
	andcf	$ac1.m, #0x8000
	jlz		irq
	si		@DMBH, #0x8bad
	;sr		@DMBL, $wr3     ; ???
	sr		@DMBL, $ac0.m	; Exception number
	si		@DIRQ, #0x0001
	halt		; Through some magic this allows us to properly ack the exception in dspspy
	;rti		; allow dumping of ucodes which cause exceptions...probably not safe at all

; DMA:s the current state of the registers back to the PowerPC. To do this,
; it must write the contents of all regs to DRAM.
; Unfortunately, this loop uses ar0 so it's best to use AR1 and friends for testing
; when messing with indexing.
send_back:
	; make state safe.
	set16
	; store registers to reg table
	sr		@REGS_BASE,  $ar0
	lri		$ar0, #(REGS_BASE + 1)
	srri	@$ar0, $ar1
	srri	@$ar0, $ar2
	srri	@$ar0, $ar3
	srri	@$ar0, $ix0
	srri	@$ar0, $ix1
	srri	@$ar0, $ix2
	srri	@$ar0, $ix3
	srri	@$ar0, $wr0
	srri	@$ar0, $wr1
	srri	@$ar0, $wr2
	srri	@$ar0, $wr3
	srri	@$ar0, $st0
	srri	@$ar0, $st1
	srri	@$ar0, $st2
	srri	@$ar0, $st3
	srri	@$ar0, $ac0.h
	srri	@$ar0, $ac1.h
	srri	@$ar0, $cr
	srri	@$ar0, $sr
	srri	@$ar0, $prod.l
	srri	@$ar0, $prod.m1
	srri	@$ar0, $prod.h
	srri	@$ar0, $prod.m2
	srri	@$ar0, $ax0.l
	srri	@$ar0, $ax1.l
	srri	@$ar0, $ax0.h
	srri	@$ar0, $ax1.h
	srri	@$ar0, $ac0.l
	srri	@$ar0, $ac1.l
	srri	@$ar0, $ac0.m
	srri	@$ar0, $ac1.m

; Regs are stored. Prepare DMA.
	lri		$ax0.l, #0x0000
	lri		$ax1.l, #1		;(DSP_CR_IMEM | DSP_CR_TO_CPU)
	lri		$ax0.h, #0x200
	lr		$ac0.l, @MEM_HI
	lr		$ac0.m, @MEM_LO

; Now, why are we looping here?
	lri		$ar1, #8+8
	bloop	$ar1, dma_copy
	call	do_dma
	addi	$ac0.m, #0x200
	mrr		$ac1.m, $ax0.l
	addi	$ac1.m, #0x100
dma_copy:
	mrr		$ax0.l, $ac1.m

; Wait for the CPU to send us a mail.
	call	0x807e
	si		@DMBH, #0x8888
	si		@DMBL, #0xfeeb
	si		@DIRQ, #0x0001

; wait for the CPU to recieve our response before we execute the next op
	call	0x8078
	andi	$ac0.m, #0x7fff
	lrs		$ac1.m, @CMBL

; Restore all regs again so we're ready to execute another op.
	lri		$ar0, #REGS_BASE+1
	lrri	$ar1, @$ar0
	lrri	$ar2, @$ar0
	lrri	$ar3, @$ar0
	lrri	$ix0, @$ar0
	lrri	$ix1, @$ar0
	lrri	$ix2, @$ar0
	lrri	$ix3, @$ar0
	lrri	$wr0, @$ar0
	lrri	$wr1, @$ar0
	lrri	$wr2, @$ar0
	lrri	$wr3, @$ar0
	lrri	$st0, @$ar0
	lrri	$st1, @$ar0
	lrri	$st2, @$ar0
	lrri	$st3, @$ar0
	lrri	$ac0.h, @$ar0
	lrri	$ac1.h, @$ar0
	lrri	$cr, @$ar0
	lrri	$sr, @$ar0
	lrri	$prod.l, @$ar0
	lrri	$prod.m1, @$ar0
	lrri	$prod.h, @$ar0
	lrri	$prod.m2, @$ar0
	lrri	$ax0.l, @$ar0
	lrri	$ax1.l, @$ar0
	lrri	$ax0.h, @$ar0
	lrri	$ax1.h, @$ar0
	lrri	$ac0.l, @$ar0
	lrri	$ac1.l, @$ar0
	lrri	$ac0.m, @$ar0
	lrri	$ac1.m, @$ar0
	lr		$ar0, @REGS_BASE

	ret		; from send_back

; If you are in set40 mode, use this instead of send_back if you want to stay
; in set40 mode.
send_back_40:
	set16
	call	send_back
	set40
	ret

; Obviously this must be included directly before your test code
start_of_test:
