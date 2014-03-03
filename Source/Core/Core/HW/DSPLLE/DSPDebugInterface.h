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
	virtual void Disassemble(unsigned int address, char *dest, int max_size);
	virtual void GetRawMemoryString(int memory, unsigned int address, char *dest, int max_size);
	virtual int GetInstructionSize(int instruction) {return 1;}
	virtual bool IsAlive();
	virtual bool IsBreakpoint(unsigned int address);
	virtual void SetBreakpoint(unsigned int address);
	virtual void ClearBreakpoint(unsigned int address);
	virtual void ClearAllBreakpoints();
	virtual void ToggleBreakpoint(unsigned int address);
	virtual bool IsMemCheck(unsigned int address);
	virtual void ToggleMemCheck(unsigned int address);
	virtual unsigned int ReadMemory(unsigned int address);
	virtual unsigned int ReadInstruction(unsigned int address);
	virtual unsigned int GetPC();
	virtual void SetPC(unsigned int address);
	virtual void Step() {}
	virtual void RunToBreakpoint();
	virtual void InsertBLR(unsigned int address, unsigned int value);
	virtual int GetColor(unsigned int address);
	virtual std::string GetDescription(unsigned int address);
};
