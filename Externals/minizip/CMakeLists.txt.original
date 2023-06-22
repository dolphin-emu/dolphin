#***************************************************************************
# Copyright (C) 2017-2020 Nathan Moinvaziri
#                         https://github.com/zlib-ng/minizip-ng
# Copyright (C)      2016 Matthias Schmieder
#                         schmieder.matthias@gmail.com
#***************************************************************************

cmake_minimum_required(VERSION 3.13)

message(STATUS "Using CMake version ${CMAKE_VERSION}")

# Compatibility options
option(MZ_COMPAT "Enables compatibility layer" ON)
# Compression library options
option(MZ_ZLIB "Enables ZLIB compression" ON)
option(MZ_BZIP2 "Enables BZIP2 compression" ON)
option(MZ_LZMA "Enables LZMA & XZ compression" ON)
option(MZ_ZSTD "Enables ZSTD compression" ON)
option(MZ_LIBCOMP "Enables Apple compression" ${APPLE})
option(MZ_FETCH_LIBS "Enables fetching third-party libraries if not found" ${WIN32})
option(MZ_FORCE_FETCH_LIBS "Enables fetching third-party libraries always" OFF)
# Encryption support options
option(MZ_PKCRYPT "Enables PKWARE traditional encryption" ON)
option(MZ_WZAES "Enables WinZIP AES encryption" ON)
option(MZ_OPENSSL "Enables OpenSSL for encryption" ${UNIX})
option(MZ_LIBBSD "Enable libbsd crypto random" ${UNIX})
option(MZ_SIGNING "Enables zip signing support" ON)
# Character conversion options
option(MZ_ICONV "Enables iconv for string encoding conversion" ON)
# Code generation options
option(MZ_COMPRESS_ONLY "Only support compression" OFF)
option(MZ_DECOMPRESS_ONLY "Only support decompression" OFF)
option(MZ_FILE32_API "Builds using posix 32-bit file api" OFF)
# Build and continuous integration options
option(MZ_BUILD_TESTS "Builds minizip test executable" OFF)
option(MZ_BUILD_UNIT_TESTS "Builds minizip unit test project" OFF)
option(MZ_BUILD_FUZZ_TESTS "Builds minizip fuzzer executables" OFF)
option(MZ_CODE_COVERAGE "Builds with code coverage flags" OFF)
# Package management options
set(MZ_PROJECT_SUFFIX "" CACHE STRING "Project name suffix for package managers")

mark_as_advanced(MZ_FILE32_API MZ_PROJECT_SUFFIX)

# Backwards compatibility
if(DEFINED MZ_BUILD_TEST)
    set(MZ_BUILD_TESTS ${MZ_BUILD_TEST})
endif()
if(DEFINED MZ_BUILD_UNIT_TEST)
    set(MZ_BUILD_UNIT_TESTS ${MZ_BUILD_UNIT_TEST})
endif()
if(DEFINED MZ_BUILD_FUZZ_TEST)
    set(MZ_BUILD_FUZZ_TESTS ${MZ_BUILD_FUZZ_TEST})
endif()

if(POLICY CMP0074)
    cmake_policy(SET CMP0074 OLD)
endif()
if(POLICY CMP0054)
    cmake_policy(SET CMP0054 NEW)
endif()

# ZLIB_ROOT - Parent directory of zlib installation
# BZIP2_ROOT - Parent directory of BZip2 installation
# OPENSSL_ROOT - Parent directory of OpenSSL installation

enable_language(C)

# Library version
set(VERSION "3.0.4")

# API version
set(SOVERSION "3")

include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckTypeSize)
include(GNUInstallDirs)
include(FeatureSummary)

set(INSTALL_BIN_DIR ${CMAKE_INSTALL_BINDIR} CACHE PATH "Installation directory for executables")
set(INSTALL_LIB_DIR ${CMAKE_INSTALL_LIBDIR} CACHE PATH "Installation directory for libraries")
set(INSTALL_INC_DIR ${CMAKE_INSTALL_INCLUDEDIR} CACHE PATH "Installation directory for headers")
set(INSTALL_MAN_DIR ${CMAKE_INSTALL_MANDIR} CACHE PATH "Installation directory for manual pages")

set(STDLIB_DEF)
set(MINIZIP_DEF)
set(MINIZIP_INC)
set(MINIZIP_LIB)
set(MINIZIP_LBD)
set(MINIZIP_DEP)
set(MINIZIP_DEP_PKG)
set(MINIZIP_LFG)

# Initial source files
set(MINIZIP_SRC
    mz_crypt.c
    mz_os.c
    mz_strm.c
    mz_strm_buf.c
    mz_strm_mem.c
    mz_strm_split.c
    mz_zip.c
    mz_zip_rw.c)

# Initial header files
set(MINIZIP_HDR
    mz.h
    mz_os.h
    mz_crypt.h
    mz_strm.h
    mz_strm_buf.h
    mz_strm_mem.h
    mz_strm_split.h
    mz_strm_os.h
    mz_zip.h
    mz_zip_rw.h)

set(PC_PRIVATE_LIBS)

# Check for system includes
check_include_file(stdint.h   HAVE_STDINT_H)
check_include_file(inttypes.h HAVE_INTTYPES_H)

if(HAVE_STDINT_H)
    list(APPEND STDLIB_DEF -DHAVE_STDINT_H)
endif()
if(HAVE_INTTYPES_H)
    list(APPEND STDLIB_DEF -DHAVE_INTTYPES_H)
endif()

# Check for large file support
check_type_size(off64_t OFF64_T)
if(HAVE_OFF64_T)
    list(APPEND STDLIB_DEF -D__USE_LARGEFILE64)
    list(APPEND STDLIB_DEF -D_LARGEFILE64_SOURCE)
