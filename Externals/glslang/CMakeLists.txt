set(ENABLE_PCH OFF)
set(BUILD_EXTERNAL OFF)
set(SKIP_GLSLANG_INSTALL ON)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_OPT OFF)
set(ENABLE_CTEST OFF)
set(ENABLE_EXCEPTIONS OFF)
add_subdirectory(glslang)

if (TARGET glslang)
  target_include_directories(glslang
  # Silence warnings on glslang by flagging it as a system include
  SYSTEM PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glslang>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glslang/Public>
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )
  dolphin_disable_warnings(glslang)
endif()
