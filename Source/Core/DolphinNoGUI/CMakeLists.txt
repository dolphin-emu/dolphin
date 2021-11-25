add_executable(dolphin-nogui
  Platform.cpp
  Platform.h
  PlatformHeadless.cpp
  MainNoGUI.cpp
)

add_executable(dolphin-discimagetool
  Platform.cpp
  Platform.h
  PlatformHeadless.cpp
  MainDiscImageTool.cpp
)

if(ENABLE_X11 AND X11_FOUND)
  target_sources(dolphin-nogui PRIVATE PlatformX11.cpp)
endif()

if(WIN32)
  target_sources(dolphin-nogui PRIVATE PlatformWin32.cpp)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  target_sources(dolphin-nogui PRIVATE PlatformFBDev.cpp)
endif()

set_target_properties(dolphin-nogui PROPERTIES OUTPUT_NAME dolphin-emu-nogui)
set_target_properties(dolphin-discimagetool PROPERTIES OUTPUT_NAME dolphin-emu-discimagetool)

target_link_libraries(dolphin-nogui
PRIVATE
  core
  uicommon
  cpp-optparse
)

target_link_libraries(dolphin-discimagetool
PRIVATE
  core
  cpp-optparse
)

if(USE_DISCORD_PRESENCE)
  target_compile_definitions(dolphin-nogui PRIVATE -DUSE_DISCORD_PRESENCE)
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-nogui)
set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-discimagetool)
install(TARGETS dolphin-nogui RUNTIME DESTINATION ${bindir})
install(TARGETS dolphin-discimagetool RUNTIME DESTINATION ${bindir})

