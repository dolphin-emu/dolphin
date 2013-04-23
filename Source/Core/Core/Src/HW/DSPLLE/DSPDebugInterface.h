// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _DSPDEBUGINTERFACE_H
#define _DSPDEBUGINTERFACE_H

#include <string>

#include "DebugInterface.h"
#include "Common.h"

class DSPDebugInterface : public DebugInterface
{
public:
	DSPDebugInterface(){} 
	virtual void disasm(unsigned int address, char *dest, int max_size);
	virtual void getRawMemoryString(int memory, unsigned int address, char *dest, int max_size);
	virtual int getInstructionSize(int instruction) {return 1;}
	virtual bool isAlive();
	virtual bool isBreakpoint(unsigned int address);
	virtual void setBreakpoint(unsigned int address);
	virtual void clearBreakpoint(unsigned int address);
	virtual void clearAllBreakpoints();
	virtual void toggleBreakpoint(unsigned int address);
	virtual bool isMemCheck(unsigned int address);
	virtual void toggleMemCheck(unsigned int address);
	virtual unsigned int readMemory(unsigned int address);
	virtual unsigned int readInstruction(unsigned int address);
	virtual unsigned int getPC();
	virtual void setPC(unsigned int address);
	virtual void step() {}
	virtual void runToBreakpoint();
	virtual void insertBLR(unsigned int address, unsigned int value);
	virtual int getColor(unsigned int address);
	virtual std::string getDescription(unsigned int address);
};

#endif  // _DSPDEBUGINTERFACE_H
