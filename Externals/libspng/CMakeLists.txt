cmake_minimum_required(VERSION 3.0)

project(spng C)

add_library(spng STATIC libspng/spng/spng.c)
target_compile_definitions(spng PUBLIC SPNG_STATIC)
target_link_libraries(spng PUBLIC ZLIB::ZLIB)
target_include_directories(spng PUBLIC libspng/spng)
dolphin_disable_warnings(spng)
add_library(spng::spng ALIAS spng)
