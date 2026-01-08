find_package(Git REQUIRED)
dolphin_fetchcontent_declare(
  cubeb
  GIT_REPOSITORY https://github.com/mozilla/cubeb.git
  GIT_TAG dcd7cddafa4b95776cf5d117549ad58e17f23764
  PATCH_COMMAND ${GIT_EXECUTABLE} apply ${CMAKE_CURRENT_LIST_DIR}/cubeb.patch
)

set(BUILD_SHARED_LIBS OFF)
set(USE_AAUDIO OFF CACHE INTERNAL "Force-disable AAudio (Requires Android >= 8.0 and we target 7.0)" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON)
set(BUILD_TESTS OFF)
set(BUILD_TOOLS OFF)
set(USE_SANITIZERS OFF)

find_program(CARGO_BIN cargo)
if(CARGO_BIN)
	set(BUILD_RUST_LIBS ON)
endif()

FetchContent_MakeAvailable(cubeb)

add_library(cubeb::cubeb ALIAS cubeb)
