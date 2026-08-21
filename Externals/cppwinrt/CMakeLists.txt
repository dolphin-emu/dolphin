set(INSTALL_CPPWINRT_TOOL OFF CACHE BOOL "" FORCE)
set(INSTALL_WINDOWS_WINMD OFF CACHE BOOL "" FORCE)

add_subdirectory(cppwinrt EXCLUDE_FROM_ALL)

add_library(cppwinrt_headers INTERFACE)

target_include_directories(cppwinrt_headers INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/cppwinrt
)

target_compile_options(cppwinrt_headers INTERFACE
  -fexceptions
)

add_library(CppWinRT::CppWinRT ALIAS cppwinrt_headers)
