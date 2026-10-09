dolphin_fetchcontent_declare(
  lz4
  GIT_REPOSITORY https://github.com/lz4/lz4.git
  GIT_TAG ebb370ca83af193212df4dcbadcc5d87bc0de2f0 # v1.10.0
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake
)

# We only want the static library, nothing else.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(LZ4_BUILD_CLI OFF)
set(LZ4_BUILD_LEGACY_LZ4C OFF)
set(LZ4_BUNDLED_MODE ON)

FetchContent_MakeAvailable(lz4)

add_library(LZ4::LZ4 ALIAS lz4_static)
target_compile_definitions(lz4_static PRIVATE XXH_NAMESPACE=LZ4_)
