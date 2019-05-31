add_executable(dolphin-nogui
  Platform.cpp
  Platform.h
  PlatformHeadless.cpp
  MainNoGUI.cpp
)

if(ENABLE_X11 AND X11_FOUND)
  target_sources(dolphin-nogui PRIVATE PlatformX11.cpp)
endif()

set_target_properties(dolphin-nogui PROPERTIES OUTPUT_NAME dolphin-emu-nogui)

target_link_libraries(dolphin-nogui
PRIVATE
  core
  uicommon
  cpp-optparse
)

if(USE_DISCORD_PRESENCE)
  target_compile_definitions(dolphin-nogui PRIVATE -DUSE_DISCORD_PRESENCE)
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-nogui)
install(TARGETS dolphin-nogui RUNTIME DESTINATION ${bindir})

