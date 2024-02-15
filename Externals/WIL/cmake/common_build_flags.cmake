
# This is unfortunately still needed to disable exceptions/RTTI since modern CMake still has no builtin support...
# E.g. replace_cxx_flag("/EHsc", "/EHs-c-")
macro(replace_cxx_flag pattern text)
    foreach (flag
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)

        string(REGEX REPLACE "${pattern}" "${text}" ${flag} "${${flag}}")

    endforeach()
endmacro()

# Fixup default compiler settings
add_compile_options(
    # Be as strict as reasonably possible, since we want to support consumers using strict warning levels
    /W4 /WX
    )

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        # Ignore some pedantic warnings enabled by '-Wextra'
        -Wno-missing-field-initializers

        # Ignore some pedantic warnings enabled by '-Wpedantic'
        -Wno-language-extension-token
        -Wno-c++17-attribute-extensions
        -Wno-gnu-zero-variadic-macro-arguments
        -Wno-extra-semi

        # For tests, we want to be able to test self assignment, so disable this warning
        -Wno-self-assign-overloaded
        -Wno-self-move

        # clang needs this to enable _InterlockedCompareExchange128
        -mcx16

        # We don't want legacy MSVC conformance
        -fno-delayed-template-parsing

        # NOTE: Windows headers not clean enough for us to realistically attempt to start fixing these errors yet. That
        # said, errors that originate from WIL headers may benefit
        # -fno-ms-compatibility
        # -ferror-limit=999
        # -fmacro-backtrace-limit=0

        # -fno-ms-compatibility turns off preprocessor compatability, which currently only works when __VA_OPT__ support
        # is available (i.e. >= C++20)
        # -Xclang -std=c++2a
        )
else()
    add_compile_options(
        # We want to be as conformant as possible, so tell MSVC to not be permissive (note that this has no effect on clang-cl)
        /permissive-

        # wistd::function has padding due to alignment. This is expected
        /wd4324

        # TODO: https://github.com/Microsoft/wil/issues/6
        # /experimental:preprocessor

        # CRT headers are not yet /experimental:preprocessor clean, so work around the known issues
        # /Wv:18

        # Some tests have a LOT of template instantiations
        /bigobj

        # NOTE: Temporary workaround while https://github.com/microsoft/wil/issues/102 is being investigated
        /d2FH4-
        )
endif()
