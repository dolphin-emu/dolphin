cmake_minimum_required(VERSION 3.11)
project(WIL)

# Set by build server to speed up build/reduce file/object size
option(FAST_BUILD "Sets options to speed up build/reduce obj/executable size" OFF)

if (NOT DEFINED WIL_BUILD_VERSION)
    set(WIL_BUILD_VERSION "0.0.0")
endif()

# Detect the Windows SDK version. If we're using the Visual Studio generator, this will be provided for us. Otherwise
# we'll need to assume that this value comes from the command line (e.g. through the VS command prompt)
if (DEFINED CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION)
    set(WIL_WINDOWS_SDK_VERSION ${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION})
else()
    # This has a trailing backslash for whatever reason...
    string(REGEX REPLACE "\\\\$" "" WIL_WINDOWS_SDK_VERSION "$ENV{WindowsSDKVersion}")
endif()

add_subdirectory(packaging)
add_subdirectory(tests)
