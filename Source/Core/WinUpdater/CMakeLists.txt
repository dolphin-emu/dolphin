set (MANIFEST_FILE Updater.exe.manifest)

set(SOURCES
  Main.cpp
  WinUI.cpp
  ${MANIFEST_FILE}
)

add_executable(WinUpdater WIN32 ${SOURCES})

target_link_libraries(WinUpdater PRIVATE
  uicommon
  updatercommon
  Comctl32
)

set_target_properties(WinUpdater PROPERTIES
  FOLDER "Updater"
  OUTPUT_NAME "Updater"
)
