project(Slippi
  VERSION 1.0.0)

set(SRCS
  SlippiGame.h
	SlippiGame.cpp
)
cmake_minimum_required(VERSION 3.14 FATAL_ERROR)

set(CMAKE_CXX_STANDARD 17)
add_definitions(-std=c++17)

if(NOT MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wno-unused-parameter")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
endif()

add_library(Slippi STATIC ${SRCS})

target_include_directories(Slippi PUBLIC "../nlohmann")

