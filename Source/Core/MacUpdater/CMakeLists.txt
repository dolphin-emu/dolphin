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
set(MacUpdater_BIN_DIR ${CMAKE_BINARY_DIR}/Binaries)
set(MacUpdater_BUNDLE_PATH ${MacUpdater_BIN_DIR}/${MacUpdater_NAME}.app)

add_dependencies(MacUpdater dolphin_scmrev)

set_target_properties(MacUpdater PROPERTIES
  MACOSX_BUNDLE true
  MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
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
  set(output $<TARGET_BUNDLE_DIR:MacUpdater>/Contents/Resources/${sb}c)
  set(input  ${CMAKE_CURRENT_SOURCE_DIR}/${sb})
  add_custom_command(TARGET MacUpdater POST_BUILD
    COMMAND ${IBTOOL} --errors --warnings --notices --output-format human-readable-text --compile ${output} ${input}
    DEPENDS ${input}
    COMMENT "Compiling Storyboard ${sb}...")
endforeach()

if(MACOS_CODE_SIGNING)
  if (MACOS_CODE_SIGNING_IDENTITY_UPDATER STREQUAL "")
    set(MACOS_CODE_SIGNING_IDENTITY_UPDATER "${MACOS_CODE_SIGNING_IDENTITY}")
  endif()

  add_custom_command(TARGET MacUpdater POST_BUILD
    COMMAND /usr/bin/codesign -f -s "${MACOS_CODE_SIGNING_IDENTITY_UPDATER}" --deep --options runtime $<TARGET_BUNDLE_DIR:MacUpdater>)
endif()
