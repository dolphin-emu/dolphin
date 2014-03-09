macro(add_dolphin_test target srcs libs)
	add_executable(Tests/${target} EXCLUDE_FROM_ALL ${srcs})
	add_custom_command(TARGET Tests/${target}
	                   PRE_LINK
	                   COMMAND mkdir -p ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tests)
	target_link_libraries(Tests/${target} ${libs} gtest)
	add_dependencies(unittests Tests/${target})
	add_test(NAME ${target} COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tests/${target})
endmacro(add_dolphin_test)

add_subdirectory(Common)
add_subdirectory(Core)
