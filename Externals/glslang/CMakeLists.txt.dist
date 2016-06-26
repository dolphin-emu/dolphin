cmake_minimum_required(VERSION 2.8.12)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

enable_testing()

set(CMAKE_INSTALL_PREFIX "install" CACHE STRING "prefix")

project(glslang)

if(WIN32)
    set(CMAKE_DEBUG_POSTFIX "d")
    include(ChooseMSVCCRT.cmake)
    add_definitions(-DGLSLANG_OSINCLUDE_WIN32)
elseif(UNIX)
    add_definitions(-fPIC)
    add_definitions(-DGLSLANG_OSINCLUDE_UNIX)
else(WIN32)
    message("unknown platform")
endif(WIN32)

if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=c++11)
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
    add_definitions(-std=c++11)
endif()

function(glslang_set_link_args TARGET)
    # For MinGW compiles, statically link against the GCC and C++ runtimes.
    # This avoids the need to ship those runtimes as DLLs.
    if(WIN32)
	if(${CMAKE_CXX_COMPILER_ID} MATCHES "GNU")
	    set_target_properties(${TARGET} PROPERTIES
		    LINK_FLAGS "-static -static-libgcc -static-libstdc++")
        endif()
    endif(WIN32)
endfunction(glslang_set_link_args)

# We depend on these for later projects, so they should come first.
add_subdirectory(External)

add_subdirectory(glslang)
add_subdirectory(OGLCompilersDLL)
add_subdirectory(StandAlone)
add_subdirectory(SPIRV)
add_subdirectory(hlsl)
add_subdirectory(gtests)
