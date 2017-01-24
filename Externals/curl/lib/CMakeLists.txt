if(NOT CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DHAVE_CONFIG_H)
endif()

include_directories(.)

file(GLOB SRCS *.c vauth/*.c vtls/*.c)
add_library(
  curl
  STATIC
  ${SRCS}
  )

target_link_libraries(curl ${MBEDTLS_LIBRARIES})
target_compile_definitions(curl PUBLIC CURL_STATICLIB PRIVATE CURL_DISABLE_LDAP)
