if(NOT(USE_X11 OR ENABLE_HEADLESS))
  return()
endif()

set(NOGUI_SRCS MainNoGUI.cpp)

add_executable(dolphin-nogui ${NOGUI_SRCS})
set_target_properties(dolphin-nogui PROPERTIES OUTPUT_NAME dolphin-emu-nogui)

target_link_libraries(dolphin-nogui PRIVATE
  core
  uicommon
  cpp-optparse
  ${LIBS}
)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-nogui)
install(TARGETS dolphin-nogui RUNTIME DESTINATION ${bindir})

