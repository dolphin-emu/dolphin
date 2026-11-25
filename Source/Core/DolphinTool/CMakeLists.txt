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

if (WIN32)
  target_sources(dolphin-tool PRIVATE DolphinTool.exe.manifest DolphinTool.rc)

  set_target_properties(dolphin-tool PROPERTIES
    DEBUG_POSTFIX    D
    OUTPUT_NAME      DolphinTool
  )
endif()

target_link_libraries(dolphin-tool
PRIVATE
  discio
  uicommon
  cpp-optparse
  fmt::fmt
)

if(UNIX AND NOT APPLE AND NOT ANDROID AND ENABLE_QT)
  # UICommon links Qt which can't use the shared pch
  dolphin_pch(dolphin-tool)
else()
  dolphin_shared_pch(dolphin-tool)
endif()

if(APPLE)
  if(MACOS_CODE_SIGNING)
    add_custom_command(TARGET dolphin-tool
      POST_BUILD
      COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
      "${MACOS_CODE_SIGNING_IDENTITY}"
      "$<TARGET_FILE:dolphin-tool>")
  endif()
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-tool)
if (NOT WIN32)
  install(TARGETS dolphin-tool RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
