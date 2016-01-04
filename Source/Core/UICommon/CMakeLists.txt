set(SRCS Disassembler.cpp
         DebugInterface.cpp
         UICommon.cpp)

set(LIBS common)

add_dolphin_library(uicommon "${SRCS}" "${LIBS}")
