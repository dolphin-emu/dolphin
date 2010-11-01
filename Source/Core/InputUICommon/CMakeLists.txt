set(SRCS	Src/WXInputBase.cpp)

add_library(inputuicommon ${SRCS})
target_link_libraries(inputuicommon inputcommon)