endif()
# Check for fseeko support
check_function_exists(fseeko HAVE_FSEEKO)
if(NOT HAVE_FSEEKO)
    list(APPEND STDLIB_DEF -DNO_FSEEKO)
endif()

# Checkout remote repository
macro(clone_repo name url)
    if(NOT ${name}_REPOSITORY)
        set(${name}_REPOSITORY ${url})
    endif()
    if(NOT ${name}_TAG)
        set(${name}_TAG master)
    endif()

    message(STATUS "Fetching ${name} ${${name}_REPOSITORY} ${${name}_TAG}")

    # Check for FetchContent cmake support
    if(${CMAKE_VERSION} VERSION_LESS "3.11")
        message(FATAL_ERROR "CMake 3.11 required to fetch ${name}")
    else()
        include(FetchContent)

        string(TOLOWER ${name} name_lower)
        string(TOUPPER ${name} name_upper)

        FetchContent_Declare(${name}
            GIT_REPOSITORY ${${name}_REPOSITORY}
            GIT_TAG ${${name}_TAG}
            SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lib/${name_lower})

        FetchContent_GetProperties(${name} POPULATED ${name_lower}_POPULATED)

        if(NOT ${name_lower}_POPULATED)
            FetchContent_Populate(${name})
        endif()

        set(${name_upper}_SOURCE_DIR ${${name_lower}_SOURCE_DIR})
        set(${name_upper}_BINARY_DIR ${${name_lower}_BINARY_DIR})
    endif()
endmacro()

if(MZ_LIBCOMP)
    if(APPLE)
        # Use Apple libcompression
        list(APPEND MINIZIP_DEF -DHAVE_LIBCOMP)
        list(APPEND MINIZIP_SRC mz_strm_libcomp.c)
        list(APPEND MINIZIP_HDR mz_strm_libcomp.h)
        list(APPEND MINIZIP_LIB compression)

        # Disable zlib as libcompression is preferred
        set(MZ_ZLIB OFF)
    else()
        message(STATUS "LibCompression not supported on the current platform")

        set(MZ_LIBCOMP OFF)
    endif()
endif()

