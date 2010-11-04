set(SRCS
	Src/Config.cpp
	Src/DSPDebugInterface.cpp
	Src/DSPHost.cpp
	Src/DSPSymbols.cpp
	Src/Globals.cpp
	Src/main.cpp
	Src/Tools.cpp)

set(LIBS dspcore audiocommon common)

if(wxWidgets_FOUND)
	set(SRCS
		${SRCS}
		Src/DSPConfigDlgLLE.cpp
		Src/Debugger/DSPDebugWindow.cpp
		Src/Debugger/DSPRegisterView.cpp)
	set(LIBS ${LIBS} debugger_ui_util)
endif(wxWidgets_FOUND)

add_library(Plugin_DSP_LLE SHARED ${SRCS})
target_link_libraries(Plugin_DSP_LLE ${LIBS})
install(TARGETS Plugin_DSP_LLE LIBRARY DESTINATION ${plugindir})

