option(ENABLE_EXTRA "Enable a few rarely used encodings" OFF)
option(ENABLE_RELOCATABLE "Allow the package to run at any location in the file system" ON)

include(CheckIncludeFile)
check_include_file("stddef.h" HAVE_STDDEF_H)
check_include_file("stdlib.h" HAVE_STDLIB_H)
check_include_file("string.h" HAVE_STRING_H)


include(CheckFunctionExists)
check_function_exists(getc_unlocked HAVE_GETC_UNLOCKED)
check_function_exists(mbrtowc HAVE_MBRTOWC)
check_function_exists(setlocale HAVE_SETLOCALE)
check_function_exists(wcrtomb HAVE_WCRTOMB)

include(CheckCCompilerFlag)
check_c_compiler_flag("-fvisibility=hidden" HAVE_VISIBILITY)

include(CheckSymbolExists)
check_symbol_exists(nl_langinfo "langinfo.h" HAVE_LANGINFO_CODESET)
check_symbol_exists(iconv_open "iconv.h" HAVE_LANGINFO_CODESET)
check_symbol_exists(O_NOFOLLOW "fcntl.h" HAVE_WORKING_O_NOFOLLOW)

include(CheckTypeSize)
check_type_size(mbstate_t MBSTATE_T)
check_type_size(mode_t MODE_T)
check_type_size(ssize_t SSIZE_T)

if(NOT HAVE_MBSTATE_T)
    set(MBSTATE_T "int")
else()
    unset(MBSTATE_T)
endif()

if(NOT HAVE_MODE_T)
    set(MODE_T "int")
else()
    unset(MODE_T)
endif()

if(NOT HAVE_SSIZE_T)
    set(SSIZE_T "int")
else()
    unset(SSIZE_T)
endif()

include(TestBigEndian)
test_big_endian(IS_BIG_ENDIAN)
if(IS_BIG_ENDIAN)
    set(WORDS_LITTLEENDIAN 0)
else()
    set(WORDS_LITTLEENDIAN 1)
endif()

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/config.h
    @ONLY
)