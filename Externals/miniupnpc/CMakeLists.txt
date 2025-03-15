cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

project (miniupnpc
         VERSION 2.3.2
         DESCRIPTION "UPnP IGD client lightweight library"
         HOMEPAGE_URL https://miniupnp.tuxfamily.org/
         LANGUAGES C)

set (MINIUPNPC_API_VERSION 20)

option (UPNPC_BUILD_STATIC "Build static library" TRUE)
option (UPNPC_BUILD_SHARED "Build shared library" FALSE)
option (UPNPC_BUILD_TESTS "Build test executables" FALSE)
option (UPNPC_BUILD_SAMPLE "Build sample executables" FALSE)
option (NO_GETADDRINFO "Define NO_GETADDRINFO" FALSE)
option (UPNPC_NO_INSTALL "Disable installation" FALSE)

if (NOT UPNPC_BUILD_STATIC AND NOT UPNPC_BUILD_SHARED)
    message (FATAL "Both shared and static libraries are disabled!")
endif ()

include(GNUInstallDirs)

# Interface library for compile definitions, flags and option
add_library(miniupnpc-private INTERFACE)

if (NO_GETADDRINFO)
  target_compile_definitions(miniupnpc-private INTERFACE NO_GETADDRINFO)
endif ()

if (NOT WIN32)
  target_compile_definitions(miniupnpc-private INTERFACE
    MINIUPNPC_SET_SOCKET_TIMEOUT
    MINIUPNPC_GET_SRC_ADDR
    _BSD_SOURCE _DEFAULT_SOURCE)
  if (NOT APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES ".*BSD" AND NOT CMAKE_SYSTEM_NAME STREQUAL "SunOS")
    # add_definitions (-D_POSIX_C_SOURCE=200112L)
    target_compile_definitions(miniupnpc-private INTERFACE _XOPEN_SOURCE=600)
  endif ()
  if (CMAKE_SYSTEM_NAME STREQUAL "NetBSD")
    target_compile_definitions(miniupnpc-private INTERFACE _NETBSD_SOURCE)
  endif ()
else ()
  set (MINIUPNPC_TARGET_WINDOWS_VERSION "0x0501" CACHE STRING "Minimum target Windows version as hex string") # XP or higher for getnameinfo and friends
  if (MINIUPNPC_TARGET_WINDOWS_VERSION)
    target_compile_definitions(miniupnpc-private INTERFACE _WIN32_WINNT=${MINIUPNPC_TARGET_WINDOWS_VERSION})
  endif ()
endif ()

if (APPLE)
  target_compile_definitions(miniupnpc-private INTERFACE _DARWIN_C_SOURCE)
endif ()

# Set compiler specific build flags
if (CMAKE_COMPILER_IS_GNUCC AND NOT CMAKE_SYSTEM_NAME STREQUAL "AmigaOS")
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  target_compile_options(miniupnpc-private INTERFACE -Wall)
endif ()