if(MZ_ZLIB)
    # Check if zlib is present
    if(NOT MZ_FORCE_FETCH_LIBS)
        find_package(ZLIB QUIET)
        set(ZLIB_VERSION ${ZLIB_VERSION_STRING})
    endif()

    if(ZLIB_FOUND AND NOT MZ_FORCE_FETCH_LIBS)
        message(STATUS "Using ZLIB ${ZLIB_VERSION}")

        list(APPEND MINIZIP_INC ${ZLIB_INCLUDE_DIRS})
        list(APPEND MINIZIP_LIB ${ZLIB_LIBRARIES})
        list(APPEND MINIZIP_LBD ${ZLIB_LIBRARY_DIRS})

        set(PC_PRIVATE_LIBS " -lz")
    elseif(MZ_FETCH_LIBS)
        clone_repo(zlib https://github.com/madler/zlib)

        # Don't automatically add all targets to the solution
        add_subdirectory(${ZLIB_SOURCE_DIR} ${ZLIB_BINARY_DIR} EXCLUDE_FROM_ALL)

        list(APPEND MINIZIP_INC ${ZLIB_SOURCE_DIR})
        list(APPEND MINIZIP_INC ${ZLIB_BINARY_DIR})

        # Have to add zlib to install targets
        if(NOT DEFINED BUILD_SHARED_LIBS OR NOT ${BUILD_SHARED_LIBS})
            list(APPEND MINIZIP_DEP zlibstatic)
        else()
            list(APPEND MINIZIP_DEP zlib)
        endif()
    else()
        message(STATUS "ZLIB library not found")

        set(MZ_ZLIB OFF)
    endif()

    if(MZ_ZLIB)
        list(APPEND MINIZIP_DEP_PKG ZLIB)
        list(APPEND MINIZIP_DEF -DHAVE_ZLIB)
        if(ZLIB_COMPAT)
            list(APPEND MINIZIP_DEF -DZLIB_COMPAT)
        endif()
        list(APPEND MINIZIP_SRC mz_strm_zlib.c)
        list(APPEND MINIZIP_HDR mz_strm_zlib.h)
    endif()
endif()

if(MZ_BZIP2)
    # Check if bzip2 is present
    if(NOT MZ_FORCE_FETCH_LIBS)
        find_package(BZip2 QUIET)
    endif()

    if(BZIP2_FOUND AND NOT MZ_FORCE_FETCH_LIBS)
        message(STATUS "Using BZIP2 ${BZIP2_VERSION_STRING}")

        list(APPEND MINIZIP_INC ${BZIP2_INCLUDE_DIRS})
        list(APPEND MINIZIP_LIB ${BZIP2_LIBRARIES})
        list(APPEND MINIZIP_LBD ${BZIP2_LIBRARY_DIRS})

        set(PC_PRIVATE_LIBS "${PC_PRIVATE_LIBS} -lbzip2")
    elseif(MZ_FETCH_LIBS)
        clone_repo(bzip2 https://sourceware.org/git/bzip2.git)

        # BZip2 repository does not support cmake so we have to create
        # the bzip2 library ourselves
        set(BZIP2_SRC
            ${BZIP2_SOURCE_DIR}/blocksort.c
            ${BZIP2_SOURCE_DIR}/bzlib.c
            ${BZIP2_SOURCE_DIR}/compress.c
            ${BZIP2_SOURCE_DIR}/crctable.c
            ${BZIP2_SOURCE_DIR}/decompress.c
            ${BZIP2_SOURCE_DIR}/huffman.c
            ${BZIP2_SOURCE_DIR}/randtable.c)

        set(BZIP2_HDR
            ${BZIP2_SOURCE_DIR}/bzlib.h
            ${BZIP2_SOURCE_DIR}/bzlib_private.h)

        add_library(bzip2 STATIC ${BZIP2_SRC} ${BZIP2_HDR})

        target_compile_definitions(bzip2 PRIVATE -DBZ_NO_STDIO)

        list(APPEND MINIZIP_DEP bzip2)
        list(APPEND MINIZIP_INC ${BZIP2_SOURCE_DIR})
    else()
        message(STATUS "BZip2 library not found")

        set(MZ_BZIP2 OFF)
    endif()

    if(MZ_BZIP2)
        list(APPEND MINIZIP_DEP_PKG BZip2)
        list(APPEND MINIZIP_DEF -DHAVE_BZIP2)
        list(APPEND MINIZIP_SRC mz_strm_bzip.c)
        list(APPEND MINIZIP_HDR mz_strm_bzip.h)
    endif()
endif()

if(MZ_LZMA)
    # Check if liblzma is present
    if(NOT MZ_FORCE_FETCH_LIBS)
        find_package(PkgConfig QUIET)
        if(PKGCONFIG_FOUND)
            pkg_check_modules(LIBLZMA liblzma)
        endif()
        if(NOT LIBLZMA_FOUND)
            find_package(LibLZMA QUIET)
            set(LIBLZMA_VERSION ${LIBLZMA_VERSION_STRING})
        endif()
    endif()

    if(LIBLZMA_FOUND AND NOT MZ_FORCE_FETCH_LIBS)
        message(STATUS "Using LZMA ${LIBLZMA_VERSION}")

        list(APPEND MINIZIP_INC ${LIBLZMA_INCLUDE_DIRS})
        list(APPEND MINIZIP_LIB ${LIBLZMA_LIBRARIES})

        set(PC_PRIVATE_LIBS "${PC_PRIVATE_LIBS} -lliblzma")
    elseif(MZ_FETCH_LIBS)
        clone_repo(liblzma https://git.tukaani.org/xz.git)

        # Don't automatically add all targets to the solution
        add_subdirectory(${LIBLZMA_SOURCE_DIR} ${LIBLZMA_BINARY_DIR} EXCLUDE_FROM_ALL)

        list(APPEND MINIZIP_INC ${LIBLZMA_SOURCE_DIR}/src/liblzma/api)
        list(APPEND MINIZIP_DEP liblzma)
        list(APPEND MINIZIP_LIB ${LIBLZMA_TARGET})
    else()
        message(STATUS "LibLZMA library not found")

        set(MZ_LZMA OFF)
    endif()

    if(MZ_LZMA)
        list(APPEND MINIZIP_DEP_PKG LibLZMA)
        list(APPEND MINIZIP_DEF -DHAVE_LZMA -DLZMA_API_STATIC)
        list(APPEND MINIZIP_SRC mz_strm_lzma.c)
        list(APPEND MINIZIP_HDR mz_strm_lzma.h)
    endif()
endif()

if(MZ_ZSTD)
    # Check if zstd is present
    if(NOT MZ_FORCE_FETCH_LIBS)
        find_package(PkgConfig QUIET)
        if(PKGCONFIG_FOUND)
            pkg_check_modules(ZSTD libzstd)
        endif()
        if(NOT ZSTD_FOUND)
            find_package(ZSTD QUIET)
            if(ZSTD_FOUND)
                if(TARGET zstd::libzstd_static)
                    list(APPEND ZSTD_LIBRARIES zstd::libzstd_static)
                else()
                    list(APPEND ZSTD_LIBRARIES zstd::libzstd_shared)
                endif()
            endif()
        endif()
    endif()

    if(ZSTD_FOUND AND NOT MZ_FORCE_FETCH_LIBS)
        message(STATUS "Using ZSTD ${ZSTD_VERSION}")

        list(APPEND MINIZIP_INC ${ZSTD_INCLUDE_DIRS})
        list(APPEND MINIZIP_LIB ${ZSTD_LIBRARIES})
        list(APPEND MINIZIP_LBD ${ZSTD_LIBRARY_DIRS})

        set(PC_PRIVATE_LIBS "${PC_PRIVATE_LIBS} -lzstd")
    elseif(MZ_FETCH_LIBS)
        clone_repo(zstd https://github.com/facebook/zstd)

        # Don't automatically add all targets to the solution
        add_subdirectory(${ZSTD_SOURCE_DIR}/build/cmake ${ZSTD_BINARY_DIR} EXCLUDE_FROM_ALL)

        list(APPEND MINIZIP_INC ${ZSTD_SOURCE_DIR}/lib)
        if(NOT DEFINED BUILD_SHARED_LIBS OR NOT ${BUILD_SHARED_LIBS})
            list(APPEND MINIZIP_DEP libzstd_static)
        else()
            list(APPEND MINIZIP_DEP libzstd_shared)
        endif()
    else()
        message(STATUS "ZSTD library not found")

        set(MZ_ZSTD OFF)
    endif()

    if(MZ_ZSTD)
        list(APPEND MINIZIP_DEP_PKG zstd)
        list(APPEND MINIZIP_DEF -DHAVE_ZSTD)
        list(APPEND MINIZIP_SRC mz_strm_zstd.c)
        list(APPEND MINIZIP_HDR mz_strm_zstd.h)
    endif()
endif()

if(NOT MZ_LIBCOMP AND NOT MZ_ZLIB AND NOT MZ_ZSTD AND NOT MZ_BZIP2 AND NOT MZ_LZMA)
    message(STATUS "Compression not supported due to missing libraries")

    list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_DECOMPRESSION)
    list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_COMPRESSION)
endif()

if(MZ_OPENSSL)
    # Check to see if openssl installation is present
    find_package(PkgConfig)
    if(PKGCONFIG_FOUND)
        pkg_check_modules(OPENSSL openssl)
    endif()
    if(NOT OPENSSL_FOUND)
        find_package(OpenSSL)
    endif()

    if(OPENSSL_FOUND)
        message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")

        list(APPEND MINIZIP_SRC mz_crypt_openssl.c)
        list(APPEND MINIZIP_LIB ${OPENSSL_LIBRARIES})
        list(APPEND MINIZIP_LBD ${OPENSSL_LIBRARY_DIRS})
        list(APPEND MINIZIP_INC ${OPENSSL_INCLUDE_DIR})

        if(OPENSSL_INCLUDE_DIRS)
            list(APPEND MINIZIP_INC ${OPENSSL_INCLUDE_DIRS})
        endif()
    else()
        message(STATUS "OpenSSL library not found")

        set(MZ_OPENSSL OFF)
    endif()
endif()

# Windows specific
if(WIN32)
    list(APPEND MINIZIP_DEF -D_CRT_SECURE_NO_DEPRECATE)
    list(APPEND MINIZIP_SRC mz_os_win32.c mz_strm_os_win32.c)

    if(MZ_PKCRYPT OR MZ_WZAES OR MZ_SIGNING)
        if(MZ_OPENSSL)
            list(APPEND MINIZIP_DEP_PKG OpenSSL)
        else()
            message(STATUS "Using CryptoAPI")

            list(APPEND MINIZIP_SRC mz_crypt_win32.c)
            list(APPEND MINIZIP_LIB crypt32.lib)
        endif()
    else()
        list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_CRYPTO)
    endif()
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "WindowsStore")
    list(APPEND MINIZIP_DEF -DMZ_WINRT_API)
