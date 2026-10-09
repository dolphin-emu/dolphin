dolphin_fetchcontent_declare(spirv_cross
  GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Cross.git
  GIT_TAG ebe2aa0cd80f5eb5cd8a605da604cacf72205f3b # vulkan-sdk-1.4.363.0
  GIT_SHALLOW TRUE
)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(SPIRV_CROSS_CLI OFF)
set(SPIRV_CROSS_ENABLE_TESTS OFF)
set(SPIRV_CROSS_SKIP_INSTALL ON)
set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON)

FetchContent_MakeAvailable(spirv_cross)

add_library(spirv_cross INTERFACE)
target_link_libraries(spirv_cross INTERFACE
  spirv-cross-c
  spirv-cross-glsl
  spirv-cross-hlsl
  spirv-cross-msl
  spirv-cross-cpp
  spirv-cross-reflect
  spirv-cross-util
  spirv-cross-core
)
target_compile_definitions(spirv_cross INTERFACE SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
target_include_directories(spirv_cross INTERFACE "${spirv_cross_SOURCE_DIR}/include")
