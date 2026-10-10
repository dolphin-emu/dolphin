FetchContent_Declare(
  glslang
  SOURCE_DIR "${CMAKE_SOURCE_DIR}/.FetchContent/glslang/glslang"
  GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
  GIT_TAG a57276bf558f5cf94d3a9854ebdf5a2236849a5a # 16.0.0
  GIT_SHALLOW TRUE
)

set(ENABLE_PCH OFF)
set(BUILD_EXTERNAL OFF)
set(SKIP_GLSLANG_INSTALL ON)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_OPT OFF)
set(ENABLE_CTEST OFF)
set(ENABLE_EXCEPTIONS OFF)

FetchContent_MakeAvailable(glslang)

if (TARGET glslang)
  target_include_directories(glslang
  # Silence warnings on glslang by flagging it as a system include
  SYSTEM PUBLIC
    $<BUILD_INTERFACE:${glslang_SOURCE_DIR}>
    $<BUILD_INTERFACE:${glslang_SOURCE_DIR}/Public>
  PUBLIC
    "${glslang_SOURCE_DIR}/../"
  )
endif()
