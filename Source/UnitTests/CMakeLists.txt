enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND} "--output-on-failure")

string(APPEND CMAKE_RUNTIME_OUTPUT_DIRECTORY "/Tests")

add_executable(tests EXCLUDE_FROM_ALL UnitTestsMain.cpp StubHost.cpp)
set_target_properties(tests PROPERTIES FOLDER Tests)
target_link_libraries(tests PRIVATE fmt::fmt gtest::gtest core uicommon)
add_test(NAME tests COMMAND tests)
add_dependencies(unittests tests)

macro(add_dolphin_test target)
  add_library(${target} OBJECT ${ARGN})
  target_link_libraries(${target} PUBLIC fmt::fmt gtest::gtest PRIVATE core uicommon)
  target_link_libraries(tests PRIVATE ${target})
endmacro()

add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(VideoCommon)
