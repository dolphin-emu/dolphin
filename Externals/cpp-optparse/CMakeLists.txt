check_and_add_flag(CXX11 -std=c++11)

set(SRCS OptionParser.cpp OptionParser.h)

add_library(cpp-optparse STATIC ${SRCS})
target_include_directories(cpp-optparse PUBLIC .)

set_target_properties(cpp-optparse PROPERTIES FOLDER Externals)
disable_all_warnings(cpp-optparse)
