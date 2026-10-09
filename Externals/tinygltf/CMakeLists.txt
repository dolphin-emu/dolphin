dolphin_fetchcontent_declare(tinygltf
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG ffa2c665f9dc26d7cefc7dec059dade502439984 # v2.8.20
  GIT_SHALLOW TRUE
)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(TINYGLTF_BUILD_LOADER_EXAMPLE OFF)
set(TINYGLTF_INSTALL OFF)
set(TINYGLTF_HEADER_ONLY OFF)
set(BUILD_SHARED_LIBS OFF)

FetchContent_MakeAvailable(tinygltf)

target_compile_definitions(tinygltf PUBLIC
  TINYGLTF_NOEXCEPTION
  TINYGLTF_NO_EXTERNAL_IMAGE
  TINYGLTF_USE_CPP14
)
if(NOT MSVC)
  target_compile_features(tinygltf PRIVATE cxx_std_20)
endif()
