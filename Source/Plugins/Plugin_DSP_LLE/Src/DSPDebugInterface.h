#ifndef _DSPDEBUGINTERFACE_H
#define _DSPDEBUGINTERFACE_H

#include <string>

#include "../../../Core/Core/Src/Debugger/DebugInterface.h"

class DSPDebugInterface : public DebugInterface
{
public:
	DSPDebugInterface(){} 
	virtual void disasm(unsigned int address, char *dest, int max_size);
	virtual void getRawMemoryString(unsigned int address, char *dest, int max_size);
	virtual int getInstructionSize(int instruction) {return 2;}
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
	virtual void insertBLR(unsigned int address);
	virtual int getColor(unsigned int address);
	virtual std::string getDescription(unsigned int address);
};

#endif  // _DSPDEBUGINTERFACE_H
