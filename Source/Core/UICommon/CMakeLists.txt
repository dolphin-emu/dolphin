add_library(uicommon
  AutoUpdate.h
  AutoUpdate.cpp
  CommandLineParse.h
  CommandLineParse.cpp
  Disassembler.h
  Disassembler.cpp
  DiscordPresence.h
  DiscordPresence.cpp
  GameFile.h
  GameFile.cpp
  GameFileCache.h
  GameFileCache.cpp
  NetPlayIndex.h
  NetPlayIndex.cpp
  ResourcePack/Manager.h
  ResourcePack/Manager.cpp
  ResourcePack/Manifest.h
  ResourcePack/Manifest.cpp
  ResourcePack/ResourcePack.h
  ResourcePack/ResourcePack.cpp
  UICommon.h
  UICommon.cpp
  USBUtils.h
  USBUtils.cpp
  VideoUtils.h
  VideoUtils.cpp
)

target_link_libraries(uicommon
PUBLIC
  common
  cpp-optparse
  minizip

PRIVATE
  $<$<BOOL:APPLE>:${IOK_LIBRARY}>
)

set_target_properties(uicommon PROPERTIES
  FOLDER "Dolphin Core"
  PROJECT_LABEL "UICommon"
)

if ((DEFINED CMAKE_ANDROID_ARCH_ABI AND CMAKE_ANDROID_ARCH_ABI MATCHES "x86|x86_64") OR
    (NOT DEFINED CMAKE_ANDROID_ARCH_ABI AND _M_X86))
  target_link_libraries(uicommon PRIVATE bdisasm)
endif()

if(ENABLE_X11 AND X11_FOUND)
  target_include_directories(uicommon PRIVATE ${X11_INCLUDE_DIR})
  target_sources(uicommon PRIVATE
    X11Utils.h
    X11Utils.cpp
  )
  target_link_libraries(uicommon PUBLIC ${XRANDR_LIBRARIES})
endif()

include(SourceGroupByFolder)
source_group_by_folder(uicommon)

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
