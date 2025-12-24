set(BUILD_TESTING OFF)
set(ZLIB_COMPAT ON)
set(INSTALL_UTILS OFF)
set(BUILD_SHARED_LIBS OFF)

add_subdirectory(zlib-ng)

# Set ZLIB variables for find_package used by other projects
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/zlib-ng CACHE STRING "Path to zlib include directory")
set(ZLIB_LIBRARY ZLIB::ZLIB CACHE STRING "Path to zlib library")

add_library(ZLIB::ZLIB ALIAS zlib-ng)
dolphin_disable_warnings(zlib-ng)
