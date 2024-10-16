cmake_minimum_required(VERSION 3.13)

include("${PROJECT_SOURCE_DIR}/CMake/GetScmRev.cmake")

function(configure_source_file path)
  configure_file(
    "${PROJECT_SOURCE_DIR}/${path}.in"
    "${PROJECT_BINARY_DIR}/${path}.tmp"
  )

  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${PROJECT_BINARY_DIR}/${path}.tmp" "${PROJECT_BINARY_DIR}/${path}")

  file(REMOVE "${PROJECT_BINARY_DIR}/${path}.tmp")
endfunction()

configure_source_file("Source/Core/Common/scmrev.h")

if(APPLE)
  configure_source_file("Source/Core/DolphinQt/Info.plist")
  configure_source_file("Source/Core/MacUpdater/Info.plist")
endif()
