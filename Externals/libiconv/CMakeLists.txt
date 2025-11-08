project(libiconv VERSION 1.18 LANGUAGES C)
add_subdirectory(lib)
add_subdirectory(libcharset)

add_library(iconv STATIC
    lib/iconv.c
)

target_link_libraries(iconv PUBLIC libcharset)
target_include_directories(iconv
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
         $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/lib>
)

dolphin_disable_warnings(iconv)
add_library(Iconv::Iconv ALIAS iconv)