endif()

# Unix specific
if(UNIX)
    list(APPEND STDLIB_DEF -D_POSIX_C_SOURCE=200112L)
    list(APPEND MINIZIP_SRC mz_os_posix.c mz_strm_os_posix.c)

    if(MZ_PKCRYPT OR MZ_WZAES OR MZ_SIGNING)
        if(MZ_OPENSSL)
            list(APPEND MINIZIP_DEP_PKG OpenSSL)
        else()
            if(APPLE)
                message(STATUS "Using CoreFoundation Framework")
                find_library(COREFOUNDATION_LIBRARY CoreFoundation)

                list(APPEND MINIZIP_LIB ${COREFOUNDATION_LIBRARY})

                message(STATUS "Using Security Framework")
                find_library(SECURITY_LIBRARY Security)

                list(APPEND MINIZIP_LIB ${SECURITY_LIBRARY})
                list(APPEND MINIZIP_LFG "-Wl,-F/Library/Frameworks")

                check_include_file(CommonCrypto/CommonCrypto.h COMMONCRYPTO_FOUND)
                if(COMMONCRYPTO_FOUND)
                    message(STATUS "Using CommonCrypto")

                    list(APPEND MINIZIP_SRC mz_crypt_apple.c)

                    set(MZ_LIBBSD OFF)
                else()
                    message(STATUS "CommonCrypto library not found")

                    list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_CRYPTO)

                    if(MZ_WZAES)
                        message(STATUS "WinZIP AES support requires CommonCrypto or OpenSSL")

                        set(MZ_WZAES OFF)
                    endif()
                    if(MZ_SIGNING)
                        message(STATUS "Signing support requires CommonCrypto or OpenSSL")

                        set(MZ_SIGNING OFF)
                    endif()
                endif()
            else()
                list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_CRYPTO)

                if(MZ_WZAES)
                    message(STATUS "WinZIP AES support requires OpenSSL")

                    set(MZ_WZAES OFF)
                endif()
                if(MZ_SIGNING)
                    message(STATUS "Signing support requires OpenSSL")

                    set(MZ_SIGNING OFF)
                endif()
            endif()

            if(MZ_PKCRYPT AND NOT MZ_WZAES)
                # Check to see which random generation functions we have
                check_symbol_exists("getrandom" "sys/random.h" HAVE_GETRANDOM)
                if(HAVE_GETRANDOM)
                    list(APPEND MINIZIP_DEF -DHAVE_GETRANDOM)
                endif()
                check_symbol_exists("arc4random_buf" "stdlib.h" HAVE_ARC4RANDOM_BUF)
                if(HAVE_ARC4RANDOM_BUF)
                    list(APPEND MINIZIP_DEF -DHAVE_ARC4RANDOM_BUF)
                else()
                    check_symbol_exists("arc4random" "stdlib.h" HAVE_ARC4RANDOM)
                    if(HAVE_ARC4RANDOM)
                        list(APPEND MINIZIP_DEF -DHAVE_ARC4RANDOM)
                    endif()
                endif()

                if(APPLE)
                    # Requires _DARWIN_C_SOURCE for arcrandom functions
                    list(APPEND MINIZIP_DEF -D_DARWIN_C_SOURCE)
                endif()

                if(MZ_LIBBSD AND NOT HAVE_ARC4RANDOM_BUF)
                    find_package(PkgConfig REQUIRED)

                    pkg_check_modules(LIBBSD libbsd)
                    if(LIBBSD_FOUND)
                        check_library_exists("${LIBBSD_LIBRARIES}" "arc4random_buf"
                            "${LIBBSD_LIBRARY_DIRS}" HAVE_LIBBSD_ARC4RANDOM_BUF)

                        if(HAVE_LIBBSD_ARC4RANDOM_BUF)
                            list(APPEND MINIZIP_DEF -DHAVE_LIBBSD -DHAVE_ARC4RANDOM_BUF)
                            list(APPEND MINIZIP_INC ${LIBBSD_INCLUDE_DIRS})
                            list(APPEND MINIZIP_LIB ${LIBBSD_LIBRARIES})
                            list(APPEND MINIZIP_LBD ${LIBBSD_LIBRARY_DIRS})

                            link_directories(${LIBBSD_LIBRARY_DIRS})
                        endif()
                    else()
                        set(MZ_LIBBSD OFF)
                    endif()
                else()
                    set(MZ_LIBBSD OFF)
                endif()

                if(NOT MZ_LIBBSD AND NOT HAVE_GETRANDOM AND NOT HAVE_ARC4RANDOM_BUF AND NOT HAVE_ARC4RANDOM)
                    message(WARNING "Low quality entropy function used for encryption")
                endif()
            endif()
        endif()
    else()
        list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_CRYPTO)
    endif()

    # Iconv is only necessary when it is not already built-in
    # FindIconv requires cmake 3.11 or higher
    if (MZ_ICONV)
        find_package(Iconv QUIET)
    endif()

    if(Iconv_FOUND)
        message(STATUS "Using Iconv")

        list(APPEND MINIZIP_DEF -DHAVE_ICONV)
        list(APPEND MINIZIP_INC ${Iconv_INCLUDE_DIRS})
        list(APPEND MINIZIP_DEP_PKG Iconv)
        if(NOT Iconv_IS_BUILT_IN)
            list(APPEND MINIZIP_LIB ${Iconv_LIBRARIES})
            list(APPEND MINIZIP_LBD ${Iconv_LIBRARY_DIRS})
        endif()

        set(PC_PRIVATE_LIBS "${PC_PRIVATE_LIBS} -liconv")
    else()
        message(STATUS "Character encoding support requires iconv")

        set(MZ_ICONV OFF)
    endif()
