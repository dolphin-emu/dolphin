#ifndef __DSP_JIT_TESTER_
#define __DSP_JIT_TESTER_

#include "DSPCore.h"
#include "DSPInterpreter.h"
//#include "DSPIntExtOps.h"
//
//#include "x64Emitter.h"

class DSPJitTester
{
	UDSPInstruction instruction;
	const DSPOPCTemplate *opcode_template;
	DSPEmitter jit;
	SDSP last_int_dsp;
	SDSP last_jit_dsp;
	bool be_verbose;
	int run_count;
	int fail_count;
	char instruction_name[16];

	bool AreEqual(SDSP&, SDSP&);
public:
	DSPJitTester(u16 opcode, u16 opcode_ext, bool verbose = false)
		: be_verbose(verbose), run_count(0), fail_count(0)
	{
		instruction = opcode << 9 | opcode_ext;
		opcode_template = GetOpTemplate(instruction);
		sprintf(instruction_name, "%s", opcode_template->name);
		if (opcode_template->extended) 
			sprintf(&instruction_name[strlen(instruction_name)], "'%s", 
				extOpTable[instruction & (((instruction >> 12) == 0x3) ? 0x7F : 0xFF)]->name);
	}
	bool Test(SDSP);
	SDSP RunInterpreter(SDSP);
	SDSP RunJit(SDSP);
	void ResetInterpreter();
	void ResetJit();
	inline SDSP GetLastInterpreterDSP() { return last_int_dsp; }
	inline SDSP GetLastJitDSP() { return last_jit_dsp; }
	inline int GetRunCount() { return run_count; }
	inline int GetFailCount() { return fail_count; }
	inline const char* GetInstructionName() { return instruction_name; }
	void Report();

	static void Initialize();
};

#endif