if(NOT wxWidgets_FOUND)
  return()
endif()

set(SRCS
  AboutDolphin.cpp
  ControllerConfigDiag.cpp
  Cheats/ActionReplayCodesPanel.cpp
  Cheats/ARCodeAddEdit.cpp
  Cheats/CheatSearchTab.cpp
  Cheats/CheatsWindow.cpp
  Cheats/CreateCodeDialog.cpp
  Cheats/GeckoCodeDiag.cpp
  Config/AddUSBDeviceDiag.cpp
  Config/AdvancedConfigPane.cpp
  Config/AudioConfigPane.cpp
  Config/ConfigMain.cpp
  Config/GameCubeConfigPane.cpp
  Config/GCAdapterConfigDiag.cpp
  Config/GeneralConfigPane.cpp
  Config/InterfaceConfigPane.cpp
  Config/PathConfigPane.cpp
  Config/WiiConfigPane.cpp
  Debugger/AssemblerEntryDialog.cpp
  Debugger/BreakpointDlg.cpp
  Debugger/BreakpointView.cpp
  Debugger/BreakpointWindow.cpp
  Debugger/CodeView.cpp
  Debugger/CodeWindow.cpp
  Debugger/CodeWindowFunctions.cpp
  Debugger/DSPDebugWindow.cpp
  Debugger/DSPRegisterView.cpp
  Debugger/DebuggerPanel.cpp
  Debugger/DebuggerUIUtil.cpp
  Debugger/JitWindow.cpp
  Debugger/MemoryCheckDlg.cpp
  Debugger/MemoryView.cpp
  Debugger/MemoryWindow.cpp
  Debugger/RegisterView.cpp
  Debugger/RegisterWindow.cpp
  Debugger/WatchView.cpp
  Debugger/WatchWindow.cpp
  ISOProperties/FilesystemPanel.cpp
  ISOProperties/InfoPanel.cpp
  ISOProperties/ISOProperties.cpp
  NetPlay/ChangeGameDialog.cpp
  NetPlay/MD5Dialog.cpp
  NetPlay/NetPlayLauncher.cpp
  NetPlay/NetPlaySetupFrame.cpp
  NetPlay/NetWindow.cpp
  NetPlay/PadMapDialog.cpp
  Input/InputConfigDiag.cpp
  Input/InputConfigDiagBitmaps.cpp
  Input/HotkeyInputConfigDiag.cpp
  Input/GCPadInputConfigDiag.cpp
  Input/MicButtonConfigDiag.cpp
  Input/GCKeyboardInputConfigDiag.cpp
  Input/WiimoteInputConfigDiag.cpp
  Input/NunchukInputConfigDiag.cpp
  Input/ClassicInputConfigDiag.cpp
  Input/GuitarInputConfigDiag.cpp
  Input/DrumsInputConfigDiag.cpp
  Input/TurntableInputConfigDiag.cpp
  DolphinSlider.cpp
  FifoPlayerDlg.cpp
  Frame.cpp
  FrameAui.cpp
  FrameTools.cpp
  GameListCtrl.cpp
  ISOFile.cpp
  LogConfigWindow.cpp
  LogWindow.cpp
  Main.cpp
  MainMenuBar.cpp
  MainToolBar.cpp
  MemcardManager.cpp
  PatchAddEdit.cpp
  PostProcessingConfigDiag.cpp
  SoftwareVideoConfigDialog.cpp
  TASInputDlg.cpp
  UINeedsControllerState.cpp
  VideoConfigDiag.cpp
  WxEventUtils.cpp
  WxUtils.cpp
)

set(LIBS
  core
  uicommon
  cpp-optparse
  wxWidgets::wxWidgets
)

if(APPLE)
  # Add resource files to application bundle.
  set(RESOURCES resources/Dolphin.icns)
  list(APPEND SRCS ${RESOURCES})
  set_source_files_properties(${RESOURCES} PROPERTIES
    MACOSX_PACKAGE_LOCATION Resources)
endif()

if(WIN32)
  list(APPEND SRCS
    DolphinWX.manifest
    DolphinWX.rc
  )
endif()

add_executable(dolphin-emu ${SRCS})
target_link_libraries(dolphin-emu PRIVATE ${LIBS})

if(USE_X11)
  find_package(GTK2 REQUIRED)
  target_link_libraries(dolphin-emu PRIVATE ${GTK2_LIBRARIES})
  target_include_directories(dolphin-emu PRIVATE ${GTK2_INCLUDE_DIRS})
endif()

# Handle localization
find_package(Gettext)
if(GETTEXT_MSGMERGE_EXECUTABLE AND GETTEXT_MSGFMT_EXECUTABLE)
  set(pot_file "${CMAKE_SOURCE_DIR}/Languages/po/dolphin-emu.pot")
  file(GLOB LINGUAS ${CMAKE_SOURCE_DIR}/Languages/po/*.po)

  target_sources(dolphin-emu PRIVATE ${pot_file} ${LINGUAS})
  source_group("Localization" FILES ${LINGUAS})
  source_group("Localization\\\\Generated" FILES ${pot_file})

  foreach(po ${LINGUAS})
    get_filename_component(lang ${po} NAME_WE)
    set(mo_dir ${CMAKE_CURRENT_BINARY_DIR}/${lang})
    set(mo ${mo_dir}/dolphin-emu.mo)

    target_sources(dolphin-emu PRIVATE ${mo})
    source_group("Localization\\\\Generated" FILES ${mo})

    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
      set_source_files_properties(${mo} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/${lang}.lproj")
    else()
      install(FILES ${mo} DESTINATION share/locale/${lang}/LC_MESSAGES)
    endif()

    add_custom_command(OUTPUT ${mo}
      COMMAND mkdir -p ${mo_dir}
      COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} --quiet --update --backup=none -s ${po} ${pot_file}
      COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${mo} ${po}
      DEPENDS ${po}
    )
  endforeach()
endif()

if(APPLE)
  # Ask for an application bundle.
  set_target_properties(dolphin-emu PROPERTIES
    MACOSX_BUNDLE true
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    OUTPUT_NAME Dolphin
    )

  # Copy resources in the bundle
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Data/Sys")
  file(GLOB_RECURSE resources RELATIVE "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Data/Sys/*")
  foreach(res ${resources})
    target_sources(dolphin-emu PRIVATE "${CMAKE_SOURCE_DIR}/Data/${res}")
    get_filename_component(resdir "${res}" DIRECTORY)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/Data/${res}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/${resdir}")
    source_group("Resources" FILES "${CMAKE_SOURCE_DIR}/Data/${res}")
  endforeach()

  # Update library references to make the bundle portable
  include(DolphinPostprocessBundle)
  dolphin_postprocess_bundle(dolphin-emu)

  # Install bundle into systemwide /Applications directory.
  install(TARGETS dolphin-emu DESTINATION /Applications)
elseif(WIN32)
  set_target_properties(dolphin-emu PROPERTIES
    WIN32_EXECUTABLE ON
  )
  add_custom_command(TARGET dolphin-emu
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/Data/Sys $<TARGET_FILE_DIR:dolphin-emu>/Sys
  )
else()
  install(TARGETS dolphin-emu RUNTIME DESTINATION ${bindir})
endif()

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} dolphin-emu)
