dolphin_fetchcontent_declare(xxhash
  GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
  GIT_TAG c87183a77d67f7d37e3d2d1b7eaac5e7c695e4f0 # v0.8.4
  GIT_SHALLOW TRUE
  SOURCE_SUBDIR build/cmake
)

set(BUILD_SHARED_LIBS OFF)
set(XXHASH_BUILD_XXHSUM OFF)

FetchContent_MakeAvailable(xxhash)
