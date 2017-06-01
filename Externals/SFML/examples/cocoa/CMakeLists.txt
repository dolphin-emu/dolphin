
set(SRCROOT ${PROJECT_SOURCE_DIR}/examples/cocoa)

# all source files
set(SRC ${SRCROOT}/CocoaAppDelegate.h
        ${SRCROOT}/CocoaAppDelegate.mm
        ${SRCROOT}/NSString+stdstring.h
        ${SRCROOT}/NSString+stdstring.mm
        ${SRCROOT}/main.m)

# all XIB files
set(XIBS MainMenu)

# all resource files
set(RESOURCES ${SRCROOT}/resources/logo.png
              ${SRCROOT}/resources/icon.icns
              ${SRCROOT}/resources/sansation.ttf
              ${SRCROOT}/resources/blue.png
              ${SRCROOT}/resources/green.png
              ${SRCROOT}/resources/red.png
              ${SRCROOT}/resources/Credits.rtf)

# define the cocoa target and customize it
add_executable(cocoa MACOSX_BUNDLE ${SRC} ${RESOURCES})
set_source_files_properties(${RESOURCES} PROPERTIES
                            MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(cocoa PROPERTIES
                      MACOSX_BUNDLE_INFO_PLIST ${SRCROOT}/resources/Cocoa-Info.plist)
target_link_libraries(cocoa "-framework Cocoa -framework Foundation"
                            sfml-system sfml-window sfml-graphics)

# set the target's folder (for IDEs that support it, e.g. Visual Studio)
set_target_properties(cocoa PROPERTIES FOLDER "Examples")

# compile XIB files
find_program(IBTOOL ibtool HINTS "/usr/bin" "${OSX_DEVELOPER_ROOT}/usr/bin")
if(${IBTOOL} STREQUAL "IBTOOL-NOTFOUND")
  message(FATAL_ERROR "ibtool is required to compile .xib files but wasn't found.")
endif()
set(RESOURCE_PATH "cocoa.app/Contents/Resources")
set(XIB_OUTPUT_PATH "${RESOURCE_PATH}/")
set(XIB_INPUT_PATH "${SRCROOT}/")
foreach(XIB ${XIBS})
    add_custom_command(TARGET cocoa
                       POST_BUILD
                       COMMAND ${IBTOOL} --errors
                                         --output-format human-readable-text
                                         --compile ${XIB_OUTPUT_PATH}/${XIB}.nib
                                         ${XIB_INPUT_PATH}/${XIB}.xib
                       COMMENT "Compiling ${XIB}.xib")
                       # deactivated options: --warnings --notices
endforeach()

# add install rule
install(TARGETS cocoa
        BUNDLE DESTINATION ${INSTALL_MISC_DIR}/examples/cocoa
        COMPONENT examples)

#
# define the cocoa target
# sfml_add_example is not compatible with application bundles !
#
#sfml_add_example(cocoa
#                 SOURCES ${SRC}
#                 DEPENDS sfml-system sfml-window sfml-graphics)
#

