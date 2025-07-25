add_library(uicommon
  AutoUpdate.cpp
  AutoUpdate.h
  CommandLineParse.cpp
  CommandLineParse.h
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
)

target_link_libraries(uicommon
PUBLIC
  common
  core
  cpp-optparse
  minizip-ng::minizip-ng
  pugixml

PRIVATE
  fmt::fmt
  $<$<BOOL:APPLE>:${IOK_LIBRARY}>
)

if ((DEFINED CMAKE_ANDROID_ARCH_ABI AND CMAKE_ANDROID_ARCH_ABI MATCHES "x86|x86_64") OR
    (NOT DEFINED CMAKE_ANDROID_ARCH_ABI AND _M_X86_64))
  target_link_libraries(uicommon PRIVATE bdisasm)
endif()

if(UNIX AND NOT APPLE AND NOT ANDROID AND ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS DBus)
  target_sources(uicommon PRIVATE DBusUtils.cpp)
  target_compile_definitions(uicommon PRIVATE -DHAVE_QTDBUS=1)
  target_link_libraries(uicommon PUBLIC Qt6::DBus)
endif()

if(X11_FOUND)
  target_sources(uicommon PRIVATE X11Utils.cpp)
  target_link_libraries(uicommon PUBLIC PkgConfig::XRANDR PkgConfig::X11)
endif()

if(TARGET LibUSB::LibUSB)
  target_link_libraries(uicommon PRIVATE LibUSB::LibUSB)
endif()

if(USE_DISCORD_PRESENCE)
  target_compile_definitions(uicommon PRIVATE -DUSE_DISCORD_PRESENCE)
  target_link_libraries(uicommon PRIVATE discord-rpc)
endif()

if(MSVC)
  # Add precompiled header
  target_link_libraries(uicommon PRIVATE use_pch)
endif()
