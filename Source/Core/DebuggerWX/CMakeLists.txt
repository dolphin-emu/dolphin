set(SRCS	Src/BreakpointDlg.cpp
			Src/BreakpointView.cpp
			Src/BreakpointWindow.cpp
			Src/CodeWindow.cpp
			Src/CodeWindowFunctions.cpp
			Src/DSPDebugWindow.cpp
			Src/DSPDebugWindow.h
			Src/DSPRegisterView.cpp
			Src/DSPRegisterView.h
			Src/JitWindow.cpp
			Src/MemoryCheckDlg.cpp
			Src/MemoryWindow.cpp
			Src/RegisterView.cpp
			Src/RegisterWindow.cpp)

add_library(debwx STATIC ${SRCS})
target_link_libraries(debwx common core debugger_ui_util)
