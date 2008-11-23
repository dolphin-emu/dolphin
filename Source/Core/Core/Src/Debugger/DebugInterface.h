#ifndef _DEBUGINTERFACE_H
#define _DEBUGINTERFACE_H

#include <string>

class DebugInterface
{
protected:
	virtual ~DebugInterface() {}
public:
	virtual const char *disasm(unsigned int /*address*/) {return "NODEBUGGER";}
	virtual const char *getRawMemoryString(unsigned int /*address*/){return "NODEBUGGER";}
	virtual int getInstructionSize(int /*instruction*/) {return 1;}

	virtual bool isAlive() {return true;}
	virtual bool isBreakpoint(unsigned int /*address*/) {return false;}
	virtual void setBreakpoint(unsigned int /*address*/){}
	virtual void clearBreakpoint(unsigned int /*address*/){}
	virtual void clearAllBreakpoints() {}
	virtual void toggleBreakpoint(unsigned int /*address*/){}
	virtual unsigned int readMemory(unsigned int /*address*/){return 0;}
	virtual unsigned int readInstruction(unsigned int /*address*/){return 0;}
	virtual unsigned int getPC() {return 0;}
	virtual void setPC(unsigned int /*address*/) {}
	virtual void step() {}
	virtual void runToBreakpoint() {}
	virtual void insertBLR(unsigned int /*address*/) {}
	virtual int getColor(unsigned int /*address*/){return 0xFFFFFFFF;}
	virtual std::string getDescription(unsigned int /*address*/) = 0;
};

#endif
