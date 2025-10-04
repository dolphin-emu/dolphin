project(libcharset VERSION 1.5 LANGUAGES C)

option(ENABLE_RELOCATABLE "Allow the package to run at any location in the file system" ON)

include(CheckIncludeFile)
check_include_file("dlfcn.h" HAVE_DLFCN_H)
check_include_file("inttypes.h" HAVE_INTTYPES_H)
check_include_file("mach-o/dyld.h" HAVE_MACH_O_DYLD_H)
check_include_file("minix/config.h" HAVE_MINIX_CONFIG_H)
check_include_file("stdint.h" HAVE_STDINT_H)
check_include_file("stdio.h" HAVE_STDIO_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("strings.h" HAVE_STRINGS_H)
check_include_file("string.h" HAVE_STRING_H)
check_include_file("sys/stat.h" HAVE_SYS_STAT_H)
check_include_file("sys/types.h" HAVE_SYS_TYPES_H)
check_include_file("unistd.h" HAVE_UNISTD_H)
check_include_file("wchar.h" HAVE_WCHAR_H)

include(CheckFunctionExists)
check_function_exists(copy_file_range HAVE_COPY_FILE_RANGE)
check_function_exists(setlocale HAVE_SETLOCALE)
check_function_exists(symlink HAVE_SYMLINK)
check_function_exists(_NSGetExecutablePath HAVE__NSGETEXECUTABLEPATH)

include(CheckCCompilerFlag)
check_c_compiler_flag("-fvisibility=hidden" HAVE_VISIBILITY)

include(CheckSymbolExists)
check_symbol_exists(nl_langinfo "langinfo.h" HAVE_LANGINFO_CODESET)
check_symbol_exists(O_NOATIME "fcntl.h" HAVE_WORKING_O_NOATIME)
check_symbol_exists(O_NOFOLLOW "fcntl.h" HAVE_WORKING_O_NOFOLLOW)

set(STDC_HEADERS
    ${HAVE_STDINT_H}
    ${HAVE_STDIO_H}
    ${HAVE_STDLIB_H}
    ${HAVE_STRING_H}
    ${HAVE_STRINGS_H}
)

set(SYSTEM_DEFINES
    _ALL_SOURCE
    _DARWIN_C_SOURCE
    __EXTENSIONS__
    _GNU_SOURCE
    _HPUX_ALT_XOPEN_SOCKET_API
    _MINIX
    _NETBSD_SOURCE
    _OPENBSD_SOURCE
    _POSIX_SOURCE
    _POSIX_1_SOURCE
    _POSIX_PTHREAD_SEMANTICS
    __STDC_WANT_IEC_60559_ATTRIBS_EXT__
    __STDC_WANT_IEC_60559_BFP_EXT__
    __STDC_WANT_IEC_60559_DFP_EXT__
    __STDC_WANT_IEC_60559_EXT__
    __STDC_WANT_IEC_60559_FUNCS_EXT__
    __STDC_WANT_IEC_60559_TYPES_EXT__
    __STDC_WANT_LIB_EXT2__
    __STDC_WANT_MATH_SPEC_FUNCS__
    _TANDEM_SOURCE
    _XOPEN_SOURCE
)

foreach(def ${SYSTEM_DEFINES})
    add_compile_definitions(${def})
endforeach()

set(PACKAGE_NAME "${PROJECT_NAME}")
set(PACKAGE_STRING "${PROJECT_NAME} ${PROJECT_VERSION}")
set(PACKAGE_TARNAME "${PROJECT_NAME}")
set(PACKAGE_BUGREPORT "")
set(PACKAGE_URL "")
set(PACKAGE_VERSION "${PROJECT_VERSION}")
set(LT_OBJDIR ".libs")

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    @ONLY
)

add_library(libcharset STATIC
    lib/localcharset.c
)

target_include_directories(libcharset
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
)