set(SRCS	Src/ABI.cpp
			Src/BreakPoints.cpp
			Src/CDUtils.cpp
			Src/ColorUtil.cpp
			Src/ConsoleListener.cpp
			Src/CPUDetect.cpp
			Src/DynamicLibrary.cpp
			Src/FileSearch.cpp
			Src/FileUtil.cpp
			Src/Hash.cpp
			Src/IniFile.cpp
			Src/LogManager.cpp
			Src/MathUtil.cpp
			Src/MemArena.cpp
			Src/MemoryUtil.cpp
			Src/Misc.cpp
			Src/MsgHandler.cpp
			Src/NandPaths.cpp
			Src/OpenCL.cpp
			Src/Plugin.cpp
			Src/PluginDSP.cpp
			Src/PluginVideo.cpp
			Src/SDCardUtil.cpp
			Src/StringUtil.cpp
			Src/SymbolDB.cpp
			Src/SysConf.cpp
			Src/Thread.cpp
			Src/Thunk.cpp
			Src/Timer.cpp
			Src/Version.cpp
			Src/x64Analyzer.cpp
			Src/x64Emitter.cpp
			Src/Crypto/bn.cpp
			Src/Crypto/ec.cpp
			Src/Crypto/md5.cpp
			Src/Crypto/sha1.cpp)

if(WIN32)
	set(SRCS ${SRCS} Src/ExtendedTrace.cpp Src/stdafx.cpp)
endif(WIN32)

add_library(common STATIC ${SRCS})
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-fPIC)
endif()
