add_subdirectory(AudioCommon)
add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(DiscIO)
add_subdirectory(InputCommon)
add_subdirectory(UICommon)
add_subdirectory(VideoCommon)
add_subdirectory(VideoBackends)

if(ENABLE_NOGUI)
  add_subdirectory(DolphinNoGUI)
endif()

if(ENABLE_CLI_TOOL)
  add_subdirectory(DolphinTool)
endif()

if(ENABLE_QT)
  add_subdirectory(DolphinQt)
endif()

if (APPLE OR WIN32)
  add_subdirectory(UpdaterCommon)
endif()

if (APPLE AND ENABLE_AUTOUPDATE)
  add_subdirectory(MacUpdater)
endif()

if (WIN32 AND ENABLE_AUTOUPDATE)
  add_subdirectory(WinUpdater)
endif()

if (APPLE AND ENABLE_QT)
  set(DOLPHIN_MAC_BUNDLE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Dolphin.app")

  add_custom_target(build_final_bundle ALL
    COMMAND ${CMAKE_COMMAND} -E remove_directory
    ${DOLPHIN_MAC_BUNDLE}
    
    COMMAND cp -R
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DolphinQt.app
    ${DOLPHIN_MAC_BUNDLE}

    # HACK: The Updater does not support setting the executable bit on new files,
    # so don't use the new executable name, and instead continue to use "Dolphin".
    COMMAND ${CMAKE_COMMAND} -E rename
    ${DOLPHIN_MAC_BUNDLE}/Contents/MacOS/DolphinQt
    ${DOLPHIN_MAC_BUNDLE}/Contents/MacOS/Dolphin

    COMMAND plutil
    -replace CFBundleExecutable -string Dolphin
    ${DOLPHIN_MAC_BUNDLE}/Contents/Info.plist
    
    DEPENDS dolphin-emu)
  
  if (ENABLE_AUTOUPDATE)
    add_dependencies(build_final_bundle MacUpdater)

    add_custom_command(TARGET build_final_bundle
      POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory
      "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers"

      COMMAND cp -R
      "$<TARGET_BUNDLE_DIR:MacUpdater>"
      "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers/Dolphin Updater.app")

    if (MACOS_CODE_SIGNING)
      add_custom_command(TARGET build_final_bundle
        POST_BUILD
        COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
        "-t"
        "${MACOS_CODE_SIGNING_IDENTITY}"
        "${DOLPHIN_MAC_BUNDLE}/Contents/Helpers/Dolphin Updater.app")
    endif()
  endif()

  if (MACOS_CODE_SIGNING)
    add_custom_command(TARGET build_final_bundle
      POST_BUILD
      COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
      "-t"
      "-e" "${CMAKE_SOURCE_DIR}/Source/Core/DolphinQt/DolphinEmu$<$<CONFIG:Debug>:Debug>.entitlements"
      "${MACOS_CODE_SIGNING_IDENTITY}"
      "${DOLPHIN_MAC_BUNDLE}")
  endif()
endif()
