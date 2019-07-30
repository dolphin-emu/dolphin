add_library(updatercommon
  UI.h
  UpdaterCommon.cpp
  UpdaterCommon.h
)

target_link_libraries(updatercommon PRIVATE
  uicommon
  mbedtls
  z
  ed25519
  cpp-optparse
)