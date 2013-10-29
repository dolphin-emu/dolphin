// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _PPCDEBUGINTERFACE_H
#define _PPCDEBUGINTERFACE_H

#include <string>

#include "DebugInterface.h"

//wrapper between disasm control and Dolphin debugger

class PPCDebugInterface : public DebugInterface
{
public:
	PPCDebugInterface(){}
	virtual void disasm(unsigned int address, char *dest, int max_size) override;
	virtual void getRawMemoryString(int memory, unsigned int address, char *dest, int max_size) override;
	virtual int getInstructionSize(int /*instruction*/) override {return 4;}
	virtual bool isAlive() override;
	virtual bool isBreakpoint(unsigned int address) override;
	virtual void setBreakpoint(unsigned int address) override;
	virtual void clearBreakpoint(unsigned int address) override;
	virtual void clearAllBreakpoints() override;
	virtual void toggleBreakpoint(unsigned int address) override;
	virtual bool isMemCheck(unsigned int address) override;
	virtual void toggleMemCheck(unsigned int address) override;
	virtual unsigned int readMemory(unsigned int address) override;

	enum {
		EXTRAMEM_ARAM = 1,
	};
	virtual unsigned int readExtraMemory(int memory, unsigned int address) override;
	virtual unsigned int readInstruction(unsigned int address) override;
	virtual unsigned int getPC() override;
	virtual void setPC(unsigned int address) override;
	virtual void step() override {}
	virtual void breakNow() override;
	virtual void runToBreakpoint() override;
	virtual void insertBLR(unsigned int address, unsigned int value) override;
	virtual int getColor(unsigned int address) override;
	virtual std::string getDescription(unsigned int address) override;
	virtual void showJitResults(u32 address) override;
};

#endif

