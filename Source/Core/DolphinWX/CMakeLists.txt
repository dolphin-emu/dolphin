set(LIBS	core
			${LZO}
			discio
			bdisasm
			inputcommon
			common
			audiocommon
			z
			sfml-network
			SDL
			${OPENGL_LIBRARIES}
			${XRANDR_LIBRARIES}
			${X11_LIBRARIES})

if(LIBAV_FOUND)
	set(LIBS	${LIBS} ${LIBAV_LIBRARIES})
endif()

if(wxWidgets_FOUND)
	set(SRCS	Src/ARCodeAddEdit.cpp
				Src/AboutDolphin.cpp
				Src/CheatsWindow.cpp
				Src/ConfigMain.cpp
				Src/Debugger/BreakpointDlg.cpp
				Src/Debugger/BreakpointView.cpp
				Src/Debugger/BreakpointWindow.cpp
				Src/Debugger/CodeView.cpp
				Src/Debugger/CodeWindow.cpp
				Src/Debugger/CodeWindowFunctions.cpp
				Src/Debugger/DSPDebugWindow.cpp
				Src/Debugger/DSPRegisterView.cpp
				Src/Debugger/DebuggerPanel.cpp
				Src/Debugger/DebuggerUIUtil.cpp
				Src/Debugger/JitWindow.cpp
				Src/Debugger/MemoryCheckDlg.cpp
				Src/Debugger/MemoryView.cpp
				Src/Debugger/MemoryWindow.cpp
				Src/Debugger/RegisterView.cpp
				Src/Debugger/RegisterWindow.cpp
				Src/FifoPlayerDlg.cpp
				Src/Frame.cpp
				Src/FrameAui.cpp
				Src/FrameTools.cpp
				Src/GameListCtrl.cpp
				Src/GeckoCodeDiag.cpp
				Src/HotkeyDlg.cpp
				Src/ISOFile.cpp
				Src/ISOProperties.cpp
				Src/InputConfigDiag.cpp
				Src/InputConfigDiagBitmaps.cpp
				Src/LogConfigWindow.cpp
				Src/LogWindow.cpp
				Src/Main.cpp
				Src/MemcardManager.cpp
				Src/MemoryCards/WiiSaveCrypted.cpp
				Src/NetWindow.cpp
				Src/PHackSettings.cpp
				Src/PatchAddEdit.cpp
				Src/TASInputDlg.cpp
				Src/UDPConfigDiag.cpp
				Src/VideoConfigDiag.cpp
				Src/WXInputBase.cpp
				Src/WiimoteConfigDiag.cpp
				Src/WxUtils.cpp)

	set(WXLIBS ${wxWidgets_LIBRARIES}
				${GTK2_LIBRARIES})
else()
	set(SRCS	Src/MainNoGUI.cpp)
endif()

if(WIN32)
	set(SRCS ${SRCS} Src/stdafx.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	list(APPEND LIBS
		${APPKIT_LIBRARY}
		${AU_LIBRARY}
		${COREAUDIO_LIBRARY}
		${COREFUND_LIBRARY}
		${CORESERV_LIBRARY}
		${IOB_LIBRARY}
		${IOK_LIBRARY}
		)
	if(wxWidgets_FOUND)
		list(APPEND LIBS
			${APPSERV_LIBRARY}
			${COCOA_LIBRARY}
			)
	endif()
else()
	set(SRCS ${SRCS} Src/X11Utils.cpp)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
        ${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS ${LIBS} usbhid)
endif()

if(wxWidgets_FOUND)
	set(DOLPHIN_EXE dolphin-emu)
else()
	set(DOLPHIN_EXE dolphin-emu-nogui)
endif()

add_executable(${DOLPHIN_EXE} ${SRCS})
target_link_libraries(${DOLPHIN_EXE} ${LIBS} ${WXLIBS})
install(TARGETS ${DOLPHIN_EXE} RUNTIME DESTINATION ${bindir})

