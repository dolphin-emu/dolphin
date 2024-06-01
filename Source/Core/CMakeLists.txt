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

# Build SlippiRustExtensions for DolphinQt and DolphinNoGui
#
#set(RUST_FEATURES "")
#if(DIS_PLAYBACK)
#    set(RUST_FEATURES "playback")
#endif()

corrosion_import_crate(MANIFEST_PATH ${CMAKE_SOURCE_DIR}/Externals/SlippiRustExtensions/Cargo.toml ${RUST_FEATURES})
