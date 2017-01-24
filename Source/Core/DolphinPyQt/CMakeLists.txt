include_directories(${CMAKE_CURRENT_BINARY_DIR} ${PYTHON_INCLUDE_DIRS})

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
set(CMAKE_AUTOMOC ON)

set(SRCS
	Main.cpp
        wrap_dolphin.cpp
        sipsip_dolphincmodule.cpp
        sipsip_dolphinMenuBar.cpp
        sipsip_dolphinToolBar.cpp
        sipsip_dolphinGameList.cpp
        sipsip_dolphinPathDialog.cpp
        sipsip_dolphinSettingsWindow.cpp
        sipsip_dolphinSettings.cpp

	../DolphinQt2/AboutDialog.cpp
#	Host.cpp
#	InDevelopmentWarning.cpp
#	MainWindow.cpp
	../DolphinQt2/MenuBar.cpp
#	RenderWidget.cpp
	../DolphinQt2/Resources.cpp
	../DolphinQt2/Settings.cpp
	../DolphinQt2/ToolBar.cpp
	../DolphinQt2/Config/FilesystemWidget.cpp
	../DolphinQt2/Config/InfoWidget.cpp
	../DolphinQt2/Config/PathDialog.cpp
	../DolphinQt2/Config/PropertiesDialog.cpp
	../DolphinQt2/Config/SettingsWindow.cpp
	../DolphinQt2/GameList/GameFile.cpp
	../DolphinQt2/GameList/GameList.cpp
	../DolphinQt2/GameList/GameListModel.cpp
	../DolphinQt2/GameList/GameTracker.cpp
	../DolphinQt2/GameList/ListProxyModel.cpp
	../DolphinQt2/GameList/TableDelegate.cpp
	)

set(PYFILES
        pymain.py
        main_window.py
        render_widget.py
        host.py
        setupqt.py
        in_development_warning.py
        )

list(APPEND LIBS core uicommon)

set(DOLPHINPYQT_BINARY dolphin-emu-pyqt)

add_executable(${DOLPHINPYQT_BINARY} ${SRCS} ${UI_HEADERS})
target_link_libraries(${DOLPHINPYQT_BINARY} ${LIBS} Qt5::Widgets ${PYTHON_LIBRARIES})

if(APPLE)
	# Note: This is copied from DolphinQt2 copied from DolphinQt, based on the DolphinWX version.

	include(BundleUtilities)
	set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DOLPHINPYQT_BINARY}.app)

	# Ask for an application bundle.
	set_target_properties(${DOLPHINPYQT_BINARY} PROPERTIES
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
			execute_process(COMMAND ${CMAKE_SOURCE_DIR}/Tools/deploy-mac.py -p platforms/libqcocoa.dylib \"${BUNDLE_PATH}\")
			file(INSTALL ${CMAKE_SOURCE_DIR}/Data/Sys
				DESTINATION ${BUNDLE_PATH}/Contents/Resources
				)
			")
		add_custom_command(TARGET ${DOLPHINPYQT_BINARY} POST_BUILD
			COMMAND ${CMAKE_COMMAND} -P postprocess_bundle.cmake
			)
	else()
		add_custom_command(OUTPUT ${BUNDLE_PATH}/Contents/Resources/Sys
			COMMAND ln -nfs ${CMAKE_SOURCE_DIR}/Data/Sys ${BUNDLE_PATH}/Contents/Resources/Sys
			VERBATIM
			)
		add_custom_target(CopyDataIntoBundleQt ALL
			DEPENDS ${BUNDLE_PATH}/Contents/Resources/Sys
			)
	endif()
else()
	install(TARGETS ${DOLPHINPYQT_BINARY} RUNTIME DESTINATION ${bindir})
        install(FILES ${PYFILES} DESTINATION ${datadir}/python)
endif()
