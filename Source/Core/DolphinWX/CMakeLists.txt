set(SRCS	Src/BootManager.cpp)

set(LIBS	core
			lzo2
			discio
			bdisasm
			inputcommon
			common
			lua
			z
			sfml-network)

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

if(WIN32)
	set(SRCS ${SRCS} Src/stdafx.cpp)
elseif(APPLE AND NOT wxWidgets_FOUND)
	# TODO
elseif(APPLE AND wxWidgets_FOUND)
	# TODO
else()
	set(SRCS ${SRCS} Src/X11Utils.cpp)
endif()

set(LIBS ${LIBS} SDL)
set(EXEGUI dolphin-emu)
set(EXENOGUI dolphin-emu-nogui)

if(wxWidgets_FOUND)
	add_library(memcard STATIC ${MEMCARDSRCS})
	add_executable(${EXEGUI} ${SRCS})
	target_link_libraries(${EXEGUI} ${LIBS} ${WXLIBS})
	install(TARGETS ${EXEGUI} RUNTIME DESTINATION bin) # TODO: Move to root dir?
else()
	add_executable(${EXENOGUI} ${SRCS})
	target_link_libraries(${EXENOGUI} ${LIBS})
	install(TARGETS ${EXENOGUI} RUNTIME DESTINATION bin) # TODO: Move to root dir?
endif()
