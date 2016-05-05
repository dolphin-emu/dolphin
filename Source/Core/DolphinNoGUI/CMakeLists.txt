if(NOT(USE_X11 OR ENABLE_HEADLESS))
  return()
endif()

if(APPLE)
  set(DOLPHIN_EXE_BASE Dolphin)
else()
  set(DOLPHIN_EXE_BASE dolphin-emu)
endif()

set(DOLPHIN_NOGUI_EXE ${DOLPHIN_EXE_BASE}-nogui)

set(NOGUI_SRCS MainNoGUI.cpp)

add_executable(dolphin-nogui ${NOGUI_SRCS})
set_target_properties(dolphin-nogui PROPERTIES OUTPUT_NAME ${DOLPHIN_NOGUI_EXE})

target_link_libraries(dolphin-nogui PRIVATE
  core
  uicommon
  cpp-optparse
  ${LIBS}
)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-nogui)
install(TARGETS dolphin-nogui RUNTIME DESTINATION ${bindir})

