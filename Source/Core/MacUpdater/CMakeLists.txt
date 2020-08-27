set(STORYBOARDS Main.storyboard)

set(SOURCES
  main.m
  AppDelegate.h
  AppDelegate.mm
  ViewController.h
  ViewController.m
  MacUI.mm
  ${STORYBOARDS}
)

add_executable(MacUpdater ${SOURCES})

set(MacUpdater_NAME "Dolphin Updater")

set_target_properties(MacUpdater PROPERTIES
  MACOSX_BUNDLE true
  MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
  OUTPUT_NAME ${MacUpdater_NAME})

target_compile_options(MacUpdater PRIVATE -x objective-c++)

# Copy icon into the bundle
target_sources(MacUpdater PRIVATE "${CMAKE_SOURCE_DIR}/Data/Dolphin.icns")
set_source_files_properties("${CMAKE_SOURCE_DIR}/Data/Dolphin.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

target_link_libraries(MacUpdater PRIVATE
    "-framework Cocoa"
    "-framework AppKit"
    "-framework CoreData"
    "-framework Foundation"
	uicommon
	updatercommon
)

# Compile storyboards (Adapted from https://gitlab.kitware.com/cmake/community/wikis/doc/tutorials/OSX-InterfaceBuilderFiles)

# Make sure we can find the 'ibtool' program. If we can NOT find it we
# skip generation of this project
find_program(IBTOOL ibtool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")
if (${IBTOOL} STREQUAL "IBTOOL-NOTFOUND")
  message(SEND_ERROR "ibtool can not be found and is needed to compile the .storyboard files. It should have been installed with
                    the Apple developer tools. The default system paths were searched in addition to ${OSX_DEVELOPER_ROOT}/usr/bin")
endif()

foreach(sb ${STORYBOARDS})
  set(MacUpdater_BIN_DIR ${CMAKE_BINARY_DIR}/Binaries)

  if (CMAKE_GENERATOR STREQUAL Xcode)
    string(APPEND MacUpdater_BIN_DIR "/\${CONFIGURATION}")
  endif()

  add_custom_command(TARGET MacUpdater POST_BUILD
                     COMMAND ${IBTOOL} --errors --warnings --notices --output-format human-readable-text
                                       --compile ${MacUpdater_BIN_DIR}/${MacUpdater_NAME}.app/Contents/Resources/${sb}c
                     ${CMAKE_CURRENT_SOURCE_DIR}/${sb}
                     COMMENT "Compiling Storyboard ${sb}...")
endforeach()

