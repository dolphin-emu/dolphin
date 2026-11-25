add_library(updatercommon
  UI.h
  UpdaterCommon.cpp
  UpdaterCommon.h
)

target_link_libraries(updatercommon PRIVATE
  uicommon
  MbedTLS::mbedtls
  ZLIB::ZLIB
  ed25519
  cpp-optparse
)

dolphin_shared_pch(updatercommon)
