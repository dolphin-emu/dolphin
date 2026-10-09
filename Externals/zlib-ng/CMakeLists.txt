dolphin_fetchcontent_declare(
  zlib-ng
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/zlib-ng/zlib-ng.git
  GIT_TAG ce01b1e41da298334f8214389cc9369540a7560f # 2.3.3
)

set(ZLIB_ENABLE_TESTS OFF)
set(ZLIB_COMPAT ON)
set(SKIP_INSTALL_ALL ON)

option(BUILD_SHARED_LIBS "Build shared library" OFF)

FetchContent_MakeAvailable(zlib-ng)

# Set ZLIB variables for find_package used by other projects
set(ZLIB_INCLUDE_DIR ${CMAKE_BINARY_DIR}/zlib-ng CACHE STRING "Path to zlib include directory")
set(ZLIB_LIBRARY ZLIB::ZLIB CACHE STRING "Path to zlib library")

# Setup zlib alias project so FindZLIB doesn't recreate it
add_library(ZLIB::ZLIB ALIAS zlib)
