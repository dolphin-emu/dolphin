set(SRCS Disassembler.cpp
         UICommon.cpp
         USBUtils.cpp)

set(LIBS common)
if(LIBUSB_FOUND)
  set(LIBS ${LIBS} ${LIBUSB_LIBRARIES})
endif()

add_dolphin_library(uicommon "${SRCS}" "${LIBS}")
