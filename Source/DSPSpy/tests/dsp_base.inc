; This is the trojan program we send to the DSP from DSPSpy to figure it out.
REGS_BASE:      equ             0x0f80
MEM_HI:                 equ             0x0f7E
MEM_LO:                 equ             0x0f7F

;
; CODE STARTS HERE.

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
	sbset       #0x02
	sbset       #0x03
	sbclr       #0x04
	sbset       #0x05
	sbset       #0x06

	set16
	lri		$CR, #0x00ff

; Why do we have a main label here?	
main:

	clr         $ACC1
	clr         $ACC0

; get address of memory dump and copy it to DRAM

	call	wait_for_dsp_mbox
	si		@DMBH, #0x8888
	si		@DMBL, #0xdead
	si		@DIRQ, #0x0001

	call	wait_for_cpu_mbox
	lrs		$AC0.M, @CMBL
	andi	$ac1.m, #0x7fff

	sr		@MEM_HI, $AC1.M
	sr		@MEM_LO, $AC0.M

	lri		$ax0.l, #0
	lri		$ax1.l, #0		;(DSP_CR_IMEM | DSP_CR_TO_CPU)
	lri		$ax0.h, #0x2000
	lr		$ac0.l, @MEM_HI
	lr		$ac0.m, @MEM_LO
	call	do_dma


; get address of registers and DMA them to ram

	call	wait_for_dsp_mbox
	si		@DMBH, #0x8888
	si		@DMBL, #0xbeef
	si		@DIRQ, #0x0001

	call	wait_for_cpu_mbox
	lrs		$AC0.M, @CMBL
	andi	$ac1.m, #0x7fff

	sr		@MEM_HI, $AC1.M
	sr		@MEM_LO, $AC0.M

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
	lrri	$r08, @$ar0
	lrri	$r09, @$ar0
	lrri	$r10, @$ar0
	lrri	$r11, @$ar0
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

       jmp start_of_test

; This is where we jump when we're done testing, see above.
end_of_test:

	nop
	nop
	nop
	nop
	nop
	nop
	nop
	
; We just fall into a loop, playing dead until someone resets the DSP.	
dead_loop:
	jmp		dead_loop

; Utility function to do DMA.
; ac0.l:ac0.m - external address.
; ax0.l     - address in DSP
do_dma:
	sr		@DSMAH, $ac0.l
	sr		@DSMAL, $ac0.m
	sr		@DSPA, $ax0.l
	sr		@DSCR, $ax1.l
	sr		@DSBL, $ax0.h  ; This kicks off the DMA.

; Waits for said DMA to complete by watching a bit in DSCR.
wait_dma:
	LRS		$AC1.M, @DSCR
	andcf	$ac1.m, #0x0004
	JLZ		wait_dma
	RET

; This waits for a mail to arrive in the DSP in-mailbox.
wait_for_dsp_mbox:
	lrs		$AC1.M, @DMBH
	andcf	$ac1.m, #0x8000
	jlz		wait_for_dsp_mbox
	ret

; This waits for the CPU to grab a mail that we just sent from the DSP.
wait_for_cpu_mbox:
	lrs		$AC1.M, @cmbh
	andcf	$ac1.m, #0x8000
	jlnz	wait_for_cpu_mbox
	ret

; IRQ handlers. Not entirely sure what good they do currently.
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
; No idea what this code is doing.
	set16
	mrr		$st1, $ac0.l
	mrr		$st1, $ac0.m
	clr		$acc0
	mrr		$ac0.m, $st1
	mrr		$ac0.l, $st1
	nop     ; Or why there's a nop sled here.
	nop
	nop
	nop
	nop
	nop
	rti
	
	lri		$ac0.m, #0x0005
	jmp		irq
irq6:
	lri		$ac0.m, #0x0006
	jmp		irq
irq7:
	lri		$ac0.m, #0x0007
	jmp		irq

irq:
	lrs		$AC1.M, @DMBH
	andcf	$ac1.m, #0x8000
	jlz		irq
	si		@DMBH, #0x8BAD
	sr		@DMBL, $r11
;sr		@DMBL, $ac0.m	
	si		@DIRQ, #0x0001
	halt
	
; DMA:s the current state of the registers back to the PowerPC. To do this,
; it must write the contents of all regs to DRAM.
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
	srri	@$ar0, $r08
	srri	@$ar0, $r09
	srri	@$ar0, $r10
	srri	@$ar0, $r11
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

	lri		$ar1, #8+8

; Now, why are we looping here?
	bloop	$ar1, dma_copy
	call	do_dma
	addi	$ac0.m, #0x200
	mrr		$ac1.m, $ax0.l
	addi	$ac1.m, #0x100
	mrr		$ax0.l, $ac1.m
	nop

dma_copy:
	nop

; Wait for the CPU to send us a mail.	
	call	wait_for_dsp_mbox
	si		@DMBH, #0x8888
	si		@DMBL, #0xfeeb
	si		@DIRQ, #0x0001
	
; wait for the CPU to recieve our response before we execute the next op	
	call	wait_for_cpu_mbox
	lrs		$AC0.M, @CMBL
	andi	$ac1.m, #0x7fff

; Restore all regs again so we're ready to execute another op.
	lri		$ar0, #REGS_BASE+1
	lrri	$ar1, @$ar0
	lrri	$ar2, @$ar0
	lrri	$ar3, @$ar0
	lrri	$ix0, @$ar0
	lrri	$ix1, @$ar0
	lrri	$ix2, @$ar0
	lrri	$ix3, @$ar0
	lrri	$r08, @$ar0
	lrri	$r09, @$ar0
	lrri	$r10, @$ar0
	lrri	$r11, @$ar0
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
	
	ret   ; from send_back
	
; If you are in set40 mode, use this instead of send_back if you want to stay
; in set40 mode.
send_back_40:
	set16
	call	send_back	
	set40
	ret

; This one's odd. Doesn't look like it should work since it uses ac0.m but 
; increments acm0... (acc0)	
dump_memory:
	lri		$ar2, #0x0000
	lri		$ac0.m, #0x1000
	
	lri		$ar1, #0x1000		
	bloop	$ar1, _fill_loop2			
	
	mrr $ar3, $ac0.m
	nx'ld   : $AX0.H, $AX1.H, @$AR0
	
	mrr $ac1.m, $ar0
	mrr $ar0, $ar2
	srri @$ar0, $ax1.h
	mrr $ar2, $ar0
	mrr $ar0, $ac1.m
	
	addis $acc0, #0x1

_fill_loop2:
	nop
	ret   ; from dump_memory

        
start_of_test:
