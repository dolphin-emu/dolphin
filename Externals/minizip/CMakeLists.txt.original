# minizip-ng project
#
# Copyright (C) Nathan Moinvaziri
#   https://github.com/zlib-ng/minizip-ng
# Copyright (C) 2016 Matthias Schmieder
#   schmieder.matthias@gmail.com

cmake_minimum_required(VERSION 3.13)

# Library version
set(VERSION "4.0.1")
# API version
set(SOVERSION "4")

project(minizip-ng
    VERSION ${VERSION}
    LANGUAGES C
    DESCRIPTION "Zip manipulation library"
	HOMEPAGE_URL https://github.com/zlib-ng/minizip-ng/)

include(CMakeDependentOption)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckCSourceCompiles)
include(CheckTypeSize)
include(GNUInstallDirs)
include(FeatureSummary)

include(cmake/detect-sanitizer.cmake)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

message(STATUS "Using CMake version ${CMAKE_VERSION}")

# Compatibility options
option(MZ_COMPAT "Enables compatibility layer" ON)
# Compression library options
option(MZ_ZLIB "Enables ZLIB compression" ON)
option(MZ_BZIP2 "Enables BZIP2 compression" ON)
option(MZ_LZMA "Enables LZMA & XZ compression" ON)
option(MZ_ZSTD "Enables ZSTD compression" ON)
cmake_dependent_option(MZ_LIBCOMP "Enables Apple compression" ON "APPLE" OFF)
option(MZ_FETCH_LIBS "Enables fetching third-party libraries if not found" ${WIN32})
option(MZ_FORCE_FETCH_LIBS "Enables fetching third-party libraries always" OFF)
# Encryption support options
option(MZ_PKCRYPT "Enables PKWARE traditional encryption" ON)
option(MZ_WZAES "Enables WinZIP AES encryption" ON)
cmake_dependent_option(MZ_OPENSSL "Enables OpenSSL for encryption" ON "UNIX" OFF)
cmake_dependent_option(MZ_LIBBSD "Builds with libbsd crypto random" ON "UNIX" OFF)
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
# Multi-choice code sanitizer option
set(MZ_SANITIZER AUTO CACHE STRING "Enable sanitizer support")
set_property(CACHE MZ_SANITIZER PROPERTY STRINGS "Memory" "Address" "Undefined" "Thread")

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
if(DEFINED MZ_PROJECT_SUFFIX)
    set(MZ_LIB_SUFFIX ${MZ_PROJECT_SUFFIX})
endif()

# Package management options
if(NOT MZ_COMPAT AND NOT DEFINED MZ_LIB_SUFFIX)
    set(MZ_LIB_SUFFIX "-ng" CACHE STRING "Library name suffix for package managers")
else()
    set(MZ_LIB_SUFFIX "" CACHE STRING "Library name suffix for package managers")
endif()

mark_as_advanced(MZ_FILE32_API MZ_LIB_SUFFIX)

# ZLIB_ROOT - Parent directory of zlib installation
# BZIP2_ROOT - Parent directory of BZip2 installation
# OPENSSL_ROOT - Parent directory of OpenSSL installation

include(cmake/clone-repo.cmake)

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
        find_package(ZLIBNG QUIET)
        find_package(ZLIB QUIET)
        set(ZLIB_VERSION ${ZLIB_VERSION_STRING})
    endif()

    if(ZLIBNG_FOUND AND NOT MZ_FORCE_FETCH_LIBS)
        message(STATUS "Using ZLIBNG")

        list(APPEND MINIZIP_INC ${ZLIBNG_INCLUDE_DIRS})
        list(APPEND MINIZIP_LIB ${ZLIBNG_LIBRARIES})
        list(APPEND MINIZIP_LBD ${ZLIBNG_LIBRARY_DIRS})

        set(PC_PRIVATE_LIBS " -lz-ng")
        set(ZLIB_COMPAT OFF)
    elseif(ZLIB_FOUND AND NOT MZ_FORCE_FETCH_LIBS)
        message(STATUS "Using ZLIB ${ZLIB_VERSION}")

        list(APPEND MINIZIP_INC ${ZLIB_INCLUDE_DIRS})
        list(APPEND MINIZIP_LIB ${ZLIB_LIBRARIES})
        list(APPEND MINIZIP_LBD ${ZLIB_LIBRARY_DIRS})

        set(PC_PRIVATE_LIBS " -lz")
        set(ZLIB_COMPAT ON)
    elseif(MZ_FETCH_LIBS)
        clone_repo(zlib https://github.com/madler/zlib)

        # Don't automatically add all targets to the solution
        add_subdirectory(${ZLIB_SOURCE_DIR} ${ZLIB_BINARY_DIR} EXCLUDE_FROM_ALL)

        list(APPEND MINIZIP_INC ${ZLIB_SOURCE_DIR})
        list(APPEND MINIZIP_INC ${ZLIB_BINARY_DIR})

        # Have to add zlib to install targets
        if(NOT DEFINED BUILD_SHARED_LIBS OR NOT BUILD_SHARED_LIBS)
            list(APPEND MINIZIP_DEP zlibstatic)
        else()
            list(APPEND MINIZIP_DEP zlib)
        endif()

        if(EXISTS "${ZLIB_BINARY_DIR}/zlib-ng.h")
            message(STATUS "ZLIB repository detected as ZLIBNG")
            set(ZLIB_COMPAT OFF)
        else()
            set(ZLIB_COMPAT ON)
        endif()
    else()
        message(STATUS "ZLIB library not found")

        set(MZ_ZLIB OFF)
    endif()

    if(MZ_ZLIB)
        list(APPEND MINIZIP_DEF -DHAVE_ZLIB)
        if(ZLIB_COMPAT)
            list(APPEND MINIZIP_DEF -DZLIB_COMPAT)
        endif()
        if(ZLIBNG_FOUND OR NOT ZLIB_COMPAT)
            list(APPEND MINIZIP_DEP_PKG ZLIBNG)
        elseif(ZLIB_FOUND)
            list(APPEND MINIZIP_DEP_PKG ZLIB)
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
        list(APPEND MINIZIP_LBD ${LIBLZMA_LIBRARY_DIRS})

        set(PC_PRIVATE_LIBS "${PC_PRIVATE_LIBS} -llzma")
    elseif(MZ_FETCH_LIBS)
        set(BUILD_TESTING OFF CACHE BOOL "Build lzma tests" FORCE)

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
        set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "Build zstd programs")

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

        if(OPENSSL_INCLUDE_DIRS)
            list(APPEND MINIZIP_INC ${OPENSSL_INCLUDE_DIRS})
        endif()
        if(OPENSSL_INCLUDE_DIR)
            list(APPEND MINIZIP_INC ${OPENSSL_INCLUDE_DIR})
        endif()

        foreach(i ${OPENSSL_LIBRARIES})
            set(PC_PRIVATE_LIBS "${PC_PRIVATE_LIBS} -l${i}")
        endforeach()
    else()
        message(STATUS "OpenSSL library not found")

        set(MZ_OPENSSL OFF)
    endif()
endif()

# Platform specific
if(WIN32)
    list(APPEND MINIZIP_DEF -D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
    list(APPEND MINIZIP_SRC mz_os_win32.c mz_strm_os_win32.c)

    if(MZ_PKCRYPT OR MZ_WZAES)
        if(MZ_OPENSSL)
            list(APPEND MINIZIP_DEP_PKG OpenSSL)
        else()
            # Check for existence of BCrypt API
            set(CMAKE_REQUIRED_LIBRARIES bcrypt.lib ncrypt.lib crypt32.lib)
            check_symbol_exists(BCryptGenRandom "windows.h;bcrypt.h" HAVE_BCRYPT)
            set(CMAKE_REQUIRED_LIBRARIES)
            if (HAVE_BCRYPT)
                list(APPEND MINIZIP_SRC mz_crypt_winvista.c)
                list(APPEND MINIZIP_LIB bcrypt.lib ncrypt.lib)
            endif()

            list(APPEND MINIZIP_SRC mz_crypt_winxp.c)
            list(APPEND MINIZIP_LIB crypt32.lib)
        endif()
    else()
        list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_CRYPTO)
    endif()

    set(MZ_LIBBSD OFF)
    set(MZ_ICONV OFF)
