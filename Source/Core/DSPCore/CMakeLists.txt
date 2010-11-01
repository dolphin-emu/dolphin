set(SRCS	Src/assemble.cpp
			Src/disassemble.cpp
			Src/DSPAccelerator.cpp
			Src/DSPIntCCUtil.cpp
			Src/DSPIntExtOps.cpp
			Src/DSPHWInterface.cpp
			Src/DSPMemoryMap.cpp
			Src/DSPStacks.cpp
			Src/DSPAnalyzer.cpp
			Src/DspIntArithmetic.cpp
			Src/DspIntBranch.cpp
			Src/DspIntLoadStore.cpp
			Src/DspIntMisc.cpp
			Src/DspIntMultiplier.cpp
			Src/DSPEmitter.cpp
			Src/DSPCodeUtil.cpp
			Src/LabelMap.cpp
			Src/DSPInterpreter.cpp
			Src/DSPCore.cpp
			Src/DSPTables.cpp
			Src/Jit/DSPJitExtOps.cpp
			Src/Jit/DSPJitUtil.cpp
			Src/Jit/DSPJitMisc.cpp)

add_library(dspcore STATIC ${SRCS})
