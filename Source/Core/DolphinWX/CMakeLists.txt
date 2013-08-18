set(LIBS	core
			${LZO}
			discio
			bdisasm
			inputcommon
			common
			audiocommon
			z
			sfml-network
			${GTK2_LIBRARIES})
if(NOT ANDROID)
	if(USE_X11)
	  set(LIBS  ${LIBS} ${X11_LIBRARIES}
              ${XINPUT2_LIBRARIES}
	      ${XRANDR_LIBRARIES})
	endif()
	if(USE_WAYLAND)
	  set(LIBS  ${LIBS} ${WAYLAND_LIBRARIES}
		${XKBCOMMON_LIBRARIES})
	endif()

	link_directories(${CMAKE_PREFIX_PATH}/lib)

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
else()
	set(LIBS ${LIBS} png)
endif()

if(LIBAV_FOUND)
	set(LIBS	${LIBS} ${LIBAV_LIBRARIES})
endif()

if(wxWidgets_FOUND)
	set(SRCS 
				Src/ARCodeAddEdit.cpp
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
	if(ANDROID)
		set(SRCS Src/Android/TextureLoader.cpp
			 Src/Android/ButtonManager.cpp
			 Src/MainAndroid.cpp)
	else()
		set(SRCS Src/MainNoGUI.cpp)
	endif()
endif()

if(USE_EGL)
	set(SRCS ${SRCS} Src/GLInterface/Platform.cpp
		Src/GLInterface/EGL.cpp)	
	if(USE_WAYLAND)
		set(SRCS ${SRCS} Src/GLInterface/Wayland_Util.cpp)
	endif()
	if(USE_X11)
		set(SRCS ${SRCS} Src/GLInterface/X11_Util.cpp)
	endif()
else()
	if(WIN32)
		set(SRCS ${SRCS} Src/GLInterface/WGL.cpp)
	elseif(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
		set(SRCS ${SRCS} Src/GLInterface/AGL.cpp)
	else()
		set(SRCS ${SRCS} Src/GLInterface/GLX.cpp
			Src/GLInterface/X11_Util.cpp)

	endif()
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
elseif(USE_X11)
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

if(USE_UPNP)
  set(LIBS  ${LIBS} miniupnpc)
endif()

if(ANDROID)
	set(DOLPHIN_EXE main)
	add_library(${DOLPHIN_EXE} SHARED ${SRCS})
	target_link_libraries(${DOLPHIN_EXE} 
	log
	android
	"-Wl,--whole-archive"
	${LIBS}
	"-Wl,--no-whole-archive"
	)
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "mips")
		set (SO_TARGET "mips")
	endif()
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv7-a")
		set (SO_TARGET "armeabi-v7a")
	endif()
	if(${CMAKE_SYSTEM_PROCESSOR} MATCHES "armv5te")
		set (SO_TARGET "armeabi")
	endif()
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "i686")
		set (SO_TARGET "x86")
	endif()
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND mkdir ARGS -p ${CMAKE_SOURCE_DIR}/Source/Android/libs/${SO_TARGET}
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND cp ARGS ${CMAKE_SOURCE_DIR}/libs/${SO_TARGET}/lib${DOLPHIN_EXE}.so ${CMAKE_SOURCE_DIR}/Source/Android/libs/${SO_TARGET}/
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND cp ARGS ${CMAKE_SOURCE_DIR}/Data/Sys/GC/* ${CMAKE_SOURCE_DIR}/Source/Android/assets/
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND cp ARGS ${CMAKE_SOURCE_DIR}/Data/Sys/Wii/* ${CMAKE_SOURCE_DIR}/Source/Android/assets/
	)
else()
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
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${DOLPHIN_EXE})
