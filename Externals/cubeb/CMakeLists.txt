dolphin_fetchcontent_declare(
  cubeb
  GIT_REPOSITORY https://github.com/mozilla/cubeb.git
  GIT_TAG 54217bca3f3e0cd53c073690a23dd25d83557909
)

set(USE_AAUDIO OFF CACHE INTERNAL "Force-disable AAudio (Requires Android >= 8.0)" FORCE)
set(CMAKE_DISABLE_FIND_PACKAGE_Doxygen ON)
set(BUILD_TESTS OFF)
set(BUILD_TOOLS OFF)

FetchContent_MakeAvailable(cubeb)

add_library(cubeb::cubeb ALIAS cubeb)
