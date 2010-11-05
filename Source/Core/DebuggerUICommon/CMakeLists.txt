set(SRCS	Src/CodeView.cpp
			Src/DebuggerUIUtil.cpp
			Src/MemoryView.cpp)

add_library(debugger_ui_util STATIC ${SRCS})
if(UNIX)
	add_definitions(-fPIC)
endif(UNIX)
