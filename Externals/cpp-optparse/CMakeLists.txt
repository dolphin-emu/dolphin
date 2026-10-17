dolphin_fetchcontent_declare(
  cpp-optparse
  GIT_REPOSITORY https://github.com/weisslj/cpp-optparse.git
  GIT_TAG 2265d647232249a53a03b411099863ceca35f0d3
)
FetchContent_MakeAvailable(cpp-optparse)

set(SRCS
  ${cpp-optparse_SOURCE_DIR}/OptionParser.cpp
  ${cpp-optparse_SOURCE_DIR}/OptionParser.h
)

add_library(cpp-optparse STATIC ${SRCS})
target_compile_features(cpp-optparse PUBLIC cxx_std_11)
target_compile_options(cpp-optparse PRIVATE -fexceptions)

target_include_directories(cpp-optparse
PUBLIC
  ${cpp-optparse_SOURCE_DIR}
)
