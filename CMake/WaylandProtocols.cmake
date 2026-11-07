# Generates C bindings for the Wayland protocol extensions Dolphin uses and exposes them as the
# static library "wayland_protocols". Sets WaylandProtocols_FOUND.
#
# Only the client side is generated. The protocol descriptions come from the system's
# wayland-protocols package, so no XML is vendored.

find_program(WAYLAND_SCANNER wayland-scanner)
pkg_check_modules(WAYLAND_PROTOCOLS wayland-protocols>=1.41)

set(WaylandProtocols_FOUND FALSE)
if(WAYLAND_SCANNER AND WAYLAND_PROTOCOLS_FOUND)
  pkg_get_variable(WAYLAND_PROTOCOLS_DIR wayland-protocols pkgdatadir)

  set(_wl_protocols
    staging/content-type/content-type-v1
    staging/pointer-warp/pointer-warp-v1
    unstable/pointer-constraints/pointer-constraints-unstable-v1
    unstable/relative-pointer/relative-pointer-unstable-v1
  )

  set(_wl_sources)
  foreach(_proto IN LISTS _wl_protocols)
    get_filename_component(_name "${_proto}" NAME)
    set(_xml "${WAYLAND_PROTOCOLS_DIR}/${_proto}.xml")
    set(_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols/${_name}-client-protocol.h")
    set(_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols/${_name}-protocol.c")
    add_custom_command(
      OUTPUT "${_header}" "${_code}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols"
      COMMAND ${WAYLAND_SCANNER} client-header "${_xml}" "${_header}"
      COMMAND ${WAYLAND_SCANNER} private-code "${_xml}" "${_code}"
      DEPENDS "${_xml}"
      VERBATIM
    )
    list(APPEND _wl_sources "${_header}" "${_code}")
  endforeach()

  add_library(wayland_protocols STATIC ${_wl_sources})
  target_include_directories(wayland_protocols PUBLIC
    "${CMAKE_CURRENT_BINARY_DIR}/wayland-protocols")
  target_link_libraries(wayland_protocols PUBLIC Wayland::Client)
  set_target_properties(wayland_protocols PROPERTIES POSITION_INDEPENDENT_CODE ON)

  set(WaylandProtocols_FOUND TRUE)
endif()
