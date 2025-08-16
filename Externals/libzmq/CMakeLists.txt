set(ENABLE_DRAFTS OFF)
set(BUILD_TESTS OFF)
set(WITH_DOCS OFF)
set(BUILD_SHARED OFF)
set(BUILD_STATIC ON)

add_subdirectory(libzmq)

dolphin_disable_warnings(objects)
dolphin_disable_warnings(libzmq-static)

if (NOT MSVC)
  target_compile_options(objects PRIVATE "-fexceptions")
  target_compile_options(libzmq-static PRIVATE "-fexceptions")
endif ()

add_library(libzmq::libzmq ALIAS libzmq-static)
