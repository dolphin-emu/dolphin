check_and_add_flag(CXX11 -std=c++11)

set(SRCS OptionParser.cpp)

add_library(cpp-optparse STATIC ${SRCS})
