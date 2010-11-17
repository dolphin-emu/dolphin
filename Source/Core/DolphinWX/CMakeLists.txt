set(SRCS	Src/BootManager.cpp)

set(LIBS	core
			lzo2
			discio
			bdisasm
			inputcommon
			common
			lua
			z
			sfml-network
			wiiuse
			SDL
			GL)

if(wxWidgets_FOUND)
	set(MEMCARDSRCS	Src/MemcardManager.cpp
					Src/MemoryCards/GCMemcard.cpp
					Src/WxUtils.cpp)

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
				Src/LuaWindow.cpp
				Src/Main.cpp
				Src/NetPlay.cpp
				Src/NetPlayClient.cpp
				Src/NetPlayServer.cpp
				Src/NetWindow.cpp
				Src/PatchAddEdit.cpp
				Src/UDPConfigDiag.cpp
				Src/WiimoteConfigDiag.cpp
				Src/MemoryCards/WiiSaveCrypted.cpp)

	set(WXLIBS	debwx
				debugger_ui_util
				inputuicommon
				memcard
				${wxWidgets_LIBRARIES})

else(wxWidgets_FOUND)
	set(SRCS	${SRCS}
				Src/MainNoGUI.cpp
				Src/cmdline.c)
endif()

if(XRANDR_FOUND)
	set(LIBS ${LIBS} ${XRANDR_LIBRARIES})
endif(XRANDR_FOUND)

if(WIN32)
	set(SRCS ${SRCS} Src/stdafx.cpp)
elseif((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND NOT wxWidgets_FOUND)
	# TODO
elseif((${CMAKE_SYSTEM_NAME} MATCHES "Darwin") AND wxWidgets_FOUND)
	# TODO
else()
	set(SRCS ${SRCS} Src/X11Utils.cpp)
endif()

set(EXEGUI dolphin-emu)
set(EXENOGUI dolphin-emu-nogui)

if(wxWidgets_FOUND)
	add_library(memcard STATIC ${MEMCARDSRCS})
	add_executable(${EXEGUI} ${SRCS})
	target_link_libraries(${EXEGUI} ${LIBS} ${WXLIBS})
	install(TARGETS ${EXEGUI} RUNTIME DESTINATION ${bindir})
else()
	add_executable(${EXENOGUI} ${SRCS})
	target_link_libraries(${EXENOGUI} ${LIBS})
	install(TARGETS ${EXENOGUI} RUNTIME DESTINATION ${bindir})
endif()
