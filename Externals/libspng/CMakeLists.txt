cmake_minimum_required(VERSION 3.0)

project(spng C)

add_library(spng STATIC ${CMAKE_CURRENT_LIST_DIR}/libspng/spng/spng.c)
target_compile_definitions(spng PUBLIC SPNG_STATIC)
target_link_libraries(spng PUBLIC ZLIB::ZLIB)
target_include_directories(spng PUBLIC ${CMAKE_CURRENT_LIST_DIR}/libspng/spng/)
dolphin_disable_warnings_msvc(spng)
