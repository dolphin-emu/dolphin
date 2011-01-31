set(SRCS	Src/ABI.cpp
			Src/BreakPoints.cpp
			Src/CDUtils.cpp
			Src/ColorUtil.cpp
			Src/ConsoleListener.cpp
			Src/CPUDetect.cpp
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
			Src/SDCardUtil.cpp
			Src/StringUtil.cpp
			Src/SymbolDB.cpp
			Src/SysConf.cpp
			Src/Thread.cpp
			Src/Thunk.cpp
			Src/Timer.cpp
			Src/Version.cpp
			Src/VideoBackendBase.cpp
			Src/x64Analyzer.cpp
			Src/x64Emitter.cpp
			Src/Crypto/aes_cbc.cpp
			Src/Crypto/aes_core.cpp
			Src/Crypto/bn.cpp
			Src/Crypto/ec.cpp
			Src/Crypto/md5.cpp
			Src/Crypto/sha1.cpp)

if(WIN32)
	set(SRCS ${SRCS} Src/ExtendedTrace.cpp)
endif(WIN32)

enable_precompiled_headers(Src/stdafx.h Src/stdafx.cpp SRCS)

add_library(common STATIC ${SRCS})
target_link_libraries(common ${CMAKE_THREAD_LIBS_INIT})