# Suppress noise warnings
if (MSVC)
  target_compile_definitions(miniupnpc-private INTERFACE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

configure_file (${CMAKE_CURRENT_SOURCE_DIR}/miniupnpcstrings.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/miniupnpcstrings.h)
target_include_directories(miniupnpc-private INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)

set (MINIUPNPC_SOURCES
  src/igd_desc_parse.c
  src/miniupnpc.c
  src/minixml.c
  src/minisoap.c
  src/minissdpc.c
  src/miniwget.c
  src/upnpcommands.c
  src/upnpdev.c
  src/upnpreplyparse.c
  src/upnperrors.c
  src/connecthostport.c
  src/portlistingparse.c
  src/receivedata.c
  src/addr_is_reserved.c
  ${CMAKE_CURRENT_BINARY_DIR}/miniupnpcstrings.h
)

if (WIN32)
  target_link_libraries(miniupnpc-private INTERFACE ws2_32 iphlpapi)
elseif (CMAKE_SYSTEM_NAME STREQUAL "SunOS")
  target_link_libraries(miniupnpc-private INTERFACE socket nsl resolv)
  find_library (SOCKET_LIBRARY NAMES socket)
  find_library (NSL_LIBRARY NAMES nsl)
  find_library (RESOLV_LIBRARY NAMES resolv)
  set (LDLIBS ${SOCKET_LIBRARY} ${NSL_LIBRARY} ${RESOLV_LIBRARY} ${LDLIBS})
elseif (HAIKU)
  target_link_libraries(miniupnpc-private INTERFACE network)
  find_library (SOCKET_LIBRARY NAMES network)
  find_library (NSL_LIBRARY NAMES network)
  find_library (RESOLV_LIBRARY NAMES network)
  set (LDLIBS ${SOCKET_LIBRARY} ${NSL_LIBRARY} ${RESOLV_LIBRARY} ${LDLIBS})
endif ()


if (UPNPC_BUILD_STATIC)
  add_library (libminiupnpc-static STATIC ${MINIUPNPC_SOURCES})
  target_include_directories (libminiupnpc-static PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/miniupnpc>)
  if (NOT UPNPC_BUILD_SHARED)
    add_library (miniupnpc::miniupnpc ALIAS libminiupnpc-static)
  endif()
  set_target_properties (libminiupnpc-static PROPERTIES EXPORT_NAME miniupnpc)
  if (WIN32 AND NOT MINGW)
    set_target_properties (libminiupnpc-static PROPERTIES OUTPUT_NAME "libminiupnpc")
  else()
    set_target_properties (libminiupnpc-static PROPERTIES OUTPUT_NAME "miniupnpc")
  endif()
  target_link_libraries (libminiupnpc-static PRIVATE miniupnpc-private)
  target_include_directories(libminiupnpc-static INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_definitions(libminiupnpc-static PUBLIC MINIUPNP_STATICLIB)

  if (NOT UPNPC_NO_INSTALL)
    install (TARGETS miniupnpc-private EXPORT miniupnpc-private)

    install (EXPORT miniupnpc-private
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/miniupnpc"
      NAMESPACE miniupnpc::)

    install (TARGETS libminiupnpc-static
      EXPORT libminiupnpc-static
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install (EXPORT libminiupnpc-static
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/miniupnpc"
      NAMESPACE miniupnpc::)
  endif()

  if (UPNPC_BUILD_SAMPLE)
    add_executable (upnpc-static src/upnpc.c)
    target_link_libraries (upnpc-static PRIVATE libminiupnpc-static)
    target_include_directories(upnpc-static PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    add_executable (upnp-listdevices-static src/listdevices.c)
    target_link_libraries (upnp-listdevices-static PRIVATE libminiupnpc-static)
    target_include_directories(upnp-listdevices-static PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    if (NOT UPNPC_NO_INSTALL)
      install (TARGETS upnpc-static upnp-listdevices-static
               RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
  endif ()
endif ()

if (UPNPC_BUILD_SHARED)
  add_library (libminiupnpc-shared SHARED ${MINIUPNPC_SOURCES})
  target_include_directories (libminiupnpc-shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include/miniupnpc>)
  add_library (miniupnpc::miniupnpc ALIAS libminiupnpc-shared)
  set_target_properties (libminiupnpc-shared PROPERTIES EXPORT_NAME miniupnpc)
  set_target_properties (libminiupnpc-shared PROPERTIES OUTPUT_NAME "miniupnpc")
  set_target_properties (libminiupnpc-shared PROPERTIES VERSION ${PROJECT_VERSION})
  set_target_properties (libminiupnpc-shared PROPERTIES SOVERSION ${MINIUPNPC_API_VERSION})
  target_link_libraries (libminiupnpc-shared PRIVATE miniupnpc-private)
  target_compile_definitions(libminiupnpc-shared PRIVATE MINIUPNP_EXPORTS)

  target_include_directories(libminiupnpc-shared INTERFACE $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  if (WIN32)
    target_link_libraries(libminiupnpc-shared INTERFACE ws2_32 iphlpapi)
  endif()

  if (NOT UPNPC_NO_INSTALL)
    install (TARGETS libminiupnpc-shared
      EXPORT libminiupnpc-shared
      RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
      LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
      ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})

    install (EXPORT libminiupnpc-shared
      DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/miniupnpc"
      NAMESPACE miniupnpc::)
  endif()

  if (UPNPC_BUILD_SAMPLE)
    add_executable (upnpc-shared src/upnpc.c)
    target_link_libraries (upnpc-shared PRIVATE libminiupnpc-shared)
    target_include_directories(upnpc-shared PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    add_executable (upnp-listdevices-shared src/listdevices.c)
    target_link_libraries (upnp-listdevices-shared PRIVATE libminiupnpc-shared)
    target_include_directories(upnp-listdevices-shared PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

    if (NOT UPNPC_NO_INSTALL)
      install (TARGETS upnpc-shared upnp-listdevices-shared
               RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
  endif ()
endif ()

if (UPNPC_BUILD_TESTS)
  add_library(miniupnpc-tests INTERFACE)
  target_link_libraries(miniupnpc-tests INTERFACE miniupnpc-private)
  target_compile_definitions(miniupnpc-tests INTERFACE MINIUPNP_STATICLIB)

  add_executable (testminixml src/testminixml.c src/minixml.c src/igd_desc_parse.c)
  target_include_directories (testminixml PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_link_libraries (testminixml PRIVATE miniupnpc-tests)

  add_executable (minixmlvalid src/minixmlvalid.c src/minixml.c)
  target_link_libraries (minixmlvalid PRIVATE miniupnpc-tests)

  add_executable (testupnpreplyparse src/testupnpreplyparse.c
                                     src/minixml.c src/upnpreplyparse.c)
  target_include_directories (testupnpreplyparse PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_link_libraries (testupnpreplyparse PRIVATE miniupnpc-tests)

  add_executable (testigddescparse src/testigddescparse.c
                                   src/igd_desc_parse.c src/minixml.c src/miniupnpc.c src/miniwget.c src/minissdpc.c
                                   src/upnpcommands.c src/upnpreplyparse.c src/minisoap.c src/connecthostport.c
                                   src/portlistingparse.c src/receivedata.c src/addr_is_reserved.c
  )
  target_include_directories (testigddescparse PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_link_libraries (testigddescparse PRIVATE miniupnpc-tests)

  add_executable (testminiwget src/testminiwget.c
                               src/miniwget.c src/miniupnpc.c src/minisoap.c src/upnpcommands.c src/minissdpc.c
                               src/upnpreplyparse.c src/minixml.c src/igd_desc_parse.c src/connecthostport.c
                               src/portlistingparse.c src/receivedata.c src/addr_is_reserved.c
  )
  target_include_directories (testminiwget PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_link_libraries (testminiwget PRIVATE miniupnpc-tests)

  add_executable (testaddr_is_reserved src/testaddr_is_reserved.c
                                       src/addr_is_reserved.c
  )
  target_link_libraries (testaddr_is_reserved PRIVATE miniupnpc-tests)

  add_executable (testportlistingparse src/testportlistingparse.c
                                       src/minixml.c src/portlistingparse.c)
  target_include_directories (testportlistingparse PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
  target_link_libraries (testportlistingparse PRIVATE miniupnpc-tests)

  if (NOT WIN32)
    add_executable (minihttptestserver src/minihttptestserver.c)
  endif()

# set (UPNPC_INSTALL_TARGETS ${UPNPC_INSTALL_TARGETS} testminixml minixmlvalid testupnpreplyparse testigddescparse testminiwget)
  include(CTest)
  add_test(NAME validateminixml
           COMMAND minixmlvalid)
  add_test(NAME validateminiwget
           COMMAND ${CMAKE_SOURCE_DIR}/testminiwget.sh)
  if (NOT WIN32)
    set_property(TEST validateminiwget
                 PROPERTY ENVIRONMENT
                 TESTSERVER=${CMAKE_BINARY_DIR}/minihttptestserver
                 TESTMINIWGET=${CMAKE_BINARY_DIR}/testminiwget)
  endif()
  add_test(NAME validateupnpreplyparse
           COMMAND ${CMAKE_SOURCE_DIR}/testupnpreplyparse.sh
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
  set_property(TEST validateupnpreplyparse
               PROPERTY ENVIRONMENT
               TESTUPNPREPLYPARSE=${CMAKE_BINARY_DIR}/testupnpreplyparse)
  add_test(NAME validateportlistingparse
           COMMAND testportlistingparse)
  add_test(NAME validateigddescparse1
           COMMAND testigddescparse new_LiveBox_desc.xml new_LiveBox_desc.values
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/testdesc)
  add_test(NAME validateigddescparse2
           COMMAND testigddescparse linksys_WAG200G_desc.xml linksys_WAG200G_desc.values
           WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/testdesc)
  add_test(NAME validateaddr_is_reserved
           COMMAND testaddr_is_reserved)

endif ()

configure_file(miniupnpc.pc.in miniupnpc.pc @ONLY)

if (NOT UPNPC_NO_INSTALL)
  install (FILES
    include/miniupnpc.h
    include/miniwget.h
    include/upnpcommands.h
    include/igd_desc_parse.h
    include/upnpreplyparse.h
    include/upnperrors.h
    include/upnpdev.h
    include/miniupnpctypes.h
    include/portlistingparse.h
    include/miniupnpc_declspec.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/miniupnpc
  )

  install(FILES miniupnpc-config.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/miniupnpc
  )

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/miniupnpc.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
  )

  install(FILES man3/miniupnpc.3
    DESTINATION ${CMAKE_INSTALL_MANDIR}/man3
  )

  install(FILES external-ip.sh
    TYPE BIN
  )
endif()

# vim: ts=2:sw=2:expandtab
