add_library(main SHARED
  Cheats/ARCheat.cpp
  Cheats/Cheats.h
  Cheats/GeckoCheat.cpp
  Cheats/GraphicsMod.cpp
  Cheats/GraphicsModGroup.cpp
  Cheats/PatchCheat.cpp
  Config/ConfigChangedCallback.cpp
  Config/NativeConfig.cpp
  Config/PostProcessing.cpp
  GameList/GameFile.cpp
  GameList/GameFile.h
  GameList/GameFileCache.cpp
  Host.cpp
  Host.h
  InfinityConfig.cpp
  Input/Control.cpp
  Input/Control.h
  Input/ControlGroup.cpp
  Input/ControlGroup.h
  Input/ControlReference.cpp
  Input/ControlReference.h
  Input/CoreDevice.cpp
  Input/CoreDevice.h
  Input/EmulatedController.cpp
  Input/EmulatedController.h
  Input/InputOverrider.cpp
  Input/MappingCommon.cpp
  Input/NumericSetting.cpp
  Input/NumericSetting.h
  MainAndroid.cpp
  RiivolutionPatches.cpp
  SkylanderConfig.cpp
  WiiUtils.cpp
  GpuDriver.cpp
)

target_link_libraries(main
PRIVATE
  androidcommon
  common
  core
  inputcommon
  uicommon
)

target_link_libraries(main
PRIVATE
  android
  log
  "-Wl,--no-warn-mismatch"
  "-Wl,--whole-archive"
  "-Wl,--no-whole-archive"
)

target_include_directories(main
PRIVATE
    ${CMAKE_SOURCE_DIR}/Externals/libadrenotools/include
    ${CMAKE_SOURCE_DIR}/Externals/VulkanMemoryAllocator/include
)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/)
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/Sys/)
file(COPY ${CMAKE_SOURCE_DIR}/Data/Sys DESTINATION ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/)

# Delete folders that aren't used by the Android version of Dolphin
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/Sys/Resources/)
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/Sys/Themes/)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} main)

add_subdirectory(AndroidCommon)
