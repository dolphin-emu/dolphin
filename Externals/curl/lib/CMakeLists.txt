set(LIB_NAME libcurl)

add_definitions(-DHAVE_CONFIG_H)
include_directories(.)

file(GLOB SRCS *.c vauth/*.c vtls/*.c)
add_library(
  curl
  STATIC
  ${SRCS}
  )

target_link_libraries(curl ${MBEDTLS_LIBRARIES})
