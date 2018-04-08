enable_testing()
add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND})

string(APPEND CMAKE_RUNTIME_OUTPUT_DIRECTORY "/Tests")

# Since this is a Core dependency, it can't be linked as a normal library.
# Otherwise CMake inserts the library after core, but before other core
# dependencies like videocommon which also use Host_ functions, which makes the
# GNU linker complain.
add_library(unittests_stubhost OBJECT StubHost.cpp)

macro(add_dolphin_test target)
  add_executable(${target} EXCLUDE_FROM_ALL
    ${ARGN}
    $<TARGET_OBJECTS:unittests_stubhost>
  )
  set_target_properties(${target} PROPERTIES FOLDER Tests)
  target_link_libraries(${target} PRIVATE core uicommon gtest_main)
  add_dependencies(unittests ${target})
  add_test(NAME ${target} COMMAND ${target})
endmacro()

add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(VideoCommon)
