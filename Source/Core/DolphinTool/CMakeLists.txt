add_executable(dolphin-tool
  ToolHeadlessPlatform.cpp
  ExtractCommand.cpp
  ExtractCommand.h
  ConvertCommand.cpp
  ConvertCommand.h
  VerifyCommand.cpp
  VerifyCommand.h
  HeaderCommand.cpp
  HeaderCommand.h
  ToolMain.cpp
)

set_target_properties(dolphin-tool PROPERTIES OUTPUT_NAME dolphin-tool)

target_link_libraries(dolphin-tool
PRIVATE
  discio
  uicommon
  cpp-optparse
  fmt::fmt
)

if(MSVC)
  # Add precompiled header
  target_link_libraries(dolphin-tool PRIVATE use_pch)
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-tool)
install(TARGETS dolphin-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
