add_subdirectory(Lua EXCLUDE_FROM_ALL)
dolphin_disable_warnings_msvc(Lua)

target_compile_definitions(Lua PUBLIC HAVE_CRC32)

if(NOT MSVC)
  target_compile_options(Lua PRIVATE -Wno-unused-parameter -Wno-unused-result -Wno-unused-variable)
endif()

if(ANDROID)
  target_compile_definitions(Lua PRIVATE -Dfutimes=futimens)
endif()

add_library(Lua::Lua ALIAS Lua)