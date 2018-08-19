add_library(uicommon
  GameFile.cpp
  GameFileCache.cpp
  UICommon.cpp
  USBUtils.cpp
  VideoUtils.cpp
)

target_link_libraries(uicommon
PUBLIC
  common

PRIVATE
  bdisasm
  $<$<BOOL:APPLE>:${IOK_LIBRARY}>
)

if(NOT ANDROID)
  target_sources(uicommon PRIVATE CommandLineParse.cpp)
  target_sources(uicommon PRIVATE AutoUpdate.cpp)
  target_sources(uicommon PRIVATE DiscordPresence.cpp)
  target_sources(uicommon PRIVATE Disassembler.cpp)
  target_link_libraries(uicommon PUBLIC cpp-optparse)
endif()

if(USE_X11)
  target_sources(uicommon PRIVATE X11Utils.cpp)
endif()

if(LIBUSB_FOUND)
  target_link_libraries(uicommon PRIVATE ${LIBUSB_LIBRARIES})
endif()

if(ENABLE_LLVM)
  find_package(LLVM CONFIG QUIET)
  if(LLVM_FOUND AND TARGET LLVM)
    message(STATUS "LLVM found, enabling LLVM support in disassembler")
    target_compile_definitions(uicommon PRIVATE HAVE_LLVM)
    target_link_libraries(uicommon PRIVATE LLVM)
    target_include_directories(uicommon PRIVATE ${LLVM_INCLUDE_DIRS})
  endif()
endif()

if(USE_DISCORD_PRESENCE)
  target_compile_definitions(uicommon PRIVATE -DUSE_DISCORD_PRESENCE)
  target_link_libraries(uicommon PRIVATE discord-rpc)
endif()
