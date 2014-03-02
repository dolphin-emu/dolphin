add_custom_target(unittests)
add_custom_command(TARGET unittests POST_BUILD COMMAND ${CMAKE_CTEST_COMMAND})

macro(add_dolphin_test target srcs libs)
	add_executable(Tests/${target} EXCLUDE_FROM_ALL ${srcs})
	target_link_libraries(Tests/${target} ${libs} ${GTEST_BOTH_LIBRARIES})
	add_dependencies(unittests Tests/${target})
	add_test(NAME ${target} COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tests/${target})
endmacro(add_dolphin_test)

add_subdirectory(Core)
