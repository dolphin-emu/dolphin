
# include the SFML specific macros
include(${PROJECT_SOURCE_DIR}/cmake/Macros.cmake)

# let CMake know about our additional libraries paths
if (SFML_OS_WINDOWS)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${PROJECT_SOURCE_DIR}/extlibs/headers")
    if(SFML_COMPILER_GCC)
        if(ARCH_32BITS)
            set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-mingw/x86")
            set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/bin/x86")
        elseif(ARCH_64BITS)
            set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-mingw/x64")
            set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/bin/x64")
        endif()
    elseif(SFML_COMPILER_MSVC)
        if(SFML_MSVC_VERSION LESS 14)
            if(ARCH_32BITS)
                set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-msvc/x86")
            elseif(ARCH_64BITS)
                set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-msvc/x64")
            endif()
        else()
            if(ARCH_32BITS)
                set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-msvc-universal/x86")
            elseif(ARCH_64BITS)
                set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-msvc-universal/x64")
            endif()
        endif()
    endif()
elseif(SFML_OS_MACOSX)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${PROJECT_SOURCE_DIR}/extlibs/headers")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-osx/lib/")
elseif(SFML_OS_IOS)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${PROJECT_SOURCE_DIR}/extlibs/headers")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-ios/")
elseif(SFML_OS_ANDROID)
    set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${PROJECT_SOURCE_DIR}/extlibs/headers")
    set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${PROJECT_SOURCE_DIR}/extlibs/libs-android/${ANDROID_ABI}")
endif()

# add the SFML sources path
include_directories(${PROJECT_SOURCE_DIR}/src)

# define the path of our additional CMake modules
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake/Modules/")

# set the output directory for SFML libraries
set(LIBRARY_OUTPUT_PATH "${PROJECT_BINARY_DIR}/lib")

# add the modules subdirectories
add_subdirectory(System)
if(SFML_OS_WINDOWS OR SFML_OS_ANDROID OR SFML_OS_IOS)
    add_subdirectory(Main)
endif()
add_subdirectory(Window)
add_subdirectory(Network)
add_subdirectory(Graphics)
add_subdirectory(Audio)
