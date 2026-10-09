FetchContent_Declare(
  gtest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG 063de7e9578f82b369302001269680b4b1553359 # v1.18.0
  GIT_SHALLOW TRUE
)

set(BUILD_GMOCK OFF)
set(INSTALL_GTEST OFF)
set(gtest_force_shared_crt ON)

FetchContent_MakeAvailable(gtest)

if (NOT TARGET gtest::gtest)
  add_library(gtest::gtest ALIAS gtest)
endif()
