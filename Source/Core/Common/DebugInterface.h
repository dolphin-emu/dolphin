#pragma once

#include <cstring>
#include <string>

class DebugInterface
{
protected:
	virtual ~DebugInterface() {}

public:
	virtual std::string Disassemble(unsigned int /*address*/) { return "NODEBUGGER"; }
	virtual void GetRawMemoryString(int /*memory*/, unsigned int /*address*/, char *dest, int /*max_size*/) {strcpy(dest, "NODEBUGGER");}
	virtual int GetInstructionSize(int /*instruction*/) {return 1;}
	virtual bool IsAlive() {return true;}
	virtual bool IsBreakpoint(unsigned int /*address*/) {return false;}
	virtual void SetBreakpoint(unsigned int /*address*/){}
	virtual void ClearBreakpoint(unsigned int /*address*/){}
	virtual void ClearAllBreakpoints() {}
	virtual void ToggleBreakpoint(unsigned int /*address*/){}
	virtual void AddWatch(unsigned int /*address*/){}
	virtual void ClearAllMemChecks() {}
	virtual bool IsMemCheck(unsigned int /*address*/) {return false;}
	virtual void ToggleMemCheck(unsigned int /*address*/){}
	virtual unsigned int ReadMemory(unsigned int /*address*/){return 0;}
	virtual void WriteExtraMemory(int /*memory*/, unsigned int /*value*/, unsigned int /*address*/) {}
	virtual unsigned int ReadExtraMemory(int /*memory*/, unsigned int /*address*/){return 0;}
	virtual unsigned int ReadInstruction(unsigned int /*address*/){return 0;}
	virtual unsigned int GetPC() {return 0;}
	virtual void SetPC(unsigned int /*address*/) {}
	virtual void Step() {}
	virtual void RunToBreakpoint() {}
	virtual void BreakNow() {}
	virtual void InsertBLR(unsigned int /*address*/, unsigned int /*value*/) {}
	virtual int GetColor(unsigned int /*address*/){return 0xFFFFFFFF;}
	virtual std::string GetDescription(unsigned int /*address*/) = 0;
};
