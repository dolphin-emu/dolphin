set(MZ_COMPAT OFF)
set(MZ_BUILD_TESTS OFF)
set(MZ_BUILD_UNIT_TESTS OFF)
set(MZ_BUILD_FUZZ_TESTS OFF)
set(MZ_CODE_COVERAGE OFF)
set(SKIP_INSTALL_ALL ON)

if (MINGW OR MSVC)
  set(MZ_FETCH_LIBS OFF)
  set(MZ_FORCE_FETCH_LIBS OFF)
endif()

add_subdirectory(minizip-ng)
dolphin_disable_warnings(minizip-ng)
