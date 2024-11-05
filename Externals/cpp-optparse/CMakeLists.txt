check_and_add_flag(CXX11 -std=c++11)

set(SRCS OptionParser.cpp OptionParser.h)

add_library(cpp-optparse STATIC ${SRCS})
dolphin_disable_warnings(cpp-optparse)
target_include_directories(cpp-optparse PUBLIC .)
