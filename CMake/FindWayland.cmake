# Finds the Wayland client library.
#
# This will define:
#
#   Wayland_FOUND   - True if Wayland is found
#   Wayland::Client - Imported target for libwayland-client
#
# libwayland-egl is loaded at runtime by the OpenGL backend, so only the client library is needed
# to build.

find_path(WAYLAND_CLIENT_INCLUDE_DIR NAMES wayland-client.h)
find_library(WAYLAND_CLIENT_LIBRARY NAMES wayland-client)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Wayland REQUIRED_VARS
  WAYLAND_CLIENT_LIBRARY WAYLAND_CLIENT_INCLUDE_DIR
)

if(Wayland_FOUND AND NOT TARGET Wayland::Client)
  add_library(Wayland::Client UNKNOWN IMPORTED)
  set_target_properties(Wayland::Client PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${WAYLAND_CLIENT_INCLUDE_DIR}"
    IMPORTED_LOCATION "${WAYLAND_CLIENT_LIBRARY}"
  )
endif()

mark_as_advanced(WAYLAND_CLIENT_INCLUDE_DIR WAYLAND_CLIENT_LIBRARY)
