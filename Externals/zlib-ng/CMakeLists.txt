dolphin_fetchcontent_declare(
  zlib-ng
  GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng.git
  GIT_TAG 12731092979c6d07f42da27da673a9f6c7b13586 # 2.3.3
)

set(BUILD_TESTING OFF)
set(ZLIB_COMPAT ON)
set(SKIP_INSTALL_ALL ON)

option(BUILD_SHARED_LIBS "Build shared library" OFF)

FetchContent_MakeAvailable(zlib-ng)

# Set ZLIB variables for find_package used by other projects
set(ZLIB_INCLUDE_DIR ${zlib-ng_BINARY_DIR} CACHE STRING "Path to zlib include directory")
set(ZLIB_LIBRARY ZLIB::ZLIB CACHE STRING "Path to zlib library")

# Setup zlib alias project so FindZLIB doesn't recreate it
add_library(ZLIB::ZLIB ALIAS zlib-ng)
