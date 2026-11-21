set(CPPWINRT_WINMD_REF "73" CACHE STRING
    "windows-rs git tag for Windows.winmd")
set(CPPWINRT_WINMD_SHA256
    "e74592f1e74a77058d041f9c018bd30ffd10628bc839b451742a19680823098a"
    CACHE STRING "Expected SHA256 of Windows.winmd")

set(CPPWINRT_WINMD_URL
    "https://raw.githubusercontent.com/microsoft/windows-rs/${CPPWINRT_WINMD_REF}/crates/libs/bindgen/default/Windows.winmd")

if(DEFINED ENV{XDG_CACHE_HOME} AND NOT "$ENV{XDG_CACHE_HOME}" STREQUAL "")
  set(_cppwinrt_cache_default "$ENV{XDG_CACHE_HOME}/dolphin-emu/cppwinrt")
elseif(WIN32 AND DEFINED ENV{LOCALAPPDATA} AND NOT "$ENV{LOCALAPPDATA}" STREQUAL "")
  set(_cppwinrt_cache_default "$ENV{LOCALAPPDATA}/dolphin-emu/cppwinrt")
elseif(DEFINED ENV{HOME} AND NOT "$ENV{HOME}" STREQUAL "")
  set(_cppwinrt_cache_default "$ENV{HOME}/.cache/dolphin-emu/cppwinrt")
else()
  message(WARNING "cppwinrt: could not determine a user cache directory "
    "(XDG_CACHE_HOME/LOCALAPPDATA/HOME all unset or empty); "
    "caching Windows.winmd inside the build directory instead.")
  set(_cppwinrt_cache_default "${CMAKE_BINARY_DIR}/_cppwinrt_cache")
endif()

set(CPPWINRT_WINMD_CACHE_DIR "${_cppwinrt_cache_default}" CACHE PATH
    "Directory used to cache the downloaded Windows.winmd")

set(CPPWINRT_WINMD "${CPPWINRT_WINMD_CACHE_DIR}/Windows-${CPPWINRT_WINMD_REF}.winmd")

if(NOT EXISTS "${CPPWINRT_WINMD}")
  message(STATUS "cppwinrt: downloading Windows.winmd (tag ${CPPWINRT_WINMD_REF}, cached at ${CPPWINRT_WINMD})")
  file(MAKE_DIRECTORY "${CPPWINRT_WINMD_CACHE_DIR}")

  file(DOWNLOAD
    "${CPPWINRT_WINMD_URL}"
    "${CPPWINRT_WINMD}"
    SHOW_PROGRESS
    STATUS _cppwinrt_winmd_dl_status
    TLS_VERIFY ON
    EXPECTED_HASH SHA256=${CPPWINRT_WINMD_SHA256}
  )

  list(GET _cppwinrt_winmd_dl_status 0 _cppwinrt_winmd_dl_code)
  if(NOT _cppwinrt_winmd_dl_code EQUAL 0)
    list(GET _cppwinrt_winmd_dl_status 1 _cppwinrt_winmd_dl_msg)
    file(REMOVE "${CPPWINRT_WINMD}")
    message(FATAL_ERROR "cppwinrt: failed to download Windows.winmd: ${_cppwinrt_winmd_dl_msg}\n"
      "URL: ${CPPWINRT_WINMD_URL}")
  endif()
else()
  message(STATUS "cppwinrt: using cached Windows.winmd at ${CPPWINRT_WINMD}")
endif()

# =====================================================================
# build cppwinrt
# =====================================================================
set(INSTALL_CPPWINRT_TOOL OFF CACHE BOOL "" FORCE)
set(INSTALL_WINDOWS_WINMD OFF CACHE BOOL "" FORCE)
set(CPPWINRT_BUILD_VERSION "3.0.260818.1" CACHE STRING "" FORCE)
add_subdirectory(cppwinrt EXCLUDE_FROM_ALL)

# =====================================================================
# generate headers
# =====================================================================
set(CPPWINRT_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

add_custom_command(
  OUTPUT "${CPPWINRT_GENERATED_DIR}/winrt/base.h"
  COMMAND cppwinrt -in "${CPPWINRT_WINMD}" -out "${CPPWINRT_GENERATED_DIR}" -verbose
  DEPENDS cppwinrt "${CPPWINRT_WINMD}"
  COMMENT "Generating C++/WinRT projection headers"
  VERBATIM
)
add_custom_target(cppwinrt_headers_gen
  DEPENDS "${CPPWINRT_GENERATED_DIR}/winrt/base.h"
)

add_library(cppwinrt_headers INTERFACE)
add_dependencies(cppwinrt_headers cppwinrt_headers_gen)
target_include_directories(cppwinrt_headers INTERFACE
  "${CPPWINRT_GENERATED_DIR}"
)
target_compile_options(cppwinrt_headers INTERFACE
  -fexceptions
)

add_library(CppWinRT::CppWinRT ALIAS cppwinrt_headers)
