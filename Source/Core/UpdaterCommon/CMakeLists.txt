add_library(updatercommon
  UI.h
  UpdaterCommon.h
  UpdaterCommon.cpp
)
  
target_link_libraries(updatercommon PRIVATE
  uicommon
  mbedtls
  z
  ed25519
  cpp-optparse
)
  
set_target_properties(updatercommon PROPERTIES
  FOLDER "Updater"
  PROJECT_LABEL "UpdaterCommon"
)

include(SourceGroupByFolder)
source_group_by_folder(updatercommon)
