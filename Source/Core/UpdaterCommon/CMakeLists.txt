add_library(updatercommon
  UpdaterCommon.cpp)
  
target_link_libraries(updatercommon PRIVATE
  uicommon
  mbedtls
  z
  ed25519
  cpp-optparse)