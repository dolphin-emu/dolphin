include(FindPackageHandleStandardArgs)

find_path(SteamworksSDK_STEAM_HEADER_DIR steam_api.h
  HINTS ${SteamworksSDK_DIR}/public/steam
)

# Set the include directory to one level up from the steam_api.h file, so include statements
# have a "steam/" prefix for Steamworks SDK headers.
set(SteamworksSDK_INCLUDE_DIR "${SteamworksSDK_STEAM_HEADER_DIR}/..")

if(WIN32 AND _ARCH_64)
  set(SteamworksSDK_LIBRARY_DIR "${SteamworksSDK_DIR}/redistributable_bin/win64")
elseif(APPLE)
  set(SteamworksSDK_LIBRARY_DIR "${SteamworksSDK_DIR}/redistributable_bin/osx")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  if(_ARCH_64)
    set(SteamworksSDK_LIBRARY_DIR "${SteamworksSDK_DIR}/redistributable_bin/linux32")
  else()
    set(SteamworksSDK_LIBRARY_DIR "${SteamworksSDK_DIR}/redistributable_bin/linux64")
  endif()
else()
  message(FATAL_ERROR "Unsupported platform for SteamworksSDK")
endif()

find_library(SteamworksSDK_LIBRARY steam_api steam_api64
  HINTS ${SteamworksSDK_LIBRARY_DIR}
)

add_library(SteamworksSDK::SteamworksSDK SHARED IMPORTED)

set_target_properties(SteamworksSDK::SteamworksSDK PROPERTIES
  IMPORTED_LOCATION "${SteamworksSDK_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${SteamworksSDK_INCLUDE_DIR}"
)

find_package_handle_standard_args(SteamworksSDK DEFAULT_MSG SteamworksSDK_LIBRARY SteamworksSDK_INCLUDE_DIR)
