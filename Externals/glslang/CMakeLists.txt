set(ENABLE_PCH OFF)
set(BUILD_EXTERNAL OFF)
set(SKIP_GLSLANG_INSTALL ON)
set(ENABLE_GLSLANG_BINARIES OFF)
set(ENABLE_HLSL OFF)
set(ENABLE_OPT OFF)
set(ENABLE_CTEST OFF)
set(ENABLE_EXCEPTIONS OFF)
add_subdirectory(glslang)

if(TARGET glslang)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(glslang INTERFACE "--system-header-prefix=glslang/")
  else()
    # Silence warnings on glslang by flagging it as a system include
    set_target_properties(glslang PROPERTIES SYSTEM TRUE)
  endif()
  target_include_directories(glslang
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/glslang/Public>
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
  )
endif()
