
include(${PROJECT_SOURCE_DIR}/cmake/common_build_flags.cmake)

# All projects need to reference the WIL headers
include_directories(${PROJECT_SOURCE_DIR}/include)

# TODO: Might be worth trying to conditionally do this on SDK version, assuming there's a semi-easy way to detect that
include_directories(BEFORE SYSTEM ./workarounds/wrl)

# Because we don't always use msbuild, we need to run nuget manually
find_program(NUGET nuget)
if (NOT NUGET)
    message(FATAL_ERROR "Unable to find the nuget CLI tool. Please install it from https://www.nuget.org/downloads and ensure it has been added to the PATH")
endif()

execute_process(COMMAND
    ${NUGET} install Microsoft.Windows.CppWinRT -Version ${CPPWINRT_VERSION} -OutputDirectory packages
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE ret)
if (NOT ret EQUAL 0)
    message(FATAL_ERROR "Failed to install nuget package Microsoft.Windows.CppWinRT.${CPPWINRT_VERSION}")
endif()

set(CPPWINRT ${CMAKE_BINARY_DIR}/packages/Microsoft.Windows.CppWinRT.${CPPWINRT_VERSION}/bin/cppwinrt.exe)
execute_process(COMMAND
    ${CPPWINRT} -input sdk -output include
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    RESULT_VARIABLE ret)
if (NOT ret EQUAL 0)
    message(FATAL_ERROR "Failed to run cppwinrt.exe")
endif()

include_directories(BEFORE SYSTEM ${CMAKE_BINARY_DIR}/include)

# The build pipelines have limitations that local development environments do not, so turn a few knobs
if (${FAST_BUILD})
    replace_cxx_flag("/GR" "/GR-") # Disables RTTI
    add_definitions(-DCATCH_CONFIG_FAST_COMPILE -DWIL_FAST_BUILD)
endif()

# For some unknown reason, 'RelWithDebInfo' compiles with '/Ob1' as opposed to '/Ob2' which prevents inlining of
# functions not marked 'inline'. The reason we prefer 'RelWithDebInfo' over 'Release' is to get debug info, so manually
# revert to the desired (and default) inlining behavior as that exercises more interesting code paths
if (${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
    # TODO: This is currently blocked by an apparent Clang bug: https://github.com/llvm/llvm-project/issues/59690
    # replace_cxx_flag("/Ob1" "/Ob2")
endif()

set(COMMON_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/CommonTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ComTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FileSystemTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/NTResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ResourceTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ResultTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Rpc.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SafeCastTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/TraceLoggingTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/WistdTests.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wiTest.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../natvis/wil.natvis
    )

add_subdirectory(app)
add_subdirectory(cpplatest)
add_subdirectory(noexcept)
add_subdirectory(normal)
add_subdirectory(win7)

set(DEBUG_BUILD FALSE)
set(HAS_DEBUG_INFO FALSE)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(DEBUG_BUILD TRUE)
    set(HAS_DEBUG_INFO TRUE)
elseif(${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
    set(HAS_DEBUG_INFO TRUE)
endif()

set(ASAN_AVAILABLE FALSE)
set(UBSAN_AVAILABLE FALSE)
if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # Address Sanitizer is available for all architectures and build types, but warns/errors if debug info is not enabled
    set(ASAN_AVAILABLE ${HAS_DEBUG_INFO})
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Address Sanitizer is not available with debug libraries
    set(ASAN_AVAILABLE NOT ${DEBUG_BUILD})
    set(UBSAN_AVAILABLE NOT ${DEBUG_BUILD})
endif()

if (${ASAN_AVAILABLE})
    add_subdirectory(sanitize-address)
endif()

if (${UBSAN_AVAILABLE})
    add_subdirectory(sanitize-undefined-behavior)
endif()
