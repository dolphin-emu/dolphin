// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/DebugInterface.h"

//wrapper between disasm control and Dolphin debugger

class PPCDebugInterface final : public DebugInterface
{
public:
	PPCDebugInterface(){}
	virtual std::string Disassemble(unsigned int address) override;
	virtual void GetRawMemoryString(int memory, unsigned int address, char *dest, int max_size) override;
	virtual int GetInstructionSize(int /*instruction*/) override {return 4;}
	virtual bool IsAlive() override;
	virtual bool IsBreakpoint(unsigned int address) override;
	virtual void SetBreakpoint(unsigned int address) override;
	virtual void ClearBreakpoint(unsigned int address) override;
	virtual void ClearAllBreakpoints() override;
	virtual void AddWatch(unsigned int address) override;
	virtual void ToggleBreakpoint(unsigned int address) override;
	virtual void ClearAllMemChecks() override;
	virtual bool IsMemCheck(unsigned int address) override;
	virtual void ToggleMemCheck(unsigned int address) override;
	virtual unsigned int ReadMemory(unsigned int address) override;

	enum
	{
		EXTRAMEM_ARAM = 1,
	};

	virtual unsigned int ReadExtraMemory(int memory, unsigned int address) override;
	virtual unsigned int ReadInstruction(unsigned int address) override;
	virtual unsigned int GetPC() override;
	virtual void SetPC(unsigned int address) override;
	virtual void Step() override {}
	virtual void BreakNow() override;
	virtual void RunToBreakpoint() override;
	virtual void InsertBLR(unsigned int address, unsigned int value) override;
	virtual int GetColor(unsigned int address) override;
	virtual std::string GetDescription(unsigned int address) override;
};