else()
    set(MZ_LIBBSD OFF)
    set(MZ_ICONV OFF)
endif()

# Setup predefined macros
if(MZ_COMPRESS_ONLY)
    list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_DECOMPRESSION)
endif()
if(MZ_DECOMPRESS_ONLY)
    list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_COMPRESSION)
endif()
if(NOT MZ_PKCRYPT AND NOT MZ_WZAES)
    list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_ENCRYPTION)
endif()
if(MZ_SIGNING)
    list(APPEND MINIZIP_DEF -DMZ_ZIP_SIGNING)
endif()
if(MZ_FILE32_API)
    list(APPEND MINIZIP_DEF -DMZ_FILE32_API)
endif()

# Include traditional PKWare encryption
if(MZ_PKCRYPT)
    list(APPEND MINIZIP_DEF -DHAVE_PKCRYPT)
    list(APPEND MINIZIP_SRC mz_strm_pkcrypt.c)
    list(APPEND MINIZIP_HDR mz_strm_pkcrypt.h)
endif()

# Include WinZIP AES encryption
if(MZ_WZAES)
    list(APPEND MINIZIP_DEF -DHAVE_WZAES)
    list(APPEND MINIZIP_SRC mz_strm_wzaes.c)
    list(APPEND MINIZIP_HDR mz_strm_wzaes.h)
endif()

# Include compatibility layer
if(MZ_COMPAT)
    set(COMPAT_HEADER "\
/* file.h -- Compatibility layer shim\n\
   part of the minizip-ng project\n\n\
   This program is distributed under the terms of the same license as zlib.\n\
   See the accompanying LICENSE file for the full text of the license.\n\
*/\n\n\
#ifndef MZ_COMPAT_FILE\n\
#define MZ_COMPAT_FILE\n\n\
#include \"mz_compat.h\"\n\n\
#endif\n")

    string(REPLACE "file.h" "zip.h" ZIP_COMPAT_HEADER ${COMPAT_HEADER})
    string(REPLACE "MZ_COMPAT_FILE" "MZ_COMPAT_ZIP" ZIP_COMPAT_HEADER ${ZIP_COMPAT_HEADER})
    file(WRITE "zip.h" ${ZIP_COMPAT_HEADER})

    string(REPLACE "file.h" "unzip.h" UNZIP_COMPAT_HEADER ${COMPAT_HEADER})
    string(REPLACE "MZ_COMPAT_FILE" "MZ_COMPAT_UNZIP" UNZIP_COMPAT_HEADER ${UNZIP_COMPAT_HEADER})
    file(WRITE "unzip.h" ${UNZIP_COMPAT_HEADER})

    if(MZ_COMPAT_VERSION)
        list(APPEND MINIZIP_DEF -DMZ_COMPAT_VERSION=${MZ_COMPAT_VERSION})
    endif()
    list(APPEND MINIZIP_SRC mz_compat.c)
    list(APPEND MINIZIP_HDR mz_compat.h zip.h unzip.h)
endif()

# Set compiler options
if(MZ_CODE_COVERAGE)
    if(NOT MSVC)
        message(STATUS "Code coverage enabled")
        add_compile_options(-O0 -g -fprofile-arcs -ftest-coverage)
        if(CMAKE_C_COMPILER_ID MATCHES "(Apple)?[Cc]lang")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
        elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
            link_libraries(gcov)
        endif()
        set_property(DIRECTORY PROPERTY
            GCC_INSTRUMENT_PROGRAM_FLOW_ARCS YES
            GCC_GENERATE_TEST_COVERAGE_FILES YES)
    else()
        set(MZ_CODE_COVERAGE OFF)
    endif()
