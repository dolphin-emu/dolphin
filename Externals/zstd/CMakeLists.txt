dolphin_fetchcontent_declare(
  zstd
  EXCLUDE_FROM_ALL
  GIT_REPOSITORY https://github.com/facebook/zstd.git
  GIT_TAG 711e17da98510a3567bf47f85a08a76f64811474
)

set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
set(ZSTD_BUILD_PROGRAMS OFF)
set(ZSTD_BUILD_SHARED OFF)

FetchContent_MakeAvailable(zstd)

add_library(zstd::zstd ALIAS libzstd_static)
