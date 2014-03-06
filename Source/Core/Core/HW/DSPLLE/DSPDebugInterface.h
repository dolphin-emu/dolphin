// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <string.h>

#include "Common/Common.h"
#include "Common/DebugInterface.h"

class DSPDebugInterface : public DebugInterface
{
public:
	DSPDebugInterface(){}
	virtual void Disassemble(unsigned int address, char *dest, int max_size) final;
	virtual void GetRawMemoryString(int memory, unsigned int address, char *dest, int max_size) final;
	virtual int GetInstructionSize(int instruction) final {return 1;}
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
	virtual unsigned int ReadInstruction(unsigned int address) final;
	virtual unsigned int GetPC() final;
	virtual void SetPC(unsigned int address) final;
	virtual void Step() final {}
	virtual void RunToBreakpoint() final;
	virtual void InsertBLR(unsigned int address, unsigned int value) final;
	virtual int GetColor(unsigned int address) final;
	virtual std::string GetDescription(unsigned int address) final;
};
