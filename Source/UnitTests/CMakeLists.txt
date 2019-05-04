enable_testing()
add_custom_target(unittests)

if (WIN32)
  add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND} -C ${CMAKE_BUILD_TYPE})
else()
  add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND})
endif()

string(APPEND CMAKE_RUNTIME_OUTPUT_DIRECTORY "/Tests")

if (MSVC)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release")
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug")
endif()

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

set_target_properties(unittests PROPERTIES
  FOLDER "Tests"
  PROJECT_LABEL "UnitTests"
)

set_target_properties(unittests_stubhost PROPERTIES
  FOLDER "Tests"
  PROJECT_LABEL "UnitTests_StubHost"
)

add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(VideoCommon)
