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
	DSPJitTester(u16 opcode, u16 opcode_ext, bool verbose = false);
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
	void DumpJittedCode();

	static void Initialize();
};

#endif