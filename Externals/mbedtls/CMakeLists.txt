set(ENABLE_TESTING OFF)
set(ENABLE_PROGRAMS OFF)
set(MBEDTLS_FATAL_WARNINGS OFF)
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL ON)

add_subdirectory(mbedtls)
dolphin_disable_warnings(mbedcrypto)
dolphin_disable_warnings(mbedtls)
dolphin_disable_warnings(mbedx509)