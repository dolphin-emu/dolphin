add_executable(dolphin-nogui
  Platform.cpp
  Platform.h
  PlatformHeadless.cpp
  MainNoGUI.cpp
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

if(ENABLE_WAYLAND AND WAYLAND_FOUND)
  set(WAYLAND_PLATFORM_SRCS PlatformWayland.cpp)

  # Generate the xdg-shell and xdg-decoration protocols at build-time.
  ecm_add_wayland_client_protocol(WAYLAND_PLATFORM_SRCS
    PROTOCOL "${WAYLAND_PROTOCOLS_PKGDATADIR}/stable/xdg-shell/xdg-shell.xml"
    BASENAME xdg-shell)
  ecm_add_wayland_client_protocol(WAYLAND_PLATFORM_SRCS
    PROTOCOL "${WAYLAND_PROTOCOLS_PKGDATADIR}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml"
    BASENAME xdg-decoration)

  target_include_directories(dolphin-nogui PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
  target_sources(dolphin-nogui PRIVATE "${WAYLAND_PLATFORM_SRCS}")
  target_link_libraries(dolphin-nogui PRIVATE Wayland::Client)
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

