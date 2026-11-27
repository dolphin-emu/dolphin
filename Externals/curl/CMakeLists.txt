dolphin_fetchcontent_declare(
  curl
  GIT_REPOSITORY https://github.com/curl/curl.git
  GIT_TAG 3163ec065643ba20709f190111f42c3e1fe95273 # curl-8_23_0
  GIT_SHALLOW TRUE
)

set(BUILD_CURL_EXE OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_LIBCURL_DOCS OFF)
set(BUILD_MISC_DOCS OFF)
set(BUILD_SHARED_LIBS OFF)
set(BUILD_STATIC_LIBS ON)
set(BUILD_TESTING OFF)
set(CURL_ENABLE_EXPORT_TARGET OFF)
set(ENABLE_CURL_MANUAL OFF)

# Only HTTP(S) is used (HTTP_ONLY was removed in cURL 8.23).
set(CURL_DISABLE_DICT ON)
set(CURL_DISABLE_FILE ON)
set(CURL_DISABLE_FTP ON)
set(CURL_DISABLE_GOPHER ON)
set(CURL_DISABLE_IMAP ON)
set(CURL_DISABLE_IPFS ON)
set(CURL_DISABLE_LDAP ON)
set(CURL_DISABLE_LDAPS ON)
set(CURL_DISABLE_MQTT ON)
set(CURL_DISABLE_POP3 ON)
set(CURL_DISABLE_RTSP ON)
set(CURL_DISABLE_SMTP ON)
set(CURL_DISABLE_TELNET ON)
set(CURL_DISABLE_TFTP ON)

set(CURL_USE_LIBPSL OFF)
set(CURL_USE_LIBSSH2 OFF)
set(CURL_ZLIB OFF CACHE BOOL "" FORCE)
set(CURL_ZSTD OFF CACHE BOOL "" FORCE)
set(CURL_BROTLI OFF CACHE BOOL "" FORCE)
set(USE_LIBIDN2 OFF)
set(USE_NGHTTP2 OFF)

if(UNIX)
  # We use wolfSSL (Externals/wolfssl) on Unix(-like) systems and Android.
  set(CURL_USE_OPENSSL OFF)

  # HACK: Set some internal variables to pretend like wolfSSL was found on the system.
  # We can't use CURL_USE_WOLFSSL with our copy from Externals, as that flag expects
  # wolfSSL to be installed (the CMakeLists attempts to search for it with find_package).
  set(_ssl_enabled ON)
  set(USE_WOLFSSL ON)
  set(_curl_ca_bundle_supported TRUE)

  # Results of the checks CURL would run against an installed wolfSSL, which cannot link with a
  # library that is not built yet. They must match Externals/wolfssl/user_settings.h.
  set(HAVE_WOLFSSL_GET_PEER_CERTIFICATE ON)
  set(HAVE_WOLFSSL_USEALPN ON)
  set(HAVE_WOLFSSL_BIO_NEW ON)
  # wolfSSL_BIO_set_shutdown() needs OPENSSL_ALL.
  set(HAVE_WOLFSSL_BIO_SET_SHUTDOWN OFF)
  # No DES (NO_DES3), hence no NTLM.
  set(HAVE_WC_DES_ECBENCRYPT OFF)

  # Use the system CA store at run time.
  if(APPLE)
    set(CURL_CA_BUNDLE "/etc/ssl/cert.pem" CACHE STRING "")
  else()
    set(CURL_CA_NATIVE ON)
  endif()
endif()

if(WIN32)
  set(CURL_USE_SCHANNEL ON)
endif()

FetchContent_MakeAvailable(curl)

if(UNIX)
  # HACK: Manually link with the wolfSSL library.
  target_link_libraries(libcurl_static PRIVATE wolfssl)
endif()
