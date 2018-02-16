check_and_add_flag(CXX11 -std=c++11)

FIND_LIBRARY(METAL_LIBRARY Metal REQUIRED)
set(LIBS ${LIBS} ${METAL_LIBRARY})

set(SRCS mtlpp.mm)

add_library(mtlpp STATIC ${SRCS})
target_link_libraries(mtlpp PRIVATE ${METAL_LIBRARY})
target_include_directories(mtlpp PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
