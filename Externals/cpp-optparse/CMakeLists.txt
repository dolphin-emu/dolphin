set(SRCS
    cpp-optparse/OptionParser.cpp
    cpp-optparse/OptionParser.h
)

add_library(cpp-optparse STATIC ${SRCS})
target_compile_features(cpp-optparse PUBLIC cxx_std_11)
target_compile_options(cpp-optparse PRIVATE -fexceptions)
dolphin_disable_warnings(cpp-optparse)

target_include_directories(cpp-optparse
PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/cpp-optparse
)