else()
    if(MSVC)
        add_compile_options(
            $<$<CONFIG:Debug>:/Zi>
            $<$<CONFIG:Debug>:/Od>
            $<$<CONFIG:Debug>:/W3>
            $<$<CONFIG:Release>:/Ox>
            $<$<CONFIG:Release>:/Os>)
    else()
        add_compile_options(
            $<$<CONFIG:Debug>:-g>
            $<$<CONFIG:Debug>:-Wall>
            $<$<CONFIG:Release>:-Os>)
    endif()
endif()

list(APPEND MINIZIP_INC ${CMAKE_CURRENT_SOURCE_DIR})

# Create minizip library
project(minizip${MZ_PROJECT_SUFFIX} VERSION ${VERSION})

if(NOT ${MZ_PROJECT_SUFFIX} STREQUAL "")
    message(STATUS "Project configured as ${PROJECT_NAME}")
endif()

set(MINIZIP_PC ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc)
configure_file(minizip.pc.cmakein ${MINIZIP_PC} @ONLY)

set(INSTALL_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}"
    CACHE PATH "Installation directory for cmake files.")
set(INSTALL_PKGCONFIG_DIR "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
    CACHE PATH "Installation directory for pkgconfig (.pc) files")

add_library(${PROJECT_NAME} ${MINIZIP_SRC} ${MINIZIP_HDR})

set_target_properties(${PROJECT_NAME} PROPERTIES
                      VERSION ${VERSION}
                      SOVERSION ${SOVERSION}
                      LINKER_LANGUAGE C
                      DEFINE_SYMBOL "MZ_EXPORTS")

if(MINIZIP_LFG)
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS ${MINIZIP_LFG})
endif()
if(MSVC)
    # VS debugger has problems when executable and static library are named the same
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME lib${PROJECT_NAME})
endif()
if(NOT RISCOS)
    set_target_properties(${PROJECT_NAME} PROPERTIES POSITION_INDEPENDENT_CODE 1)
endif()
if(MZ_LZMA)
    set_target_properties(${PROJECT_NAME} PROPERTIES C_STANDARD 99)
endif()

target_link_libraries(${PROJECT_NAME} PUBLIC ${MINIZIP_LIB} ${MINIZIP_DEP})
target_link_directories(${PROJECT_NAME} PUBLIC ${MINIZIP_LBD})
target_compile_definitions(${PROJECT_NAME} PRIVATE ${STDLIB_DEF} ${MINIZIP_DEF})
target_include_directories(${PROJECT_NAME} PRIVATE ${MINIZIP_INC})
target_include_directories(${PROJECT_NAME} PUBLIC
    $<INSTALL_INTERFACE:${INSTALL_INC_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)

# Create minizip alias
add_library(MINIZIP::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

# Install files
if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    install(TARGETS ${PROJECT_NAME} ${MINIZIP_DEP}
            EXPORT ${PROJECT_NAME}
            INCLUDES DESTINATION "${INSTALL_INC_DIR}"
            RUNTIME DESTINATION "${INSTALL_BIN_DIR}"
            ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
            LIBRARY DESTINATION "${INSTALL_LIB_DIR}")
    install(EXPORT ${PROJECT_NAME}
            DESTINATION "${INSTALL_CMAKE_DIR}"
            NAMESPACE "MINIZIP::")

    # Create and install CMake package config version file to allow find_package()
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
        COMPATIBILITY SameMajorVersion)
    set(MINIZIP_CONFIG_CONTENT "@PACKAGE_INIT@\n")
    if(MINIZIP_DEP_PKG)
        string(APPEND MINIZIP_CONFIG_CONTENT "include(CMakeFindDependencyMacro)\n")
        foreach(PKG_NAME ${MINIZIP_DEP_PKG})
            string(APPEND MINIZIP_CONFIG_CONTENT "find_dependency(${PKG_NAME} REQUIRED)\n")
        endforeach()
    endif()
    string(APPEND MINIZIP_CONFIG_CONTENT "include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}.cmake\")")
    file(WRITE minizip-config.cmake.in ${MINIZIP_CONFIG_CONTENT})

    # Create config for find_package()
    configure_package_config_file(minizip-config.cmake.in ${PROJECT_NAME}-config.cmake
        INSTALL_DESTINATION "${INSTALL_CMAKE_DIR}")

    file(REMOVE minizip-config.cmake.in)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config-version.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}-config.cmake
            DESTINATION "${INSTALL_CMAKE_DIR}")
endif()
if(NOT SKIP_INSTALL_HDR AND NOT SKIP_INSTALL_ALL)
    install(FILES ${MINIZIP_HDR} DESTINATION "${INSTALL_INC_DIR}")
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL)
    install(FILES ${MINIZIP_PC} DESTINATION "${INSTALL_PKGCONFIG_DIR}")
endif()

