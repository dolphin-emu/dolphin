set(GUI_SRCS
	AboutDolphin.cpp
	ControllerConfigDiag.cpp
	Cheats/ActionReplayCodesPanel.cpp
	Cheats/ARCodeAddEdit.cpp
	Cheats/CheatSearchTab.cpp
	Cheats/CheatsWindow.cpp
	Cheats/CreateCodeDialog.cpp
	Cheats/GeckoCodeDiag.cpp
	Config/AddUSBDeviceDiag.cpp
	Config/AdvancedConfigPane.cpp
	Config/AudioConfigPane.cpp
	Config/ConfigMain.cpp
	Config/GameCubeConfigPane.cpp
	Config/GCAdapterConfigDiag.cpp
	Config/GeneralConfigPane.cpp
	Config/InterfaceConfigPane.cpp
	Config/PathConfigPane.cpp
	Config/WiiConfigPane.cpp
	Debugger/BreakpointDlg.cpp
	Debugger/BreakpointView.cpp
	Debugger/BreakpointWindow.cpp
	Debugger/CodeView.cpp
	Debugger/CodeWindow.cpp
	Debugger/CodeWindowFunctions.cpp
	Debugger/DSPDebugWindow.cpp
	Debugger/DSPRegisterView.cpp
	Debugger/DebuggerPanel.cpp
	Debugger/DebuggerUIUtil.cpp
	Debugger/JitWindow.cpp
	Debugger/MemoryCheckDlg.cpp
	Debugger/MemoryView.cpp
	Debugger/MemoryWindow.cpp
	Debugger/RegisterView.cpp
	Debugger/RegisterWindow.cpp
	Debugger/WatchView.cpp
	Debugger/WatchWindow.cpp
	ISOProperties/FilesystemPanel.cpp
	ISOProperties/InfoPanel.cpp
	ISOProperties/ISOProperties.cpp
	NetPlay/ChangeGameDialog.cpp
	NetPlay/MD5Dialog.cpp
	NetPlay/NetPlayLauncher.cpp
	NetPlay/NetPlaySetupFrame.cpp
	NetPlay/NetWindow.cpp
	NetPlay/PadMapDialog.cpp
	Input/InputConfigDiag.cpp
	Input/InputConfigDiagBitmaps.cpp
	Input/HotkeyInputConfigDiag.cpp
	Input/GCPadInputConfigDiag.cpp
	Input/MicButtonConfigDiag.cpp
	Input/GCKeyboardInputConfigDiag.cpp
	Input/WiimoteInputConfigDiag.cpp
	Input/NunchukInputConfigDiag.cpp
	Input/ClassicInputConfigDiag.cpp
	Input/GuitarInputConfigDiag.cpp
	Input/DrumsInputConfigDiag.cpp
	Input/TurntableInputConfigDiag.cpp
	DolphinSlider.cpp
	FifoPlayerDlg.cpp
	Frame.cpp
	FrameAui.cpp
	FrameTools.cpp
	GameListCtrl.cpp
	ISOFile.cpp
	LogConfigWindow.cpp
	LogWindow.cpp
	Main.cpp
	MainMenuBar.cpp
	MainToolBar.cpp
	MemcardManager.cpp
	PatchAddEdit.cpp
	PostProcessingConfigDiag.cpp
	SoftwareVideoConfigDialog.cpp
	TASInputDlg.cpp
	VideoConfigDiag.cpp
	WxEventUtils.cpp
	WXInputBase.cpp
	WxUtils.cpp)

set(NOGUI_SRCS MainNoGUI.cpp)

if(USE_X11)
	set(GUI_SRCS ${GUI_SRCS} X11Utils.cpp)
	set(NOGUI_SRCS ${NOGUI_SRCS} X11Utils.cpp)
endif()

set(WXLIBS ${wxWidgets_LIBRARIES})

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
	set(WXLIBS ${WXLIBS} dl)
endif()

list(APPEND LIBS core uicommon)

if(APPLE)
	if(wxWidgets_FOUND)
		list(APPEND WXLIBS
			${APPSERV_LIBRARY}
			${COCOA_LIBRARY}
			)
	endif()
	# Add resource files to application bundle.
	set(RESOURCES resources/Dolphin.icns)
	list(APPEND SRCS ${RESOURCES})
	set_source_files_properties(${RESOURCES} PROPERTIES
		MACOSX_PACKAGE_LOCATION Resources)
