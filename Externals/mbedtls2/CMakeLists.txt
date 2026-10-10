# Our custom config renames symbols from mbedtls_ -> mbedtls2_
set(MBEDTLS_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/custom_config.h")

# Prevent conflicts in cmake target names.
set(MBEDTLS_TARGET_PREFIX mbedtls2_)

# The main CMakeLists in mbedtls "requires" a deprecated version of cmake.
# Luckily a usable CMakeLists is included their library directory.
# Note: Alternatively, just compiling every *.c in library/ also works.
set(MBEDTLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mbedtls")
add_subdirectory("${MBEDTLS_DIR}/library")

dolphin_disable_warnings("${MBEDTLS_TARGET_PREFIX}mbedtls")
dolphin_disable_warnings("${MBEDTLS_TARGET_PREFIX}mbedx509")
dolphin_disable_warnings("${MBEDTLS_TARGET_PREFIX}mbedcrypto")

add_library(mbedtls2::mbedtls ALIAS "${MBEDTLS_TARGET_PREFIX}mbedtls")
