set(SRCS	Src/CodeView.cpp
			Src/DebuggerUIUtil.cpp
			Src/MemoryView.cpp)

add_library(debugger_ui_util STATIC ${SRCS})
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-fPIC)
endif()
