set(ZLIB_ENABLE_TESTS OFF)
set(ZLIB_COMPAT ON)
set(SKIP_INSTALL_ALL ON)

option(BUILD_SHARED_LIBS "Build shared library" OFF)

add_subdirectory(zlib-ng)

# Set ZLIB variables for find_package used by other projects
set(ZLIB_INCLUDE_DIR ${CMAKE_BINARY_DIR}/zlib-ng CACHE STRING "Path to zlib include directory")
set(ZLIB_LIBRARY ZLIB::ZLIB CACHE STRING "Path to zlib library")

# Setup zlib alias project so FindZLIB doesn't recreate it
add_library(ZLIB::ZLIB ALIAS zlib)
dolphin_disable_warnings(zlib)
