
# E.g. replace_cxx_flag("/W[0-4]", "/W4")
macro(replace_cxx_flag pattern text)
    foreach (flag
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)

        string(REGEX REPLACE "${pattern}" "${text}" ${flag} "${${flag}}")

    endforeach()
endmacro()

macro(append_cxx_flag text)
    foreach (flag
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)

        string(APPEND ${flag} " ${text}")

    endforeach()
endmacro()

# Fixup default compiler settings

# Be as strict as reasonably possible, since we want to support consumers using strict warning levels
replace_cxx_flag("/W[0-4]" "/W4")
append_cxx_flag("/WX")

# We want to be as conformant as possible, so tell MSVC to not be permissive (note that this has no effect on clang-cl)
append_cxx_flag("/permissive-")

# wistd::function has padding due to alignment. This is expected
append_cxx_flag("/wd4324")

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    # Ignore a few Clang warnings. We may want to revisit in the future to see if any of these can/should be removed
    append_cxx_flag("-Wno-switch")
    append_cxx_flag("-Wno-c++17-compat-mangling")
    append_cxx_flag("-Wno-missing-field-initializers")

    # For tests, we want to be able to test self assignment, so disable this warning
    append_cxx_flag("-Wno-self-assign-overloaded")
    append_cxx_flag("-Wno-self-move")

    # clang-cl does not understand the /permissive- flag (or at least it opts to ignore it). We can achieve similar
    # results through the following flags.
    append_cxx_flag("-fno-delayed-template-parsing")

    # NOTE: Windows headers not clean enough for us to realistically attempt to start fixing these errors yet. That
    # said, errors that originate from WIL headers may benefit
    # append_cxx_flag("-fno-ms-compatibility")
    # append_cxx_flag("-ferror-limit=999")
    # append_cxx_flag("-fmacro-backtrace-limit=0")
    # -fno-ms-compatibility turns off preprocessor compatability, which currently only works when __VA_OPT__ support is
    # available (i.e. >= C++20)
    # append_cxx_flag("-Xclang -std=c++2a")
else()
    # Flags that are either ignored or unrecognized by clang-cl
    # TODO: https://github.com/Microsoft/wil/issues/6
    # append_cxx_flag("/experimental:preprocessor")

    # CRT headers are not yet /experimental:preprocessor clean, so work around the known issues
    # append_cxx_flag("/Wv:18")

    append_cxx_flag("/bigobj")

    # NOTE: Temporary workaround while https://github.com/microsoft/wil/issues/102 is being investigated
    append_cxx_flag("/d2FH4-")
endif()
