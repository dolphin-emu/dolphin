; Interrupt vectors 8 vectors, 2 opcodes each
	jmp		irq0
	jmp		irq1
	jmp		irq2
	jmp		irq3
	jmp		irq4
	jmp		irq5
	jmp		irq6
	jmp		irq7

; The rest is in dsp_base_noirq.inc
include "dsp_base_noirq.inc"
