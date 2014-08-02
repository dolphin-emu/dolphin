macro(add_dolphin_test target srcs)
	# Since this is a Core dependency, it can't be linked as a library and has
	# to be linked as an object file. Otherwise CMake inserts the library after
	# core, but before other core dependencies like videocommon which also use
	# Host_ functions.
	set(srcs2 ${srcs} ${CMAKE_SOURCE_DIR}/Source/UnitTests/TestUtils/StubHost.cpp)
	add_executable(Tests/${target} EXCLUDE_FROM_ALL ${srcs2})
	add_custom_command(TARGET Tests/${target}
	                   PRE_LINK
	                   COMMAND mkdir -p ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tests)
	target_link_libraries(Tests/${target} core gtest)
	add_dependencies(unittests Tests/${target})
	add_test(NAME ${target} COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Tests/${target})
endmacro(add_dolphin_test)

add_subdirectory(TestUtils)

add_subdirectory(Common)
add_subdirectory(Core)
add_subdirectory(VideoCommon)
