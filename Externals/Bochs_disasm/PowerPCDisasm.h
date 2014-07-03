/* $VER: ppc_disasm V0.1 (23.05.1998)
 *
 * Disassembler module for the PowerPC microprocessor family
 * Copyright (c) 1998-2000  Frank Wille
 *
 * ppc_disasm.c is freeware and may be freely redistributed as long as
 * no modifications are made and nothing is charged for it.
 * Non-commercial usage is allowed without any restrictions.
 * EVERY PRODUCT OR PROGRAM DERIVED DIRECTLY FROM MY SOURCE MAY NOT BE
 * SOLD COMMERCIALLY WITHOUT PERMISSION FROM THE AUTHOR.
 *
 *
 * v0.1  (23.05.1998) phx
 *       First version, which implements all PowerPC instructions.
 * v0.0  (09.05.1998) phx
 *       File created.
 */


// Yeah, this does not really belong in bochs_disasm, but hey, it's a disasm and it needed a common location...

#ifndef _POWERPC_DISASM
#define _POWERPC_DISASM

void DisassembleGekko(unsigned int opcode, unsigned int curInstAddr, char* dest, int max_size);
const char* GetGPRName(unsigned int index);
const char* GetFPRName(unsigned int index);

#endif
