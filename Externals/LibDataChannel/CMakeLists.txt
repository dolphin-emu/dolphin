# LibDataChannel can use OpenSSL, GnuTLS, or MbedTLS 3.x
# We Fetch MbedTLS if neither OpenSSL/GnuTLS are found.
set(USE_OPENSSL OFF)
set(USE_GNUTLS OFF)
set(USE_MBEDTLS OFF)

# libsrtp can use OpenSSL, MbedTLS, NSS, or some internal code.
# It seems if OpenSSL is used in LibDataChannel then it must also be libsrtp.
# But if OpenSSL is not used in LibDataChannel then libsrtp is free to use anything.
# Possibly there's some limitation in libsrtp's internal crypto implementation ?
# Also, I feel like this setup should be the responsibility of LibDataChannel ?
set(LIBSRTP_TEST_APPS OFF)
set(ENABLE_OPENSSL OFF)
set(ENABLE_MBEDTLS OFF)
set(OPENSSL OFF)
set(MBEDTLS OFF)
set(GCM OFF)

if(NOT OpenSSL_FOUND)
  find_package(OpenSSL)
endif()
if(OpenSSL_FOUND)
  message(STATUS "Using OpenSSL for LibDataChannel")
  set(USE_OPENSSL ON)
  # libsrtp
  set(ENABLE_OPENSSL ON)
  set(OPENSSL ON)
  set(GCM ON)
else()
  if(NOT GnuTLS_FOUND)
    find_package(GnuTLS)
  endif()
  if(GnuTLS_FOUND)
    message(STATUS "Using GnuTLS for LibDataChannel")
    set(USE_GNUTLS ON)
  else()
    message(STATUS "Fetching MbedTLS for LibDataChannel")
    set(USE_MBEDTLS ON)
    # libsrtp
    set(ENABLE_MBEDTLS ON)
    set(MBEDTLS ON)
    set(GCM ON)

    FetchContent_Declare(
      MbedTLS
      GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
      # v3.6.7
      GIT_TAG 068ff080b369adfac81509f9b57b2afabaf82dc5
      GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(MbedTLS)

    # A required feature for LibDataChannel.
    target_compile_definitions(mbedtls PUBLIC MBEDTLS_SSL_DTLS_SRTP)

    # This satisfies target checks in LibDataChannel's CMakeLists.
    add_library(MbedTLS::MbedTLS INTERFACE IMPORTED)
    target_link_libraries(MbedTLS::MbedTLS INTERFACE MbedTLS::mbedtls)
  endif()
endif()

set(BUILD_SHARED_LIBS OFF)

# The LibDataChannel repo pulls these dependencies in regardless so just use them.
set(PREFER_SYSTEM_LIB OFF)
set(USE_SYSTEM_SRTP OFF)
set(USE_SYSTEM_JUICE OFF)
set(USE_SYSTEM_USRSCTP OFF)
set(USE_SYSTEM_PLOG OFF)
set(USE_SYSTEM_JSON OFF)

# LibDataChannel features:
set(NO_WEBSOCKET ON)
set(NO_MEDIA OFF)
set(NO_EXAMPLES ON)
set(NO_TESTS ON)

# Some jank to satisfy a find_package(MbedTLS REQUIRED) in libsrtp.
if(ENABLE_MBEDTLS)
  # We have a no-op FindMbedTLS.cmake here.
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
endif()

# Fetch LibDataChannel.
FetchContent_Declare(
  libdatachannel
  GIT_REPOSITORY https://github.com/paullouisageneau/libdatachannel.git
  # v0.24.6
  GIT_TAG 6b1e2e620f1e37f0eafeee702eaea0043cb305fd
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libdatachannel)

# More jank to connect libsrtp to our above Fetched MbedTLS.
if(ENABLE_MBEDTLS)
  target_link_libraries(srtp2 MbedTLS::MbedTLS)
endif()

if(MSVC)
  # Enable RTTI since it is disabled globally.
  target_compile_options(datachannel PRIVATE /GR)
else()
  # Enable exceptions since they are disabled globally.
  target_compile_options(datachannel PRIVATE -fexceptions)
endif()
