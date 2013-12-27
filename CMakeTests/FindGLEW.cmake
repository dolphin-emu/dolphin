include(FindPkgConfig OPTIONAL)

# This is a hack to deal with Ubuntu's mess.
# Ubuntu's version of glew is 1.8, but they have patched in most of glew 1.9.
# So Ubuntu's version works for dolphin.
macro(test_glew)
	set(CMAKE_REQUIRED_INCLUDES ${GLEW_INCLUDE_DIRS})
	set(CMAKE_REQUIRED_LIBRARIES GLEW)
	check_cxx_source_runs("
	#include <GL/glew.h>
	int main()
	{
	#ifdef GLEW_ARB_buffer_storage
	return 0;
	#else
	return 1;
	#endif
	}"
	GLEW_HAS_1_10_METHODS)

endmacro()

if(PKG_CONFIG_FOUND AND NOT ${var}_FOUND)
	pkg_search_module(GLEW glew>=1.8)
endif()

if(GLEW_FOUND)
	test_glew()
	if (GLEW_HAS_1_10_METHODS)
		include_directories(${GLEW_INCLUDE_DIRS})
		message("GLEW found")
	endif()
endif()
