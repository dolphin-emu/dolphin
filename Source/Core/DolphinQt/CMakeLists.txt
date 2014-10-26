# because of generated UI files
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
set(CMAKE_AUTOMOC ON)

set(SRCS
	AboutDialog.cpp
	AboutDialog.h
	Host.cpp
	Main.cpp
	MainWindow.cpp
	MainWindow.h
	SystemInfo.cpp
	Utils/Resources.cpp
	Utils/Utils.cpp
	VideoInterface/VideoInterface.cpp
	)

set(UIS
	AboutDialog.ui
	MainWindow.ui
	SystemInfo.ui
	)

set(LIBS core uicommon)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	# Link against OS X system frameworks.
	list(APPEND LIBS
		${APPKIT_LIBRARY}
		${AU_LIBRARY}
		${COREAUDIO_LIBRARY}
		${COREFUND_LIBRARY}
		${CORESERV_LIBRARY}
		${IOK_LIBRARY}
		${FORCEFEEDBACK}
		)
	set(DOLPHINQT_BINARY DolphinQt)
else()
	set(DOLPHINQT_BINARY dolphin-emu-qt)
endif()

qt5_wrap_ui(UI_HEADERS ${UIS})
add_executable(${DOLPHINQT_BINARY} ${SRCS} ${UI_HEADERS})
target_link_libraries(${DOLPHINQT_BINARY} ${LIBS})
qt5_use_modules(${DOLPHINQT_BINARY} Widgets)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
	# Note: This is based on the DolphinWX version.

	include(BundleUtilities)
	set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DOLPHINQT_BINARY}.app)

	# Ask for an application bundle.
	set_target_properties(${DOLPHINQT_BINARY} PROPERTIES
		MACOSX_BUNDLE true
		MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
		)

	# get rid of any old copies
	file (REMOVE_RECURSE ${BUNDLE_PATH}/Contents/Resources/Sys)
	if(NOT SKIP_POSTPROCESS_BUNDLE)
		# Fix up the bundle after it is finished.
		# There does not seem to be an easy way to run CMake commands post-build,
		# so we invoke CMake again on a generated script.
		file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/postprocess_bundle.cmake "
			include(BundleUtilities)
			message(\"Fixing up application bundle: ${BUNDLE_PATH}\")
			message(\"(Note: This is only necessary to produce a redistributable binary.\")
			message(\"To skip, pass -DSKIP_POSTPROCESS_BUNDLE=1 to cmake.)\")
			set(BU_CHMOD_BUNDLE_ITEMS ON)
			fixup_bundle(\"${BUNDLE_PATH}\" \"\" \"\")
			file(INSTALL ${CMAKE_SOURCE_DIR}/Data/Sys
				DESTINATION ${BUNDLE_PATH}/Contents/Resources
				)
			")
		add_custom_command(TARGET ${DOLPHINQT_BINARY} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -P postprocess_bundle.cmake
			)
	else()
		add_custom_command(OUTPUT ${BUNDLE_PATH}/Contents/Resources/Sys
			COMMAND ln -nfs ${CMAKE_SOURCE_DIR}/Data/Sys ${BUNDLE_PATH}/Contents/Resources/Sys
			VERBATIM
			)
		add_custom_target(CopyDataIntoBundle ALL
			DEPENDS ${BUNDLE_PATH}/Contents/Resources/Sys
			)
	endif()
endif()
