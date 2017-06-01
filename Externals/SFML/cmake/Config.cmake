# detect the OS
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(SFML_OS_WINDOWS 1)

    # don't use the OpenGL ES implementation on Windows
    set(OPENGL_ES 0)

    # detect the architecture (note: this test won't work for cross-compilation)
    include(CheckTypeSize)
    check_type_size(void* SIZEOF_VOID_PTR)
    if(${SIZEOF_VOID_PTR} STREQUAL "4")
        set(ARCH_32BITS 1)
    elseif(${SIZEOF_VOID_PTR} STREQUAL "8")
        set(ARCH_64BITS 1)
    else()
        message(FATAL_ERROR "Unsupported architecture")
        return()
    endif()
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
    set(SFML_OS_UNIX 1)
    if(ANDROID)
        set(SFML_OS_ANDROID 1)
        # use the OpenGL ES implementation on Android
        set(OPENGL_ES 1)
    else()
        set(SFML_OS_LINUX 1)
        # don't use the OpenGL ES implementation on Linux
        set(OPENGL_ES 0)
    endif()
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
    set(SFML_OS_FREEBSD 1)
    # don't use the OpenGL ES implementation on FreeBSD
    set(OPENGL_ES 0)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
    if(IOS)
        set(SFML_OS_IOS 1)

        # set the target framework and platforms
        set(CMAKE_OSX_SYSROOT "iphoneos")
        set(CMAKE_OSX_ARCHITECTURES "armv6;armv7;i386")
        set(CMAKE_XCODE_EFFECTIVE_PLATFORMS "-iphoneos;-iphonesimulator")

        # help the compiler detection script below
        set(CMAKE_COMPILER_IS_GNUCXX 1)

        # use the OpenGL ES implementation on iOS
        set(OPENGL_ES 1)
    else()
        set(SFML_OS_MACOSX 1)

        # don't use the OpenGL ES implementation on Mac OS X
        set(OPENGL_ES 0)

        # detect OS X version. (use '/usr/bin/sw_vers -productVersion' to extract V from '10.V.x'.)
        EXEC_PROGRAM(/usr/bin/sw_vers ARGS -productVersion OUTPUT_VARIABLE MACOSX_VERSION_RAW)
        STRING(REGEX REPLACE "10\\.([0-9]+).*" "\\1" MACOSX_VERSION "${MACOSX_VERSION_RAW}")
        if(${MACOSX_VERSION} LESS 7)
            message(FATAL_ERROR "Unsupported version of OS X: ${MACOSX_VERSION_RAW}")
            return()
        endif()
    endif()
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    set(SFML_OS_ANDROID 1)

    # use the OpenGL ES implementation on Android
    set(OPENGL_ES 1)
else()
    message(FATAL_ERROR "Unsupported operating system or environment")
    return()
endif()

# detect the compiler and its version
# Note: on some platforms (OS X), CMAKE_COMPILER_IS_GNUCXX is true
# even when CLANG is used, therefore the Clang test is done first
if(CMAKE_CXX_COMPILER MATCHES ".*clang[+][+]" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
   # CMAKE_CXX_COMPILER_ID is an internal CMake variable subject to change,
   # but there is no other way to detect CLang at the moment
   set(SFML_COMPILER_CLANG 1)
   execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "--version" OUTPUT_VARIABLE CLANG_VERSION_OUTPUT)
   string(REGEX REPLACE ".*clang version ([0-9]+\\.[0-9]+).*" "\\1" SFML_CLANG_VERSION "${CLANG_VERSION_OUTPUT}")
elseif(CMAKE_COMPILER_IS_GNUCXX)
    set(SFML_COMPILER_GCC 1)
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "-dumpversion" OUTPUT_VARIABLE GCC_VERSION_OUTPUT)
    string(REGEX REPLACE "([0-9]+\\.[0-9]+).*" "\\1" SFML_GCC_VERSION "${GCC_VERSION_OUTPUT}")
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "--version" OUTPUT_VARIABLE GCC_COMPILER_VERSION)
    string(REGEX MATCHALL ".*(tdm[64]*-[1-9]).*" SFML_COMPILER_GCC_TDM "${GCC_COMPILER_VERSION}")
    execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "-dumpmachine" OUTPUT_VARIABLE GCC_MACHINE)
    string(STRIP "${GCC_MACHINE}" GCC_MACHINE)
    if(GCC_MACHINE MATCHES ".*w64.*")
        set(SFML_COMPILER_GCC_W64 1)
    endif()
elseif(MSVC)
    set(SFML_COMPILER_MSVC 1)
    if(MSVC_VERSION EQUAL 1400)
        set(SFML_MSVC_VERSION 8)
    elseif(MSVC_VERSION EQUAL 1500)
        set(SFML_MSVC_VERSION 9)
    elseif(MSVC_VERSION EQUAL 1600)
        set(SFML_MSVC_VERSION 10)
    elseif(MSVC_VERSION EQUAL 1700)
        set(SFML_MSVC_VERSION 11)
    elseif(MSVC_VERSION EQUAL 1800)
        set(SFML_MSVC_VERSION 12)
    elseif(MSVC_VERSION EQUAL 1900)
        set(SFML_MSVC_VERSION 14)
    endif()
else()
    message(FATAL_ERROR "Unsupported compiler")
    return()
endif()

# define the install directory for miscellaneous files
if(SFML_OS_WINDOWS OR SFML_OS_IOS)
    set(INSTALL_MISC_DIR .)
elseif(SFML_OS_LINUX OR SFML_OS_FREEBSD OR SFML_OS_MACOSX)
    set(INSTALL_MISC_DIR share/SFML)
elseif(SFML_OS_ANDROID)
    set(INSTALL_MISC_DIR ${ANDROID_NDK}/sources/sfml)
endif()
