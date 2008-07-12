#ifndef _PPCDEBUGINTERFACE_H
#define _PPCDEBUGINTERFACE_H

#include <string>

//wrapper between disasm control and Dolphin debugger

class PPCDebugInterface : public DebugInterface
{
public:
	PPCDebugInterface(){} 
	virtual const char *disasm(unsigned int address);
	virtual int getInstructionSize(int instruction) {return 4;}
	virtual bool isAlive();
	virtual bool isBreakpoint(unsigned int address);
	virtual void setBreakpoint(unsigned int address);
	virtual void clearBreakpoint(unsigned int address);
	virtual void clearAllBreakpoints();
	virtual void toggleBreakpoint(unsigned int address);
	virtual unsigned int readMemory(unsigned int address);
	virtual unsigned int getPC();
	virtual void setPC(unsigned int address);
	virtual void step() {}
	virtual void runToBreakpoint();
	virtual int getColor(unsigned int address);
	virtual std::string getDescription(unsigned int address);
};

#endif