# Build test executables
if(MZ_BUILD_TESTS)
    if(MZ_ZLIB AND NOT MZ_LIBCOMP)
        add_executable(minigzip_cmd minigzip.c)
        set_target_properties(minigzip_cmd PROPERTIES OUTPUT_NAME minigzip)
        target_compile_definitions(minigzip_cmd PRIVATE ${STDLIB_DEF} ${MINIZIP_DEF})
        target_include_directories(minigzip_cmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(minigzip_cmd ${PROJECT_NAME})

        if(NOT SKIP_INSTALL_BINARIES AND NOT SKIP_INSTALL_ALL)
            install(TARGETS minigzip_cmd RUNTIME DESTINATION "bin")
        endif()
    endif()

    add_executable(minizip_cmd minizip.c)
    set_target_properties(minizip_cmd PROPERTIES OUTPUT_NAME ${PROJECT_NAME})
    target_compile_definitions(minizip_cmd PRIVATE ${STDLIB_DEF} ${MINIZIP_DEF})
    target_include_directories(minizip_cmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(minizip_cmd ${PROJECT_NAME})

    if(NOT SKIP_INSTALL_BINARIES AND NOT SKIP_INSTALL_ALL)
        install(TARGETS minizip_cmd RUNTIME DESTINATION "bin")
    endif()

    add_executable(test_cmd test/test.c test/test.h)
    target_compile_definitions(test_cmd PRIVATE ${STDLIB_DEF} ${MINIZIP_DEF})
    if(MZ_COMPAT)
        target_compile_definitions(test_cmd PRIVATE -DHAVE_COMPAT)
    endif()
    target_include_directories(test_cmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(test_cmd ${PROJECT_NAME})
endif()

if(MZ_BUILD_TESTS AND MZ_BUILD_UNIT_TESTS)
    enable_testing()

    # Can't disable zlib testing so ctest tries to run zlib example app
    if(MZ_ZLIB AND NOT MZ_LIBCOMP AND NOT ZLIB_FOUND)
        add_dependencies(${PROJECT_NAME} example)
        if(HAVE_OFF64_T)
            add_dependencies(${PROJECT_NAME} example64)
        endif()
    endif()

    add_test(NAME test_cmd COMMAND test_cmd WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

    function(create_compress_tests EXTRA_NAME EXTRA_ARGS)
        if(MZ_DECOMPRESS_ONLY)
            return()
        endif()
        list(FIND EXTRA_ARGS "-z" ZIPCD_IDX)
        if(${ZIPCD_IDX} EQUAL -1)
            set(COMPRESS_METHOD_NAMES "raw")
            set(COMPRESS_METHOD_ARGS "-0")
        endif()
        if(MZ_ZLIB OR MZ_LIBCOMP)
            list(APPEND COMPRESS_METHOD_NAMES "deflate")
            list(APPEND COMPRESS_METHOD_ARGS "-9")
        endif()
        if(MZ_BZIP2)
            list(APPEND COMPRESS_METHOD_NAMES "bzip2")
            list(APPEND COMPRESS_METHOD_ARGS "-b")
        endif()
        if(MZ_LZMA)
            list(APPEND COMPRESS_METHOD_NAMES "lzma")
            list(APPEND COMPRESS_METHOD_ARGS "-m")
        endif()
        if(MZ_LZMA OR MZ_LIBCOMP)
            list(APPEND COMPRESS_METHOD_NAMES "xz")
            list(APPEND COMPRESS_METHOD_ARGS "-n")
        endif()
        if(MZ_ZSTD)
            list(APPEND COMPRESS_METHOD_NAMES "zstd")
            list(APPEND COMPRESS_METHOD_ARGS "-t")
        endif()
        list(LENGTH COMPRESS_METHOD_NAMES COMPRESS_METHOD_COUNT)
        math(EXPR COMPRESS_METHOD_COUNT "${COMPRESS_METHOD_COUNT}-1")
        foreach(INDEX RANGE ${COMPRESS_METHOD_COUNT})
            list(GET COMPRESS_METHOD_NAMES ${INDEX} COMPRESS_METHOD_NAME)
            list(GET COMPRESS_METHOD_ARGS ${INDEX} COMPRESS_METHOD_ARG)
            add_test(NAME ${COMPRESS_METHOD_NAME}-zip-${EXTRA_NAME}
                     COMMAND minizip_cmd ${COMPRESS_METHOD_ARG} -o ${EXTRA_ARGS}
                        result.zip test.c test.h empty.txt random.bin uniform.bin fuzz
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            add_test(NAME ${COMPRESS_METHOD_NAME}-list-${EXTRA_NAME}
                     COMMAND minizip_cmd -l ${EXTRA_ARGS} result.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            if(NOT MZ_COMPRESS_ONLY)
                add_test(NAME ${COMPRESS_METHOD_NAME}-unzip-${EXTRA_NAME}
                         COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out result.zip
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            endif()
            add_test(NAME ${COMPRESS_METHOD_NAME}-append-${EXTRA_NAME}
                    COMMAND minizip_cmd ${COMPRESS_METHOD_ARG} -a ${EXTRA_ARGS}
                        result.zip single.txt
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            if(NOT MZ_COMPRESS_ONLY)
                add_test(NAME ${COMPRESS_METHOD_NAME}-append-unzip-${EXTRA_NAME}
                            COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out result.zip
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            endif()
            add_test(NAME ${COMPRESS_METHOD_NAME}-erase-${EXTRA_NAME}
                    COMMAND minizip_cmd -o -e result.zip test.c test.h
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            if(NOT MZ_COMPRESS_ONLY)
                add_test(NAME ${COMPRESS_METHOD_NAME}-erase-unzip-${EXTRA_NAME}
                         COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out result.zip
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            endif()
        endforeach()
    endfunction()

    # Perform tests against ourself
    create_compress_tests("generic" "")
    create_compress_tests("span" "-k;1024")
    create_compress_tests("zipcd" "-z")
    if(MZ_PKCRYPT)
        create_compress_tests("pkcrypt" "-p;test123")
    endif()
    if(MZ_WZAES)
        create_compress_tests("wzaes" "-s;-p;test123")
    endif()
    if(MZ_SIGNING)
        create_compress_tests("signed" "-h;test.p12;-w;test")
        create_compress_tests("secure" "-z;-h;test.p12;-w;test")
    endif()

    # Perform tests on others
    if(NOT MZ_COMPRESS_ONLY)
        if(MZ_ZLIB OR MZ_LIBCOMP)
            add_test(NAME unzip-tiny
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out
                        fuzz/unzip_fuzzer_seed_corpus/tiny.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_BZIP2)
            add_test(NAME unzip-bzip2
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out
                        fuzz/unzip_fuzzer_seed_corpus/bzip2.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_LZMA)
            add_test(NAME unzip-lzma
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out
                        fuzz/unzip_fuzzer_seed_corpus/lzma.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_PKCRYPT)
            add_test(NAME unzip-pkcrypt
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out -p test123
                        fuzz/unzip_fuzzer_seed_corpus/encrypted_pkcrypt.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_WZAES)
            add_test(NAME unzip-wzaes
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS} -d out -p test123
                        fuzz/unzip_fuzzer_seed_corpus/encrypted_wzaes.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
    endif()
    if(NOT MZ_COMPRESS_ONLY AND NOT MZ_DECOMPRESS_ONLY)
        if(MZ_ZLIB AND NOT MZ_LIBCOMP)
            add_test(NAME gz
                COMMAND minigzip_cmd random.bin
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            add_test(NAME ungz
                COMMAND minigzip_cmd -x -d out random.bin.gz
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
    endif()
endif()

#Build fuzzer executables
if(MZ_BUILD_FUZZ_TESTS)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
        enable_language(CXX)

        if(DEFINED ENV{LIB_FUZZING_ENGINE})
            set(FUZZING_ENGINE $ENV{LIB_FUZZING_ENGINE})
            set(FUZZING_ENGINE_FOUND TRUE)
        else()
            find_library(FUZZING_ENGINE "FuzzingEngine")
        endif()
    endif()

    if(NOT FUZZING_ENGINE_FOUND)
        set(FUZZER_SRC "test/fuzz/standalone.c")
    else()
        set(FUZZER_SRC)
    endif()

    macro(configure_fuzz_test target)
        add_executable(${target} "test/fuzz/${target}.c" ${FUZZER_SRC})
        set_target_properties(${target} PROPERTIES LINKER_LANGUAGE CXX)
        target_compile_definitions(${target} PRIVATE ${STDLIB_DEF})
        target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(${target} ${PROJECT_NAME})

        add_dependencies(${target} ${PROJECT_NAME})
        if(FUZZING_ENGINE_FOUND)
            target_link_libraries(${target} ${FUZZING_ENGINE})
        endif()
    endmacro()

    configure_fuzz_test(zip_fuzzer)
    configure_fuzz_test(unzip_fuzzer)

    if(NOT SKIP_INSTALL_BINARIES AND NOT SKIP_INSTALL_ALL)
        install(TARGETS zip_fuzzer RUNTIME DESTINATION "bin")
        install(TARGETS unzip_fuzzer RUNTIME DESTINATION "bin")
    endif()
endif()

# Compatibility options
add_feature_info(MZ_COMPAT MZ_COMPAT "Enables compatibility layer")
# Compression library options
add_feature_info(MZ_ZLIB MZ_ZLIB "Enables ZLIB compression")
add_feature_info(MZ_BZIP2 MZ_BZIP2 "Enables BZIP2 compression")
add_feature_info(MZ_LZMA MZ_LZMA "Enables LZMA & XZ compression")
add_feature_info(MZ_ZSTD MZ_ZSTD "Enables ZSTD compression")
add_feature_info(MZ_LIBCOMP MZ_LIBCOMP "Enables Apple compression")
add_feature_info(MZ_FETCH_LIBS MZ_FETCH_LIBS "Enables fetching third-party libraries if not found")
add_feature_info(MZ_FORCE_FETCH_LIBS MZ_FORCE_FETCH_LIBS "Enables fetching third-party libraries always")
# Encryption support options
add_feature_info(MZ_PKCRYPT MZ_PKCRYPT "Enables PKWARE traditional encryption")
add_feature_info(MZ_WZAES MZ_WZAES "Enables WinZIP AES encryption")
add_feature_info(MZ_OPENSSL MZ_OPENSSL "Enables OpenSSL for encryption")
add_feature_info(MZ_LIBBSD MZ_LIBBSD "Build with libbsd for crypto random")
add_feature_info(MZ_SIGNING MZ_SIGNING "Enables zip signing support")
# Character conversion options
add_feature_info(MZ_ICONV MZ_ICONV "Enables iconv string encoding conversion library")
# Code generation options
add_feature_info(MZ_COMPRESS_ONLY MZ_COMPRESS_ONLY "Only support compression")
add_feature_info(MZ_DECOMPRESS_ONLY MZ_DECOMPRESS_ONLY "Only support decompression")
add_feature_info(MZ_FILE32_API MZ_FILE32_API "Builds using posix 32-bit file api")
# Build and continuous integration options
add_feature_info(MZ_BUILD_TESTS MZ_BUILD_TESTS "Builds minizip test executable")
add_feature_info(MZ_BUILD_UNIT_TESTS MZ_BUILD_UNIT_TESTS "Builds minizip unit test project")
add_feature_info(MZ_BUILD_FUZZ_TESTS MZ_BUILD_FUZZ_TESTS "Builds minizip fuzzer executables")
add_feature_info(MZ_CODE_COVERAGE MZ_CODE_COVERAGE "Builds with code coverage flags")

feature_summary(WHAT ENABLED_FEATURES DISABLED_FEATURES INCLUDE_QUIET_PACKAGES)
