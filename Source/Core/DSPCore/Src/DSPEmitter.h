// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#ifndef _DSPEMITTER_H
#define _DSPEMITTER_H

#include "DSPCommon.h"
#include "x64Emitter.h"

#define COMPILED_CODE_SIZE sizeof(void *) * 0x200000

#define MAX_BLOCKS 0x10000

typedef u32 (*CompiledCode)();

class DSPEmitter : public Gen::XCodeBlock
{
public:
	DSPEmitter();
	~DSPEmitter();

	const u8 *m_compiledCode;

	void EmitInstruction(UDSPInstruction inst);
	void unknown_instruction(UDSPInstruction inst);
	void Default(UDSPInstruction _inst);
	void ClearIRAM();

	void CompileDispatcher();

	const u8 *Compile(int start_addr);

	int STACKALIGN RunForCycles(int cycles);

	// Register helpers
	void setCompileSR(u16 bit);
	void clrCompileSR(u16 bit);
	void checkExceptions(u32 retval);

	// Memory helper functions
	void increment_addr_reg(int reg);
	void decrement_addr_reg(int reg);
	void increase_addr_reg(int reg);
	void decrease_addr_reg(int reg);
	void ext_dmem_write(u32 src, u32 dest);
	void ext_dmem_read(u16 addr);

	// Ext command helpers
	void pushExtValueFromReg(u16 dreg, u16 sreg);
	void popExtValueToReg();
	void pushExtValueFromMem(u16 dreg, u16 sreg);
	void pushExtValueFromMem2(u16 dreg, u16 sreg);
	void zeroWriteBackLog(const UDSPInstruction opc);

	// Ext commands
	void l(const UDSPInstruction opc);
	void ln(const UDSPInstruction opc);
	void ls(const UDSPInstruction opc);
	void lsn(const UDSPInstruction opc);
	void lsm(const UDSPInstruction opc);
	void lsnm(const UDSPInstruction opc);
	void sl(const UDSPInstruction opc);
	void sln(const UDSPInstruction opc);
	void slm(const UDSPInstruction opc);
	void slnm(const UDSPInstruction opc);
	void s(const UDSPInstruction opc);
	void sn(const UDSPInstruction opc);
	void ld(const UDSPInstruction opc);
	void ldn(const UDSPInstruction opc);
	void ldm(const UDSPInstruction opc);
	void ldnm(const UDSPInstruction opc);
	void mv(const UDSPInstruction opc);
	void dr(const UDSPInstruction opc);
	void ir(const UDSPInstruction opc);
	void nr(const UDSPInstruction opc);
	void nop(const UDSPInstruction opc) {}

	// Commands
	void dar(const UDSPInstruction opc);
	void iar(const UDSPInstruction opc);
	void subarn(const UDSPInstruction opc);
	void addarn(const UDSPInstruction opc);
	void sbclr(const UDSPInstruction opc);
	void sbset(const UDSPInstruction opc);
	void srbith(const UDSPInstruction opc);

private:
	CompiledCode *blocks;
	u16 *blockSize;
	u16 compileSR;

	// CALL this to start the dispatcher
	u8 *enterDispatcher;

	// JMP here when a block should be dispatches. make sure you're in a block
	// or at the same stack level already.
	u8 *dispatcher;

	// The index of the last stored ext value (compile time).
	int storeIndex;
	int storeIndex2;
	
	// Counts down.
	// int cycles;

	DISALLOW_COPY_AND_ASSIGN(DSPEmitter);

	void ToMask(Gen::X64Reg value_reg = Gen::EDI, Gen::X64Reg temp_reg = Gen::ESI);
	void dsp_increment_one(Gen::X64Reg ar = Gen::EAX, Gen::X64Reg wr = Gen::EDX, Gen::X64Reg wr_pow = Gen::EDI, Gen::X64Reg temp_reg = Gen::ESI);
	void dsp_decrement_one(Gen::X64Reg ar = Gen::EAX, Gen::X64Reg wr = Gen::EDX, Gen::X64Reg wr_pow = Gen::EDI, Gen::X64Reg temp_reg = Gen::ESI);
};									  


#endif // _DSPEMITTER_H

