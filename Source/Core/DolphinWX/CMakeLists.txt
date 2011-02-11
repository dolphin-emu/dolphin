set(SRCS	Src/BootManager.cpp)

set(LIBS	core
			lzo2
			discio
			bdisasm
			inputcommon
			common
			audiocommon
			z
			sfml-network
			SDL
			GL
			${XRANDR_LIBRARIES}
			${X11_LIBRARIES})

if(wxWidgets_FOUND)
	set(SRCS	${SRCS}
				Src/AboutDolphin.cpp
				Src/ARCodeAddEdit.cpp
				Src/CheatsWindow.cpp
				Src/ConfigMain.cpp
				Src/Frame.cpp
				Src/FrameAui.cpp
				Src/FrameTools.cpp
				Src/GameListCtrl.cpp
				Src/GeckoCodeDiag.cpp
				Src/HotkeyDlg.cpp
				Src/InputConfigDiag.cpp
				Src/InputConfigDiagBitmaps.cpp
				Src/ISOFile.cpp
				Src/ISOProperties.cpp
				Src/LogWindow.cpp
				Src/Main.cpp
				Src/MemcardManager.cpp
				Src/MemoryCards/GCMemcard.cpp
				Src/NetWindow.cpp
				Src/PatchAddEdit.cpp
				Src/PHackSettings.cpp
				Src/UDPConfigDiag.cpp
				Src/WiimoteConfigDiag.cpp
				Src/MemoryCards/WiiSaveCrypted.cpp
				Src/WXInputBase.cpp
				Src/WxUtils.cpp
				Src/BreakpointDlg.cpp
				Src/BreakpointView.cpp
				Src/BreakpointWindow.cpp
				Src/CodeView.cpp
				Src/CodeWindow.cpp
				Src/CodeWindowFunctions.cpp
				Src/DebuggerPanel.cpp
				Src/DebuggerUIUtil.cpp
				Src/DSPDebugWindow.cpp
				Src/DSPRegisterView.cpp
				Src/JitWindow.cpp
				Src/MemoryCheckDlg.cpp
				Src/MemoryView.cpp
				Src/MemoryWindow.cpp
				Src/RegisterView.cpp
				Src/RegisterWindow.cpp
				Src/VideoConfigDiag.cpp)

	set(WXLIBS ${wxWidgets_LIBRARIES}
				${GTK2_LIBRARIES})

else(wxWidgets_FOUND)
	set(SRCS	${SRCS}
				Src/MainNoGUI.cpp)
endif()

if(WIN32)
	set(SRCS ${SRCS} Src/stdafx.cpp)
elseif((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND NOT wxWidgets_FOUND)
	# TODO
elseif((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND wxWidgets_FOUND)
	# TODO
else()
	set(SRCS ${SRCS} Src/X11Utils.cpp)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
        ${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS ${LIBS} usbhid)
endif()

set(EXEGUI dolphin-emu)
set(EXENOGUI dolphin-emu-nogui)

if(wxWidgets_FOUND)
	add_executable(${EXEGUI} ${SRCS})
	target_link_libraries(${EXEGUI} ${LIBS} ${WXLIBS})
	install(TARGETS ${EXEGUI} RUNTIME DESTINATION ${bindir})
else()
	add_executable(${EXENOGUI} ${SRCS})
	target_link_libraries(${EXENOGUI} ${LIBS})
	install(TARGETS ${EXENOGUI} RUNTIME DESTINATION ${bindir})
endif()
