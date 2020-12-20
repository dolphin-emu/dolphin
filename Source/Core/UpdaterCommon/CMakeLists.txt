add_library(updatercommon
  UI.h
  UpdaterCommon.cpp
  UpdaterCommon.h
)

target_link_libraries(updatercommon PRIVATE
  uicommon
  mbedtls
  ZLIB::ZLIB
  ed25519
  cpp-optparse
)