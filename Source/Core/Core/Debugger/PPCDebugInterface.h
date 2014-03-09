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
	virtual void Disassemble(unsigned int address, char *dest, int max_size) final override;
	virtual void GetRawMemoryString(int memory, unsigned int address, char *dest, int max_size) final override;
	virtual int GetInstructionSize(int /*instruction*/) final override {return 4;}
	virtual bool IsAlive() final override;
	virtual bool IsBreakpoint(unsigned int address) final override;
	virtual void SetBreakpoint(unsigned int address) final override;
	virtual void ClearBreakpoint(unsigned int address) final override;
	virtual void ClearAllBreakpoints() final override;
	virtual void ToggleBreakpoint(unsigned int address) final override;
	virtual void ClearAllMemChecks() final override;
	virtual bool IsMemCheck(unsigned int address) final override;
	virtual void ToggleMemCheck(unsigned int address) final override;
	virtual unsigned int ReadMemory(unsigned int address) final override;

	enum {
		EXTRAMEM_ARAM = 1,
	};
	virtual unsigned int ReadExtraMemory(int memory, unsigned int address) final override;
	virtual unsigned int ReadInstruction(unsigned int address) final override;
	virtual unsigned int GetPC() final override;
	virtual void SetPC(unsigned int address) final override;
	virtual void Step() final override {}
	virtual void BreakNow() final override;
	virtual void RunToBreakpoint() final override;
	virtual void InsertBLR(unsigned int address, unsigned int value) final override;
	virtual int GetColor(unsigned int address) final override;
	virtual std::string GetDescription(unsigned int address) final override;
	virtual void ShowJitResults(u32 address) final override;
};
