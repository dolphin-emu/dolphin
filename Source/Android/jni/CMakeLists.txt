add_library(main SHARED
  ButtonManager.cpp
  MainAndroid.cpp
)

target_link_libraries(main
PRIVATE
  core
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

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/)
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/Sys/)
file(COPY ${CMAKE_SOURCE_DIR}/Data/Sys DESTINATION ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/)
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/Sys/Resources/)  # not used on Android

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} main)
