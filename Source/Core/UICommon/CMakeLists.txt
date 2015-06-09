set(SRCS Disassembler.cpp
         UICommon.cpp)

set(LIBS common)

add_dolphin_library(uicommon "${SRCS}" "${LIBS}")
