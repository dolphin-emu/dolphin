add_subdirectory(cpp-ipc EXCLUDE_FROM_ALL)

dolphin_disable_warnings(ipc)

if (NOT MSVC)
  target_compile_options(ipc PRIVATE "-fexceptions")
endif ()

if (MINGW)
  execute_process(
    COMMAND patch --forward -p1 -i ${CMAKE_SOURCE_DIR}/Externals/cpp-ipc/patches/0001-windows-h-mingw.patch
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Externals/cpp-ipc/cpp-ipc
    RESULT_VARIABLE PATCH_RESULT
  )
  execute_process(
    COMMAND patch --forward -p1 -i ${CMAKE_SOURCE_DIR}/Externals/cpp-ipc/patches/0002-windows-h-mingw-extra.patch
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Externals/cpp-ipc/cpp-ipc
    RESULT_VARIABLE PATCH_RESULT
  )
endif()

add_library(cpp-ipc::ipc ALIAS ipc)
