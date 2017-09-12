set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR powerpc-eabi)

set(DEVKITPRO $ENV{DEVKITPRO})
set(DEVKITPPC $ENV{DEVKITPPC})

if(NOT DEVKITPPC)
    message(FATAL_ERROR "Please set DEVKITPPC in your environment")
endif()

if(NOT DEVKITPRO)
    message(FATAL_ERROR "Please set DEVKITPRO in your environment")
endif()

if(NOT LIBOGCDIR)
    message(STATUS "LIBOGCDIR not set, using default")
    set(LIBOGCDIR ${DEVKITPRO}/libogc)
endif()

if(WIN32)
	set(CMAKE_C_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-gcc.exe)
	set(CMAKE_CXX_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-g++.exe)
else()
	set(CMAKE_C_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-gcc)
	set(CMAKE_CXX_COMPILER ${DEVKITPPC}/bin/powerpc-eabi-g++)
endif()

set(CMAKE_FIND_ROOT_PATH ${DEVKITPPC})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

