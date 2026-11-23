dolphin_fetchcontent_declare(
    lz4
    GIT_REPOSITORY https://github.com/lz4/lz4.git
    GIT_TAG aa4647fad575d01a874aaaadcdbe8e6a40e072f5 # v1.11.1
    GIT_SHALLOW TRUE
    SOURCE_SUBDIR build/cmake
)

set(LZ4_STATIC_MULTITHREAD ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(lz4)

add_library(LZ4::LZ4 ALIAS lz4_static)
