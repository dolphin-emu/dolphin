#ifndef _PPCDEBUGINTERFACE_H
#define _PPCDEBUGINTERFACE_H

#include <string>

#include "DebugInterface.h"

//wrapper between disasm control and Dolphin debugger

class PPCDebugInterface : public DebugInterface
{
public:
	PPCDebugInterface(){} 
	virtual void disasm(unsigned int address, char *dest, int max_size);
	virtual void getRawMemoryString(unsigned int address, char *dest, int max_size);
	virtual int getInstructionSize(int instruction) {return 4;}
	virtual bool isAlive();
	virtual bool isBreakpoint(unsigned int address);
	virtual void setBreakpoint(unsigned int address);
	virtual void clearBreakpoint(unsigned int address);
	virtual void clearAllBreakpoints();
	virtual void toggleBreakpoint(unsigned int address);
	virtual unsigned int readMemory(unsigned int address);
	virtual unsigned int readInstruction(unsigned int address);
	virtual unsigned int getPC();
	virtual void setPC(unsigned int address);
	virtual void step() {}
	virtual void runToBreakpoint();
	virtual void insertBLR(unsigned int address, unsigned int);
	virtual int getColor(unsigned int address);
	virtual std::string getDescription(unsigned int address);
};

#endif
