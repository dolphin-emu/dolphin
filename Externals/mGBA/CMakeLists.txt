set(LIBMGBA_ONLY ON)
set(USE_LZMA ON)
add_subdirectory(mgba EXCLUDE_FROM_ALL)
dolphin_disable_warnings_msvc(mgba)

if(NOT MSVC)
  target_compile_options(mgba PRIVATE -Wno-unused-parameter -Wno-unused-result -Wno-unused-variable)
endif()

if(ANDROID)
  target_compile_definitions(mgba PRIVATE -Dfutimes=futimens)
endif()

add_library(mGBA::mgba ALIAS mgba)
