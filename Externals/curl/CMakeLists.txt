set(BUILD_CURL_EXE OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_LIBCURL_DOCS OFF)
set(BUILD_MISC_DOCS OFF)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
set(BUILD_TESTING OFF)
set(CURL_ENABLE_EXPORT_TARGET OFF)

set(HTTP_ONLY ON)

set(CURL_USE_LIBPSL OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF)
set(USE_LIBIDN2 OFF)
set(USE_NGHTTP2 OFF)

if(UNIX)
  # We use mbedtls on Unix(-like) systems and Android.
  set(CURL_USE_OPENSSL OFF)

  # This is set if the dolphin_find_optional_system_library call from the main CMakeLists.txt
  # is able to find mbedtls on the system.
  if(MBEDTLS_FOUND)
    # We can just enable CURL_USE_MBEDTLS.
    set(CURL_USE_MBEDTLS ON)
  else()
    # HACK: Set some internal variables to pretend like mbedtls was found on the system.
    # We can't use CURL_USE_MBEDTLS with our copy from Externals, as that flag expects
    # mbedtls to be installed (the CMakeLists attempts to search for it with find_package).
    set(_ssl_enabled ON)
    set(USE_MBEDTLS ON)
    set(_curl_ca_bundle_supported TRUE)
  endif()
endif()

if(WIN32)
  set(CURL_USE_SCHANNEL ON)
endif()

if(ANDROID)
  set(CURL_CA_PATH "/system/etc/security/cacerts" CACHE STRING "")
endif()

add_subdirectory(curl)

if(UNIX AND NOT MBEDTLS_FOUND)
  # HACK: Manually link with the mbedtls libraries.
  target_link_libraries(libcurl_static PRIVATE
    MbedTLS::mbedtls
    MbedTLS::mbedx509)
endif()
