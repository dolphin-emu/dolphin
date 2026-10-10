dolphin_fetchcontent_declare(
  mgba
  GIT_REPOSITORY https://github.com/mgba-emu/mgba.git
  GIT_TAG 0b40863f64d0940f333fa1c638e75f86f8a26a33
)

set(LIBMGBA_ONLY ON)
set(USE_LZMA ON)

FetchContent_MakeAvailable(mgba)

target_compile_definitions(mgba PUBLIC HAVE_CRC32 ENABLE_VFS ENABLE_DIRECTORIES)
target_link_libraries(mgba ZLIB::ZLIB)

if(NOT MSVC)
  target_compile_options(mgba PRIVATE -Wno-unused-parameter -Wno-unused-result -Wno-unused-variable)
endif()

if(ANDROID)
  target_compile_definitions(mgba PUBLIC ENABLE_VFS_FD)
  target_compile_definitions(mgba PRIVATE -Dfutimes=futimens)
endif()

add_library(mGBA::mgba ALIAS mgba)