endif()

if(WIN32)
	list(APPEND SRCS
		DolphinWX.manifest
		DolphinWX.rc
	)
endif()

if(APPLE)
	set(DOLPHIN_EXE_BASE Dolphin)
else()
	set(DOLPHIN_EXE_BASE dolphin-emu)
endif()

set(DOLPHIN_EXE ${DOLPHIN_EXE_BASE})


if(wxWidgets_FOUND)
	add_executable(${DOLPHIN_EXE} ${SRCS} ${GUI_SRCS})
	target_link_libraries(${DOLPHIN_EXE} ${LIBS} ${WXLIBS})

	# Handle localization
	find_package(Gettext)
	if(GETTEXT_MSGMERGE_EXECUTABLE AND GETTEXT_MSGFMT_EXECUTABLE)
		set(pot_file "${CMAKE_SOURCE_DIR}/Languages/po/dolphin-emu.pot")
		file(GLOB LINGUAS ${CMAKE_SOURCE_DIR}/Languages/po/*.po)

		target_sources(${DOLPHIN_EXE} PRIVATE ${pot_file} ${LINGUAS})
		source_group("Localization" FILES ${LINGUAS})
		source_group("Localization\\\\Generated" FILES ${pot_file})

		foreach(po ${LINGUAS})
			get_filename_component(lang ${po} NAME_WE)
			set(mo_dir ${CMAKE_CURRENT_BINARY_DIR}/${lang})
			set(mo ${mo_dir}/dolphin-emu.mo)

			target_sources(${DOLPHIN_EXE} PRIVATE ${mo})
			source_group("Localization\\\\Generated" FILES ${mo})

			if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
				set_source_files_properties(${mo} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${lang}.lproj")
			else()
				install(FILES ${mo} DESTINATION share/locale/${lang}/LC_MESSAGES)
			endif()

			add_custom_command(OUTPUT ${mo}
				COMMAND mkdir -p ${mo_dir}
				COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} --quiet --update --backup=none -s ${po} ${pot_file}
				COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${mo} ${po}
				DEPENDS ${po}
			)
		endforeach()
	endif()

	if(APPLE)
		set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DOLPHIN_EXE}.app)

		# Ask for an application bundle.
		set_target_properties(${DOLPHIN_EXE} PROPERTIES
			MACOSX_BUNDLE true
			MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
			)

		# Copy resources in the bundle
		set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Data/Sys")
		file(GLOB_RECURSE resources RELATIVE "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Data/Sys/*")
		foreach(res ${resources})
			target_sources(${DOLPHIN_EXE} PRIVATE "${CMAKE_SOURCE_DIR}/Data/${res}")
			get_filename_component(resdir "${res}" DIRECTORY)
			set_source_files_properties("${CMAKE_SOURCE_DIR}/Data/${res}" PROPERTIES
				MACOSX_PACKAGE_LOCATION "Resources/${resdir}")
			source_group("Resources" FILES "${CMAKE_SOURCE_DIR}/Data/${res}")
		endforeach()

		# Update library references to make the bundle portable
		include(DolphinPostprocessBundle)
		dolphin_postprocess_bundle(${DOLPHIN_EXE})

		# Install bundle into systemwide /Applications directory.
		install(TARGETS ${DOLPHIN_EXE} DESTINATION /Applications)
	elseif(WIN32)
		set_target_properties(${DOLPHIN_EXE} PROPERTIES
			WIN32_EXECUTABLE ON
		)
		add_custom_command(TARGET ${DOLPHIN_EXE}
			COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Data/Sys $<TARGET_FILE_DIR:${DOLPHIN_EXE}>/Sys
		)
	else()
		install(TARGETS ${DOLPHIN_EXE} RUNTIME DESTINATION ${bindir})
	endif()

	set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${DOLPHIN_EXE})
endif()

if(USE_X11 OR ENABLE_HEADLESS)
	set(DOLPHIN_NOGUI_EXE ${DOLPHIN_EXE_BASE}-nogui)
	add_executable(${DOLPHIN_NOGUI_EXE} ${SRCS} ${NOGUI_SRCS})
	target_link_libraries(${DOLPHIN_NOGUI_EXE} ${LIBS})
	set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${DOLPHIN_NOGUI_EXE})
	install(TARGETS ${DOLPHIN_NOGUI_EXE} RUNTIME DESTINATION ${bindir})
endif()
