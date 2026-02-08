cmake_minimum_required(VERSION 3.13...4.2.0)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_SHARED OFF)

add_subdirectory(zstd EXCLUDE_FROM_ALL)

dolphin_disable_warnings(libzstd_static)
add_library(zstd::zstd ALIAS libzstd_static)