else()
    list(APPEND STDLIB_DEF -D_POSIX_C_SOURCE=200809L -D_BSD_SOURCE)
    list(APPEND MINIZIP_SRC mz_os_posix.c mz_strm_os_posix.c)

    if(MZ_PKCRYPT OR MZ_WZAES)
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
                endif()
            else()
                list(APPEND MINIZIP_DEF -DMZ_ZIP_NO_CRYPTO)

                if(MZ_WZAES)
                    message(STATUS "WinZIP AES support requires OpenSSL")

                    set(MZ_WZAES OFF)
                endif()
            endif()

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

                pkg_check_modules(LIBBSD libbsd-overlay)
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
            set(PC_PRIVATE_LIBS "${PC_PRIVATE_LIBS} -liconv")
        endif()
    else()
        message(STATUS "Character encoding support requires iconv")

        set(MZ_ICONV OFF)
    endif()
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
    set(SOVERSION "1")

    set(FILE_H "zip.h")
    set(MZ_COMPAT_FILE "MZ_COMPAT_ZIP")
    configure_file(mz_compat_shim.h.in zip.h)

    set(FILE_H "unzip.h")
    set(MZ_COMPAT_FILE "MZ_COMPAT_UNZIP")
    configure_file(mz_compat_shim.h.in unzip.h)

    if(MZ_COMPAT_VERSION)
        list(APPEND MINIZIP_DEF -DMZ_COMPAT_VERSION=${MZ_COMPAT_VERSION})
    endif()
    list(APPEND MINIZIP_SRC mz_compat.c)
    list(APPEND MINIZIP_HDR mz_compat.h ${CMAKE_CURRENT_BINARY_DIR}/zip.h ${CMAKE_CURRENT_BINARY_DIR}/unzip.h)
endif()

# Detect available sanitizers
if(MZ_SANITIZER STREQUAL "Address")
    add_address_sanitizer()
elseif(MZ_SANITIZER STREQUAL "Memory")
    add_memory_sanitizer()
elseif(MZ_SANITIZER STREQUAL "Thread")
    add_thread_sanitizer()
elseif(MZ_SANITIZER STREQUAL "Undefined")
    add_undefined_sanitizer()
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
        add_compile_options($<$<CONFIG:Debug>:/W3>)
    else()
        add_compile_options($<$<CONFIG:Debug>:-Wall>)
    endif()
endif()

list(APPEND MINIZIP_INC ${CMAKE_CURRENT_SOURCE_DIR})

set(MINIZIP_TARGET minizip${MZ_LIB_SUFFIX})
set(MINIZIP_PC ${CMAKE_CURRENT_BINARY_DIR}/${MINIZIP_TARGET}.pc)
configure_file(minizip.pc.cmakein ${MINIZIP_PC} @ONLY)

# Create minizip library and aliases
add_library(${MINIZIP_TARGET} ${MINIZIP_SRC} ${MINIZIP_HDR})
add_library(MINIZIP::${MINIZIP_TARGET} ALIAS ${MINIZIP_TARGET})
if(NOT TARGET MINIZIP::minizip)
    add_library(MINIZIP::minizip ALIAS ${MINIZIP_TARGET})
endif()

set_target_properties(${MINIZIP_TARGET} PROPERTIES
                      VERSION ${VERSION}
                      SOVERSION ${SOVERSION}
                      LINKER_LANGUAGE C
                      DEFINE_SYMBOL "MZ_EXPORTS")

if(MINIZIP_LFG)
    set_target_properties(${MINIZIP_TARGET} PROPERTIES LINK_FLAGS ${MINIZIP_LFG})
endif()
if(MSVC)
    # VS debugger has problems when executable and static library are named the same
    set_target_properties(${MINIZIP_TARGET} PROPERTIES OUTPUT_NAME lib${MINIZIP_TARGET})
endif()
if(MZ_LZMA)
    set_target_properties(${MINIZIP_TARGET} PROPERTIES C_STANDARD 99)
endif()

