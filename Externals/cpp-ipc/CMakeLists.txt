add_subdirectory(cpp-ipc EXCLUDE_FROM_ALL)

dolphin_disable_warnings(ipc)

if (NOT MSVC)
  target_compile_options(ipc PRIVATE "-fexceptions")
endif ()

add_library(cpp-ipc::ipc ALIAS ipc)
