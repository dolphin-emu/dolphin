dolphin_fetchcontent_declare(
  libspng
  GIT_REPOSITORY https://github.com/randy408/libspng.git
  GIT_TAG fb768002d4288590083a476af628e51c3f1d47cd # v0.7.4
  GIT_SHALLOW TRUE
)

option(SPNG_SHARED "Build shared lib" OFF)
option(SPNG_STATIC "Build static lib" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

set(_PREV_CMAKE_SKIP_INSTALL_RULES "${CMAKE_SKIP_INSTALL_RULES}")
set(CMAKE_SKIP_INSTALL_RULES TRUE)

FetchContent_MakeAvailable(libspng)

set(CMAKE_SKIP_INSTALL_RULES "${_PREV_CMAKE_SKIP_INSTALL_RULES}")

add_library(spng::spng ALIAS spng_static)
