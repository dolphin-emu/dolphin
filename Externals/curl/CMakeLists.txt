include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/curl/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/curl/lib)

set(CURL_LIBS MbedTLS::mbedtls zlibstatic)
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(use_core_foundation ON)

    find_library(SYSTEMCONFIGURATION_FRAMEWORK "SystemConfiguration")
    if(NOT SYSTEMCONFIGURATION_FRAMEWORK)
        message(FATAL_ERROR "SystemConfiguration framework not found")
    endif()

    list(APPEND CURL_LIBS "-framework SystemConfiguration")
endif()

file(GLOB SRCS curl/lib/*.c curl/lib/vauth/*.c curl/lib/vquic/*.c curl/lib/vssh/*.c curl/lib/vtls/*.c)
add_library(
        curl
        STATIC
        curl/include/curl/curl.h
        ${SRCS}
)

set(SEARCH_CA_BUNDLE_PATHS
        /etc/ssl/certs/ca-certificates.crt
        /etc/pki/tls/certs/ca-bundle.crt
        /usr/share/ssl/certs/ca-bundle.crt
        /usr/local/share/certs/ca-root-nss.crt
        /etc/ssl/cert.pem)

foreach(SEARCH_CA_BUNDLE_PATH ${SEARCH_CA_BUNDLE_PATHS})
    if(EXISTS "${SEARCH_CA_BUNDLE_PATH}")
        message(STATUS "Found CA bundle: ${SEARCH_CA_BUNDLE_PATH}")
        set(CURL_CA_BUNDLE "${SEARCH_CA_BUNDLE_PATH}")
        set(CURL_CA_BUNDLE_SET TRUE)
        break()
    endif()
endforeach()

if(NOT CURL_CA_PATH_SET)
    if(EXISTS "/etc/ssl/certs")
        set(CURL_CA_PATH "/etc/ssl/certs")
        set(CURL_CA_PATH_SET TRUE)
    endif()
endif()

dolphin_disable_warnings(curl)
target_link_libraries(curl ${CURL_LIBS})
target_include_directories(curl PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/curl/include")
target_compile_definitions(curl PRIVATE "BUILDING_LIBCURL=1")
if (WIN32)
    target_compile_definitions(curl PUBLIC "CURL_STATICLIB=1" "CURL_DISABLE_LDAP" "USE_WINDOWS_SSPI" "USE_SCHANNEL")
    target_link_libraries(curl Crypt32.lib)
else()
    target_compile_definitions(curl PUBLIC "CURL_STATICLIB=1" "USE_MBEDTLS=1" "HAVE_CONFIG_H=1" "CURL_DISABLE_LDAP")
    if (CURL_CA_PATH_SET)
        target_compile_definitions(curl PUBLIC CURL_CA_PATH="${CURL_CA_PATH}")
    endif()
endif()

add_library(CURL::libcurl ALIAS curl)
