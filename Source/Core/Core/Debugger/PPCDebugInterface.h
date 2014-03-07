// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/DebugInterface.h"

//wrapper between disasm control and Dolphin debugger

class PPCDebugInterface : public DebugInterface
{
public:
	PPCDebugInterface(){}
	virtual void Disassemble(unsigned int address, char *dest, int max_size) final;
	virtual void GetRawMemoryString(int memory, unsigned int address, char *dest, int max_size) final;
	virtual int GetInstructionSize(int /*instruction*/) final {return 4;}
	virtual bool IsAlive() final;
	virtual bool IsBreakpoint(unsigned int address) final;
	virtual void SetBreakpoint(unsigned int address) final;
	virtual void ClearBreakpoint(unsigned int address) final;
	virtual void ClearAllBreakpoints() final;
	virtual void ToggleBreakpoint(unsigned int address) final;
	virtual void ClearAllMemChecks() final;
	virtual bool IsMemCheck(unsigned int address) final;
	virtual void ToggleMemCheck(unsigned int address) final;
	virtual unsigned int ReadMemory(unsigned int address) final;

	enum {
		EXTRAMEM_ARAM = 1,
	};
	virtual unsigned int ReadExtraMemory(int memory, unsigned int address) final;
	virtual unsigned int ReadInstruction(unsigned int address) final;
	virtual unsigned int GetPC() final;
	virtual void SetPC(unsigned int address) final;
	virtual void Step() final {}
	virtual void BreakNow() final;
	virtual void RunToBreakpoint() final;
	virtual void InsertBLR(unsigned int address, unsigned int value) final;
	virtual int GetColor(unsigned int address) final;
	virtual std::string GetDescription(unsigned int address) final;
	virtual void ShowJitResults(u32 address) final;
};
