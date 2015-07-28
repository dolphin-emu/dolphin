set(SRCS BreakPoints.cpp
         CDUtils.cpp
         ColorUtil.cpp
         ENetUtil.cpp
         FileSearch.cpp
         FileUtil.cpp
         GekkoDisassembler.cpp
         Hash.cpp
         IniFile.cpp
         JitRegister.cpp
         MathUtil.cpp
         MemArena.cpp
         MemoryUtil.cpp
         Misc.cpp
         MsgHandler.cpp
         NandPaths.cpp
         Network.cpp
         PcapFile.cpp
         PerformanceCounter.cpp
         Profiler.cpp
         SettingsHandler.cpp
         SDCardUtil.cpp
         StringUtil.cpp
         SymbolDB.cpp
         SysConf.cpp
         Thread.cpp
         Timer.cpp
         TraversalClient.cpp
         Version.cpp
         x64ABI.cpp
         x64Analyzer.cpp
         x64Emitter.cpp
         Crypto/bn.cpp
         Crypto/ec.cpp
         Logging/ConsoleListenerNix.cpp
         Logging/LogManager.cpp)

set(LIBS enet)
if(_M_ARM_64)
	set(SRCS ${SRCS}
	         Arm64Emitter.cpp
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

set(LIBS "${CMAKE_THREAD_LIBS_INIT}" ${VTUNE_LIBRARIES})
if(NOT APPLE AND NOT ANDROID)
	set(LIBS ${LIBS} rt)
endif()

add_dolphin_library(common "${SRCS}" "${LIBS}")
add_executable(traversal_server TraversalServer.cpp)
