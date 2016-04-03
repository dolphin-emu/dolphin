set(SRCS
    Main.mm)

list(APPEND LIBS core uicommon ${UIKIT_LIBRARY} ${COREANIMATION_LIBRARY})

set(RESOURCES
    resources/AppIcon@3x.png
    resources/AppIcon@2x.png
    resources/AppIcon.png)
list(APPEND SRCS ${RESOURCES})
set_source_files_properties(${RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION "")

set(DOLPHIN_EXE_BASE Dolphin)
set(DOLPHIN_EXE ${DOLPHIN_EXE_BASE})

add_executable(${DOLPHIN_EXE} ${SRCS})
target_link_libraries(${DOLPHIN_EXE} ${LIBS})

include(BundleUtilities)
set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DOLPHIN_EXE}.app)

# Build an application bundle.
set_target_properties(${DOLPHIN_EXE} PROPERTIES
	MACOSX_BUNDLE true
	MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
	)

if("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "aarch64|arm64" AND NOT "${IPHONEOS_SKIP_CODE_SIGNING}")
    if("${IPHONEOS_CODE_SIGNING_IDENTITY}" STREQUAL "")
        message(FATAL_ERROR
                "Building for device requires a code signing identity.\n"
                "This is the name of a key and certificate pair in your keychain.\n"
                "Specify the identity with -DIPHONEOS_CODE_SIGNING_IDENTITY= to CMake.")
    endif()
    if("${IPHONEOS_TEAM_IDENTIFIER}" STREQUAL "")
        message(FATAL_ERROR
                "Building for device requires a team identifier.\n"
                "This is the \"prefix\" listed under \"Identifiers\" in the provisioning portal.\n"
                "Specify the identity with -DIPHONEOS_TEAM_IDENTIFIER= to CMake.")
    endif()

    # Generate entitlements.
    file(READ "Entitlements.xml.in" ENTITLEMENTS_CONTENTS)
    string(REPLACE "TEAM_IDENTIFIER" "${IPHONEOS_TEAM_IDENTIFIER}" ENTITLEMENTS_CONTENTS "${ENTITLEMENTS_CONTENTS}")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/Entitlements.xml" "${ENTITLEMENTS_CONTENTS}")

    # Code sign.
    set(CODESIGN_COMMAND codesign -f -s "${IPHONEOS_CODE_SIGNING_IDENTITY}" --entitlements "${CMAKE_CURRENT_BINARY_DIR}/Entitlements.xml" "${BUNDLE_PATH}")
    add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
                       COMMAND ${CODESIGN_COMMAND}
                       VERBATIM)
endif()
