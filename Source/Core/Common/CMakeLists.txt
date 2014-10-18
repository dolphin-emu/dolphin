set(SRCS BreakPoints.cpp
         CDUtils.cpp
         ColorUtil.cpp
         FileSearch.cpp
         FileUtil.cpp
         GekkoDisassembler.cpp
         Hash.cpp
         IniFile.cpp
         MathUtil.cpp
         MemArena.cpp
         MemoryUtil.cpp
         Misc.cpp
         MsgHandler.cpp
         NandPaths.cpp
         Network.cpp
         PcapFile.cpp
         PerformanceCounter.cpp
         SettingsHandler.cpp
         SDCardUtil.cpp
         StringUtil.cpp
         SymbolDB.cpp
         SysConf.cpp
         Thread.cpp
         Timer.cpp
         Version.cpp
         x64ABI.cpp
         x64Analyzer.cpp
         x64Emitter.cpp
         Crypto/bn.cpp
         Crypto/ec.cpp
         Logging/ConsoleListener.cpp
         Logging/LogManager.cpp)


if(_M_ARM)
	if (_M_ARM_32) #ARMv7
		set(SRCS ${SRCS}
		         ArmEmitter.cpp)
	else() #AArch64
		set(SRCS ${SRCS}
	               Arm64Emitter.cpp)
	endif()
	set(SRCS ${SRCS}
	    ArmCPUDetect.cpp
	    GenericFPURoundMode.cpp)
else()
	if(_M_X86) #X86
		set(SRCS ${SRCS}
		         x64FPURoundMode.cpp
		         x64CPUDetect.cpp)
	else() # Generic
		set(SRCS ${SRCS}
		         GenericFPURoundMode.cpp
		         GenericCPUDetect.cpp)
	endif()
endif()
if(WIN32)
    set(SRCS ${SRCS} ExtendedTrace.cpp)
endif(WIN32)

set(LIBS "${CMAKE_THREAD_LIBS_INIT}")
if((NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND (NOT ANDROID))
	set(LIBS ${LIBS} rt)
endif()

add_dolphin_library(common "${SRCS}" "${LIBS}")
