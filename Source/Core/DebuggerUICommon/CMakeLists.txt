set(SRCS	Src/CodeView.cpp
		Src/DebuggerUIUtil.cpp
		Src/MemoryView.cpp)

add_library(debugger_ui_util STATIC ${SRCS})
target_link_libraries(debugger_ui_util ${wxWidgets_LIBRARIES})
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-fPIC)
endif()
