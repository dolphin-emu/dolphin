cmake_minimum_required(VERSION 3.13)

# We only want the static library, nothing else.
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(LZ4_BUILD_CLI OFF)
set(LZ4_BUILD_LEGACY_LZ4C OFF)
set(LZ4_BUNDLED_MODE ON)

add_subdirectory(lz4/build/cmake)

dolphin_disable_warnings_msvc(lz4_static)
add_library(LZ4::LZ4 ALIAS lz4_static)
target_compile_definitions(lz4_static PRIVATE XXH_NAMESPACE=LZ4_)
