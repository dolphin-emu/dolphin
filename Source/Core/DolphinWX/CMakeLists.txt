set(LIBS	core
			${LZO}
			discio
			bdisasm
			inputcommon
			common
			audiocommon
			z
			sfml-network
			${GTK2_LIBRARIES}
			${OPENGL_LIBRARIES}
			${XRANDR_LIBRARIES}
			${X11_LIBRARIES})

if(SDL2_FOUND)
	# Using shared SDL2
	set(LIBS	${LIBS} ${SDL2_LIBRARY})
else(SDL2_FOUND)
	if(SDL_FOUND)
		# Using shared SDL
		set(LIBS	${LIBS} ${SDL_LIBRARY})
	else(SDL_FOUND)
		# Using static SDL from Externals
		set(LIBS	${LIBS} SDL)
	endif()
endif()


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

	set(WXLIBS ${wxWidgets_LIBRARIES})
else()
	set(SRCS	Src/MainNoGUI.cpp)
endif()

if(WIN32)
	set(SRCS ${SRCS} Src/stdafx.cpp)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	# Link against OS X system frameworks.
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
	# Add resource files to application bundle.
	set(RESOURCES resources/Dolphin.icns)
	list(APPEND SRCS ${RESOURCES})
	set_source_files_properties(${RESOURCES} PROPERTIES
		MACOSX_PACKAGE_LOCATION Resources)
else()
	set(SRCS ${SRCS} Src/X11Utils.cpp)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "FreeBSD" OR
        ${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
	set(LIBS ${LIBS} usbhid)
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	set(DOLPHIN_EXE_BASE Dolphin)
else()
	set(DOLPHIN_EXE_BASE dolphin-emu)
endif()
if(wxWidgets_FOUND)
	set(DOLPHIN_EXE ${DOLPHIN_EXE_BASE})
else()
	set(DOLPHIN_EXE ${DOLPHIN_EXE_BASE}-nogui)
endif()

add_executable(${DOLPHIN_EXE} ${SRCS})
target_link_libraries(${DOLPHIN_EXE} ${LIBS} ${WXLIBS})

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	include(BundleUtilities)
	set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DOLPHIN_EXE}.app)

	# Ask for an application bundle.
	set_target_properties(${DOLPHIN_EXE} PROPERTIES
		MACOSX_BUNDLE true
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
		)

	# Install Cg framework into application bundle.
	copy_resolved_framework_into_bundle(
		# Our framework in "Externals" does not have "Versions/Current/" in
		# its path; work around the missing directory levels using "././".
		"${CMAKE_SOURCE_DIR}/Externals/Cg/Cg.framework/././Cg"
		"${BUNDLE_PATH}/Contents/Frameworks/Cg.framework/././Cg"
		)

	# Fix up the bundle after it is finished.
	# There does not seem to be an easy way to run CMake commands post-build,
	# so we invoke CMake again on a generated script.
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/postprocess_bundle.cmake "
		include(BundleUtilities)
		message(\"Fixing up application bundle: ${BUNDLE_PATH}\")
		set(BU_CHMOD_BUNDLE_ITEMS ON)
		fixup_bundle(\"${BUNDLE_PATH}\" \"\" \"\")
		")
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND ${CMAKE_COMMAND} -P postprocess_bundle.cmake
		)

	# Copy data files into application bundle.
	file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/copy_data_into_bundle.cmake "
		file(INSTALL ${CMAKE_SOURCE_DIR}/Data/Sys ${CMAKE_SOURCE_DIR}/Data/User
			DESTINATION ${BUNDLE_PATH}/Contents/Resources
			)
		file(GLOB TRANSLATION_FILES RELATIVE ${CMAKE_BINARY_DIR}
			${CMAKE_BINARY_DIR}/*.gmo
			)
		foreach(TRANSLATION_FILE \${TRANSLATION_FILES})
			string(REPLACE \".gmo\" \".lproj\" TRANSLATION_DIR
				\${TRANSLATION_FILE}
				)
			# It would be better to copy to the new name as a single action,
			# but I can't figure out a way to let CMake do that.
			file(COPY ${CMAKE_BINARY_DIR}/\${TRANSLATION_FILE}
				DESTINATION ${BUNDLE_PATH}/Contents/Resources/\${TRANSLATION_DIR}
				NO_SOURCE_PERMISSIONS
				)
			file(RENAME
				${BUNDLE_PATH}/Contents/Resources/\${TRANSLATION_DIR}/\${TRANSLATION_FILE}
				${BUNDLE_PATH}/Contents/Resources/\${TRANSLATION_DIR}/dolphin-emu.mo
				)
		endforeach(TRANSLATION_FILE)
		")
	add_custom_target(CopyDataIntoBundle ALL
		COMMAND ${CMAKE_COMMAND} -P copy_data_into_bundle.cmake
		VERBATIM
		)

	# Install bundle into systemwide /Applications directory.
	install(DIRECTORY ${BUNDLE_PATH} DESTINATION /Applications
		USE_SOURCE_PERMISSIONS
		)
else()
	install(TARGETS ${DOLPHIN_EXE} RUNTIME DESTINATION ${bindir})
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${DOLPHIN_EXE})
