add_library(uicommon
  AutoUpdate.cpp
  AutoUpdate.h
  CommandLineParse.cpp
  CommandLineParse.h
  Disassembler.cpp
  Disassembler.h
  DiscordPresence.cpp
  DiscordPresence.h
  GameFile.cpp
  GameFile.h
  GameFileCache.cpp
  GameFileCache.h
  NetPlayIndex.cpp
  NetPlayIndex.h
  ResourcePack/Manager.cpp
  ResourcePack/Manager.h
  ResourcePack/Manifest.cpp
  ResourcePack/Manifest.h
  ResourcePack/ResourcePack.cpp
  ResourcePack/ResourcePack.h
  UICommon.cpp
  UICommon.h
  USBUtils.cpp
  USBUtils.h
)

target_link_libraries(uicommon
PUBLIC
  common
  cpp-optparse
  minizip::minizip
  pugixml

PRIVATE
  fmt::fmt
  $<$<BOOL:APPLE>:${IOK_LIBRARY}>
)

if ((DEFINED CMAKE_ANDROID_ARCH_ABI AND CMAKE_ANDROID_ARCH_ABI MATCHES "x86|x86_64") OR
    (NOT DEFINED CMAKE_ANDROID_ARCH_ABI AND _M_X86_64))
  target_link_libraries(uicommon PRIVATE bdisasm)
endif()

if(ENABLE_X11 AND X11_FOUND)
  target_include_directories(uicommon PRIVATE ${X11_INCLUDE_DIR})
  target_sources(uicommon PRIVATE X11Utils.cpp)
  target_link_libraries(uicommon PUBLIC ${XRANDR_LIBRARIES})
endif()

if(TARGET LibUSB::LibUSB)
  target_link_libraries(uicommon PRIVATE LibUSB::LibUSB)
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

if(MSVC)
  # Add precompiled header
  target_link_libraries(uicommon PRIVATE use_pch)
endif()