target_link_libraries(${MINIZIP_TARGET} PUBLIC ${MINIZIP_LIB} ${MINIZIP_DEP})
target_link_directories(${MINIZIP_TARGET} PUBLIC ${MINIZIP_LBD})
target_compile_definitions(${MINIZIP_TARGET} PRIVATE ${STDLIB_DEF} ${MINIZIP_DEF})
target_include_directories(${MINIZIP_TARGET} PRIVATE ${MINIZIP_INC})
target_include_directories(${MINIZIP_TARGET} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
if(MZ_COMPAT)
    target_include_directories(${MINIZIP_TARGET} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
endif()

# Install files
if(NOT SKIP_INSTALL_LIBRARIES AND NOT SKIP_INSTALL_ALL)
    target_include_directories(${MINIZIP_TARGET} PUBLIC
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${MINIZIP_TARGET}>)

    install(TARGETS ${MINIZIP_TARGET} ${MINIZIP_DEP}
            EXPORT ${MINIZIP_TARGET}
            INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${MINIZIP_TARGET}"
            RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
            ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
            LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(EXPORT ${MINIZIP_TARGET}
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${MINIZIP_TARGET}"
            NAMESPACE "MINIZIP::")

    # Create and install CMake package config version file to allow find_package()
    include(CMakePackageConfigHelpers)
    write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/${MINIZIP_TARGET}-config-version.cmake
        COMPATIBILITY SameMajorVersion)
    set(MINIZIP_CONFIG_CONTENT "@PACKAGE_INIT@\n")
    if(MINIZIP_DEP_PKG)
        string(APPEND MINIZIP_CONFIG_CONTENT "include(CMakeFindDependencyMacro)\n")
        foreach(PKG_NAME ${MINIZIP_DEP_PKG})
            string(APPEND MINIZIP_CONFIG_CONTENT "find_dependency(${PKG_NAME} REQUIRED)\n")
        endforeach()
    endif()
    string(APPEND MINIZIP_CONFIG_CONTENT "include(\"\${CMAKE_CURRENT_LIST_DIR}/${MINIZIP_TARGET}.cmake\")")
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/minizip-config.cmake.in ${MINIZIP_CONFIG_CONTENT})

    # Create config for find_package()
    configure_package_config_file(
        ${CMAKE_CURRENT_BINARY_DIR}/minizip-config.cmake.in
        ${MINIZIP_TARGET}-config.cmake
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${MINIZIP_TARGET}")

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${MINIZIP_TARGET}-config-version.cmake
                  ${CMAKE_CURRENT_BINARY_DIR}/${MINIZIP_TARGET}-config.cmake
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${MINIZIP_TARGET}")
endif()
if(NOT SKIP_INSTALL_HDR AND NOT SKIP_INSTALL_ALL)
    install(FILES ${MINIZIP_HDR} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/${MINIZIP_TARGET}")
endif()
if(NOT SKIP_INSTALL_FILES AND NOT SKIP_INSTALL_ALL)
    install(FILES ${MINIZIP_PC} DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
endif()

# Build test executables
if(MZ_BUILD_TESTS)
    if(MZ_ZLIB AND NOT MZ_LIBCOMP)
        add_executable(minigzip_cmd minigzip.c)
        set_target_properties(minigzip_cmd PROPERTIES OUTPUT_NAME minigzip)
        target_compile_definitions(minigzip_cmd PRIVATE ${STDLIB_DEF} ${MINIZIP_DEF})
        target_include_directories(minigzip_cmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
        target_link_libraries(minigzip_cmd ${MINIZIP_TARGET})

        if(NOT SKIP_INSTALL_BINARIES AND NOT SKIP_INSTALL_ALL)
            install(TARGETS minigzip_cmd RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        endif()
    endif()

    add_executable(minizip_cmd minizip.c)
    set_target_properties(minizip_cmd PROPERTIES OUTPUT_NAME ${MINIZIP_TARGET})
    target_compile_definitions(minizip_cmd PRIVATE ${STDLIB_DEF} ${MINIZIP_DEF})
    target_include_directories(minizip_cmd PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(minizip_cmd ${MINIZIP_TARGET})
    if(WIN32)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT minizip_cmd)
    endif()

    if(NOT SKIP_INSTALL_BINARIES AND NOT SKIP_INSTALL_ALL)
        install(TARGETS minizip_cmd RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    endif()
endif()

if(MZ_BUILD_TESTS AND MZ_BUILD_UNIT_TESTS)
    enable_testing()

    add_subdirectory(test)

    # Can't disable zlib testing so ctest tries to run zlib example app
    if(MZ_ZLIB AND NOT MZ_LIBCOMP AND NOT ZLIB_FOUND)
        if(TARGET example)
            target_include_directories(example PRIVATE ${ZLIB_SOURCE_DIR})
            add_dependencies(${MINIZIP_TARGET} example)
        endif()
        if(TARGET example64)
            target_include_directories(example64 PRIVATE ${ZLIB_SOURCE_DIR})
            add_dependencies(${MINIZIP_TARGET} example64)
        endif()
    endif()

    set(TEST_TEMP_DIR ${CMAKE_CURRENT_BINARY_DIR}/Testing/Temporary)

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

            set(COMPRESS_METHOD_DEST_DIR
                ${TEST_TEMP_DIR}/${COMPRESS_METHOD_NAME}-${EXTRA_NAME})
            set(COMPRESS_METHOD_PATH
                ${TEST_TEMP_DIR}/${COMPRESS_METHOD_NAME}-${EXTRA_NAME}.zip)

            add_test(NAME ${COMPRESS_METHOD_NAME}-zip-${EXTRA_NAME}
                     COMMAND minizip_cmd ${COMPRESS_METHOD_ARG} -o ${EXTRA_ARGS}
                        ${COMPRESS_METHOD_PATH}
                        test.c test.h empty.txt random.bin uniform.bin fuzz
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            add_test(NAME ${COMPRESS_METHOD_NAME}-list-${EXTRA_NAME}
                     COMMAND minizip_cmd -l ${EXTRA_ARGS} ${COMPRESS_METHOD_PATH}
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            if(NOT MZ_COMPRESS_ONLY)
                add_test(NAME ${COMPRESS_METHOD_NAME}-unzip-${EXTRA_NAME}
                         COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                            -d ${COMPRESS_METHOD_DEST_DIR} ${COMPRESS_METHOD_PATH}
                         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            endif()
            add_test(NAME ${COMPRESS_METHOD_NAME}-append-${EXTRA_NAME}
                    COMMAND minizip_cmd ${COMPRESS_METHOD_ARG} -a ${EXTRA_ARGS}
                        ${COMPRESS_METHOD_PATH} single.txt
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            if(NOT MZ_COMPRESS_ONLY)
                add_test(NAME ${COMPRESS_METHOD_NAME}-append-unzip-${EXTRA_NAME}
                            COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                                -d ${COMPRESS_METHOD_DEST_DIR} ${COMPRESS_METHOD_PATH}
                            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            endif()
            add_test(NAME ${COMPRESS_METHOD_NAME}-erase-${EXTRA_NAME}
                    COMMAND minizip_cmd -o -e ${COMPRESS_METHOD_PATH} test.c test.h
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
            if(NOT MZ_COMPRESS_ONLY)
                add_test(NAME ${COMPRESS_METHOD_NAME}-erase-unzip-${EXTRA_NAME}
                         COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                            -d ${COMPRESS_METHOD_DEST_DIR} ${COMPRESS_METHOD_PATH}
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

    # Perform tests on others
    if(NOT MZ_COMPRESS_ONLY)
        if(MZ_ZLIB OR MZ_LIBCOMP)
            add_test(NAME unzip-tiny
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                        -d ${TEST_TEMP_DIR}/unzip-tiny
                        fuzz/unzip_fuzzer_seed_corpus/tiny.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_BZIP2)
            add_test(NAME unzip-bzip2
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                        -d ${TEST_TEMP_DIR}/unzip-bzip2
                        fuzz/unzip_fuzzer_seed_corpus/bzip2.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_LZMA)
            add_test(NAME unzip-lzma
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                        -d ${TEST_TEMP_DIR}/unzip-lzma
                        fuzz/unzip_fuzzer_seed_corpus/lzma.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_PKCRYPT)
            add_test(NAME unzip-pkcrypt
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                        -d ${TEST_TEMP_DIR}/unzip-pkcrypt -p test123
                        fuzz/unzip_fuzzer_seed_corpus/encrypted_pkcrypt.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
        if(MZ_WZAES)
            add_test(NAME unzip-wzaes
                     COMMAND minizip_cmd -x -o ${EXTRA_ARGS}
                        -d ${TEST_TEMP_DIR}/unzip-wzaes -p test123
                        fuzz/unzip_fuzzer_seed_corpus/encrypted_wzaes.zip
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/test)
        endif()
    endif()
    if(NOT MZ_COMPRESS_ONLY AND NOT MZ_DECOMPRESS_ONLY)
        if(MZ_ZLIB AND NOT MZ_LIBCOMP)
            file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/test/random.bin DESTINATION ${TEST_TEMP_DIR})
            add_test(NAME gz
                COMMAND minigzip_cmd random.bin
                WORKING_DIRECTORY ${TEST_TEMP_DIR})
            add_test(NAME ungz
                COMMAND minigzip_cmd -x -d ${TEST_TEMP_DIR} random.bin.gz
                WORKING_DIRECTORY ${TEST_TEMP_DIR})
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
        target_link_libraries(${target} ${MINIZIP_TARGET})

        if(FUZZING_ENGINE_FOUND)
            target_link_libraries(${target} ${FUZZING_ENGINE})
        endif()
    endmacro()

    configure_fuzz_test(zip_fuzzer)
    configure_fuzz_test(unzip_fuzzer)

    if(NOT SKIP_INSTALL_BINARIES AND NOT SKIP_INSTALL_ALL)
        install(TARGETS zip_fuzzer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
        install(TARGETS unzip_fuzzer RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
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
add_feature_info(MZ_LIBBSD MZ_LIBBSD "Builds with libbsd crypto random")
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
