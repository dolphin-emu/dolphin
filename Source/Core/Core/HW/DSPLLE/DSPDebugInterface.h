// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"
#include "Common/DebugInterface.h"

class DSPDebugInterface final : public DebugInterface
{
public:
	DSPDebugInterface(){}
	virtual std::string Disassemble(unsigned int address) override;
	virtual void GetRawMemoryString(int memory, unsigned int address, char *dest, int max_size) override;
	virtual int GetInstructionSize(int instruction) override { return 1; }
	virtual bool IsAlive() override;
	virtual bool IsBreakpoint(unsigned int address) override;
	virtual void SetBreakpoint(unsigned int address) override;
	virtual void ClearBreakpoint(unsigned int address) override;
	virtual void ClearAllBreakpoints() override;
	virtual void ToggleBreakpoint(unsigned int address) override;
	virtual void ClearAllMemChecks() override;
	virtual bool IsMemCheck(unsigned int address) override;
	virtual void ToggleMemCheck(unsigned int address) override;
	virtual unsigned int ReadMemory(unsigned int address) override;
	virtual unsigned int ReadInstruction(unsigned int address) override;
	virtual unsigned int GetPC() override;
	virtual void SetPC(unsigned int address) override;
	virtual void Step() override {}
	virtual void RunToBreakpoint() override;
	virtual void InsertBLR(unsigned int address, unsigned int value) override;
	virtual int GetColor(unsigned int address) override;
	virtual std::string GetDescription(unsigned int address) override;
};
