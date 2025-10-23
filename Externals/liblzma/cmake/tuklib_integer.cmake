# SPDX-License-Identifier: 0BSD

#############################################################################
#
# tuklib_integer.cmake - see tuklib_integer.m4 for description and comments
#
# Author: Lasse Collin
#
#############################################################################

include("${CMAKE_CURRENT_LIST_DIR}/tuklib_common.cmake")
include(TestBigEndian)
include(CheckCSourceCompiles)
include(CheckIncludeFile)
include(CheckSymbolExists)

function(tuklib_integer TARGET_OR_ALL)
    # Check for endianness. Unlike the Autoconf's AC_C_BIGENDIAN, this doesn't
    # support Apple universal binaries. The CMake module will leave the
    # variable unset so we can catch that situation here instead of continuing
    # as if we were little endian.
    test_big_endian(WORDS_BIGENDIAN)
    if(NOT DEFINED WORDS_BIGENDIAN)
        message(FATAL_ERROR "Cannot determine endianness")
    endif()
    tuklib_add_definition_if("${TARGET_OR_ALL}" WORDS_BIGENDIAN)

    # Look for a byteswapping method.
    check_c_source_compiles("
            int main(void)
            {
                __builtin_bswap16(1);
                __builtin_bswap32(1);
                __builtin_bswap64(1);
                return 0;
            }
        "
        HAVE___BUILTIN_BSWAPXX)
    if(HAVE___BUILTIN_BSWAPXX)
        tuklib_add_definitions("${TARGET_OR_ALL}" HAVE___BUILTIN_BSWAPXX)
    else()
        check_include_file(byteswap.h HAVE_BYTESWAP_H)
        if(HAVE_BYTESWAP_H)
            tuklib_add_definitions("${TARGET_OR_ALL}" HAVE_BYTESWAP_H)
            check_symbol_exists(bswap_16 byteswap.h HAVE_BSWAP_16)
            tuklib_add_definition_if("${TARGET_OR_ALL}" HAVE_BSWAP_16)
            check_symbol_exists(bswap_32 byteswap.h HAVE_BSWAP_32)
            tuklib_add_definition_if("${TARGET_OR_ALL}" HAVE_BSWAP_32)
            check_symbol_exists(bswap_64 byteswap.h HAVE_BSWAP_64)
            tuklib_add_definition_if("${TARGET_OR_ALL}" HAVE_BSWAP_64)
        else()
            check_include_file(sys/endian.h HAVE_SYS_ENDIAN_H)
            if(HAVE_SYS_ENDIAN_H)
                tuklib_add_definitions("${TARGET_OR_ALL}" HAVE_SYS_ENDIAN_H)
            else()
                check_include_file(sys/byteorder.h HAVE_SYS_BYTEORDER_H)
                tuklib_add_definition_if("${TARGET_OR_ALL}"
                                         HAVE_SYS_BYTEORDER_H)
            endif()
        endif()
    endif()

    # Guess that unaligned access is fast on these archs:
    #   - 32/64-bit x86 / x86-64
    #   - 32/64-bit big endian PowerPC
    #   - 64-bit little endian PowerPC
    #   - Some 32-bit ARM
    #   - Some 64-bit ARM64 (AArch64)
    #   - Some 32/64-bit RISC-V
    #
    # CMake doesn't provide a standardized/normalized list of processor arch
    # names. For example, x86-64 may be "x86_64" (Linux), "AMD64" (Windows),
    # or even "EM64T" (64-bit WinXP).
    set(FAST_UNALIGNED_GUESS OFF)
    string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" PROCESSOR)

    # There is no ^ in the first regex branch to allow "i" at the beginning
    # so it can match "i386" to "i786", and "x86_64".
    if(PROCESSOR MATCHES "[x34567]86|^x64|^amd64|^em64t")
        set(FAST_UNALIGNED_GUESS ON)

    elseif(PROCESSOR MATCHES "^powerpc|^ppc")
        if(WORDS_BIGENDIAN OR PROCESSOR MATCHES "64")
            set(FAST_UNALIGNED_GUESS ON)
        endif()

    elseif(PROCESSOR MATCHES "^arm|^aarch64|^riscv")
        # On 32-bit and 64-bit ARM, GCC and Clang
        # #define __ARM_FEATURE_UNALIGNED if
        # unaligned access is supported.
        #
        # Exception: GCC at least up to 13.2.0
        # defines it even when using -mstrict-align
        # so in that case this autodetection goes wrong.
        # Most of the time -mstrict-align isn't used so it
        # shouldn't be a common problem in practice. See:
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=111555
        #
        # RISC-V C API Specification says that if
        # __riscv_misaligned_fast is defined then
        # unaligned access is known to be fast.
        #
        # MSVC is handled as a special case: We assume that
        # 32/64-bit ARM supports fast unaligned access.
        # If MSVC gets RISC-V support then this will assume
        # fast unaligned access on RISC-V too.
        check_c_source_compiles("
                #if !defined(__ARM_FEATURE_UNALIGNED) \
                        && !defined(__riscv_misaligned_fast) \
                        && !defined(_MSC_VER)
                compile error
                #endif
                int main(void) { return 0; }
            "
            TUKLIB_FAST_UNALIGNED_DEFINED_BY_PREPROCESSOR)
        if(TUKLIB_FAST_UNALIGNED_DEFINED_BY_PREPROCESSOR)
            set(FAST_UNALIGNED_GUESS ON)
        endif()
    endif()

    option(TUKLIB_FAST_UNALIGNED_ACCESS
           "Enable if the system supports *fast* unaligned memory access \
with 16-bit, 32-bit, and 64-bit integers."
           "${FAST_UNALIGNED_GUESS}")
    tuklib_add_definition_if("${TARGET_OR_ALL}" TUKLIB_FAST_UNALIGNED_ACCESS)

    # Unsafe type punning:
    option(TUKLIB_USE_UNSAFE_TYPE_PUNNING
           "This introduces strict aliasing violations and \
may result in broken code. However, this might improve performance \
in some cases, especially with old compilers \
(e.g. GCC 3 and early 4.x on x86, GCC < 6 on ARMv6 and ARMv7)."
           OFF)
    tuklib_add_definition_if("${TARGET_OR_ALL}" TUKLIB_USE_UNSAFE_TYPE_PUNNING)

    # Check for GCC/Clang __builtin_assume_aligned().
    check_c_source_compiles(
        "int main(void) { __builtin_assume_aligned(\"\", 1); return 0; }"
        HAVE___BUILTIN_ASSUME_ALIGNED)
    tuklib_add_definition_if("${TARGET_OR_ALL}" HAVE___BUILTIN_ASSUME_ALIGNED)
endfunction()
