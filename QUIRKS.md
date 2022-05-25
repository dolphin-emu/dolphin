# Dolphin Quirks

## What is this document?

This document aims to note down any emulated behaviors that differ from console behavior and, if available, the rationale behind those decisions.

### DSP opcode execution behavior

Extended opcodes are run *in parallel* with the main opcode; they see the same register state as the input. Since they are executed in parallel, the main and extension opcodes could theoretically write to the same registers When the main and extension opcodes write to the same register, the register is set to the two values bitwise-OR'd together. Consider the following:

```
INC'L $ac0 : $ac0.l, @$ar0
```

For the above example, `$ar0.l` would be set to `($ar0.l + 1) | MEM[$ar0]`. *Note that no official uCode writes to the same register twice like this.*

Dolphin only supports this multi-write behavior in the DSP Interpreter when the `PRECISE_BACKLOG` define is present. In `Interpreter::ExecuteInstruction`, if an extended opcode is in use, the extended opcode's behavior is executed first, followed by the main opcode's behavior. The extended opcode does not directly write to registers, but instead records the writes into a backlog `WriteToBackLog`. The main opcode calls `ZeroWriteBackLog` after it is done reading the register values; this directly writes zero to all registers that have pending writes in the backlog. The main opcode then is free to write directly to registers it changes. Afterwards, `ApplyWriteBackLog` bitwise-ORs the value of the register and the value in the backlog; if the main opcode didn't write to the register then `ZeroWriteBackLog` means that the pending value is being OR'd with zero, so it's used without changes. When `PRECISE_BACKLOG` is not defined, `ZeroWriteBackLog` does nothing and `ApplyWriteBackLog` overwrites the register value with the value from the backlog (so writes from extended opcodes "win'' over the main opcode).
