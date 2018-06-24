cmake_minimum_required (VERSION 3.2.0)
project (DiscordRPC)

include(GNUInstallDirs)

# format
file(GLOB_RECURSE ALL_SOURCE_FILES
    include/*.h
    src/*.cpp src/*.h src/*.c
)

# Set CLANG_FORMAT_SUFFIX if you are using custom clang-format, e.g. clang-format-5.0
find_program(CLANG_FORMAT_CMD clang-format${CLANG_FORMAT_SUFFIX})

if (CLANG_FORMAT_CMD)
    add_custom_target(
        clangformat
        COMMAND ${CLANG_FORMAT_CMD}
        -i -style=file -fallback-style=none
        ${ALL_SOURCE_FILES}
        DEPENDS
        ${ALL_SOURCE_FILES}
    )
endif(CLANG_FORMAT_CMD)

# add subdirs

add_subdirectory(src)
