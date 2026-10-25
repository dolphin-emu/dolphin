# Entry point of the guest test program: set up a stack, clear .bss and call main().
	.section .text.start, "ax"
	.globl _start
_start:
	lis 1, 0x8170		# Stack at the top of MEM1, below 24 MiB
	li 0, 0
	stwu 0, -16(1)

	# Clear .bss
	lis 3, __bss_start@ha
	addi 3, 3, __bss_start@l
	lis 4, __bss_end@ha
	addi 4, 4, __bss_end@l
1:	cmplw 3, 4
	bge 2f
	stw 0, 0(3)
	addi 3, 3, 4
	b 1b
2:
	bl main

	# Wait forever: an idle loop.
3:	b 3b
