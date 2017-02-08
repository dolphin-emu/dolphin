set(SRCS Disassembler.cpp
         UICommon.cpp
         USBUtils.cpp)

set(LIBS common)
if(LIBUSB_FOUND)
  set(LIBS ${LIBS} ${LIBUSB_LIBRARIES})
endif()

add_dolphin_library(uicommon "${SRCS}" "${LIBS}")

if(ENABLE_LLVM)
  find_package(LLVM CONFIG QUIET)
  if(LLVM_FOUND)
    message(STATUS "LLVM found, enabling LLVM support in disassembler")
    target_compile_definitions(uicommon PRIVATE HAVE_LLVM)
    target_link_libraries(uicommon PRIVATE ${LLVM_AVAILABLE_LIBS})
    target_include_directories(uicommon PRIVATE ${LLVM_INCLUDE_DIRS})
  endif()
endif()
