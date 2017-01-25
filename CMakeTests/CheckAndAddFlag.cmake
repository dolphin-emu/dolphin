include(CheckCCompilerFlag)
include(CheckCXXCompilerFlag)

macro(check_and_add_flag var flag)
	check_c_compiler_flag(${flag} FLAG_C_${var})
	if(FLAG_C_${var})
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${flag}")
	endif()

	check_cxx_compiler_flag(${flag} FLAG_CXX_${var})
	if(FLAG_CXX_${var})
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
	endif()
endmacro()
