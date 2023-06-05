add_executable(dolphin-tool
  ToolHeadlessPlatform.cpp
  ConvertCommand.cpp
  VerifyCommand.cpp
  HeaderCommand.cpp
  ToolMain.cpp
)

if (MSVC)
  # The only way, as it stands, to use .cpp files as module interface units in MSVC
  # FIXME when MSVC gets smarter
  target_compile_options(dolphin-tool /interface /TP)
endif()

set_target_properties(dolphin-tool PROPERTIES OUTPUT_NAME dolphin-tool)

target_link_libraries(dolphin-tool
PRIVATE
  discio
  uicommon
  cpp-optparse
)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-tool)
install(TARGETS dolphin-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
