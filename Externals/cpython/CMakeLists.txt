add_subdirectory(Python EXCLUDE_FROM_ALL)
dolphin_disable_warnings_msvc(Python)

target_compile_definitions(Python PUBLIC HAVE_CRC32)

if(NOT MSVC)
  target_compile_options(Python PRIVATE -Wno-unused-parameter -Wno-unused-result -Wno-unused-variable)
endif()

if(ANDROID)
  target_compile_definitions(Python PRIVATE -Dfutimes=futimens)
endif()

add_library(Python::Python ALIAS Python)