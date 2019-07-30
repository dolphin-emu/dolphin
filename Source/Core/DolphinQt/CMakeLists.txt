if (NOT Qt5_DIR AND MSVC)
set(Qt5_DIR "${CMAKE_SOURCE_DIR}/Externals/Qt5.11.1/5.11.1/msvc2017_64/lib/cmake/Qt5")
endif()

find_package(Qt5 5.9 REQUIRED COMPONENTS Gui Widgets)

set_property(TARGET Qt5::Core PROPERTY INTERFACE_COMPILE_FEATURES "")
message(STATUS "Found Qt version ${Qt5Core_VERSION}")

set(CMAKE_AUTOMOC ON)

add_executable(dolphin-emu
  AboutDialog.cpp
  AboutDialog.h
  CheatsManager.cpp
  CheatsManager.h
  DiscordHandler.cpp
  DiscordHandler.h
  DiscordJoinRequestDialog.cpp
  DiscordJoinRequestDialog.h
  FIFO/FIFOPlayerWindow.cpp
  FIFO/FIFOPlayerWindow.h
  FIFO/FIFOAnalyzer.cpp
  FIFO/FIFOAnalyzer.h
  Host.cpp
  Host.h
  HotkeyScheduler.cpp
  HotkeyScheduler.h
  Main.cpp
  MainWindow.cpp
  MainWindow.h
  MenuBar.cpp
  MenuBar.h
  RenderWidget.cpp
  RenderWidget.h
  Resources.cpp
  Resources.h
  SearchBar.cpp
  SearchBar.h
  Settings.cpp
  Settings.h
  ToolBar.cpp
  ToolBar.h
  Translation.cpp
  Translation.h
  WiiUpdate.cpp
  WiiUpdate.h
  Config/ARCodeWidget.cpp
  Config/ARCodeWidget.h
  Config/CheatCodeEditor.cpp
  Config/CheatCodeEditor.h
  Config/CheatWarningWidget.cpp
  Config/CheatWarningWidget.h
  Config/ControllersWindow.cpp
  Config/ControllersWindow.h
  Config/FilesystemWidget.cpp
  Config/FilesystemWidget.h
  Config/GameConfigEdit.cpp
  Config/GameConfigEdit.h
  Config/GameConfigHighlighter.cpp
  Config/GameConfigHighlighter.h
  Config/GameConfigWidget.cpp
  Config/GameConfigWidget.h
  Config/GeckoCodeWidget.cpp
  Config/GeckoCodeWidget.h
  Config/Graphics/AdvancedWidget.cpp
  Config/Graphics/AdvancedWidget.h
  Config/Graphics/EnhancementsWidget.cpp
  Config/Graphics/EnhancementsWidget.h
  Config/Graphics/GeneralWidget.cpp
  Config/Graphics/GeneralWidget.h
  Config/Graphics/GraphicsBool.cpp
  Config/Graphics/GraphicsBool.h
  Config/Graphics/GraphicsChoice.cpp
  Config/Graphics/GraphicsChoice.h
  Config/Graphics/GraphicsInteger.cpp
  Config/Graphics/GraphicsInteger.h
  Config/Graphics/GraphicsRadio.cpp
  Config/Graphics/GraphicsRadio.h
  Config/Graphics/GraphicsSlider.cpp
  Config/Graphics/GraphicsSlider.h
  Config/Graphics/GraphicsWidget.cpp
  Config/Graphics/GraphicsWidget.h
  Config/Graphics/GraphicsWindow.cpp
  Config/Graphics/GraphicsWindow.h
  Config/Graphics/HacksWidget.cpp
  Config/Graphics/HacksWidget.h
  Config/Graphics/PostProcessingConfigWindow.cpp
  Config/Graphics/PostProcessingConfigWindow.h
  Config/Graphics/SoftwareRendererWidget.cpp
  Config/Graphics/SoftwareRendererWidget.h
  Config/InfoWidget.cpp
  Config/InfoWidget.h
  Config/LogConfigWidget.cpp
  Config/LogConfigWidget.h
  Config/LogWidget.cpp
  Config/LogWidget.h
  Config/Mapping/GCKeyboardEmu.cpp
  Config/Mapping/GCKeyboardEmu.h
  Config/Mapping/GCMicrophone.cpp
  Config/Mapping/GCMicrophone.h
  Config/Mapping/GCPadEmu.cpp
  Config/Mapping/GCPadEmu.h
  Config/Mapping/GCPadWiiUConfigDialog.cpp
  Config/Mapping/GCPadWiiUConfigDialog.h
  Config/Mapping/Hotkey3D.cpp
  Config/Mapping/Hotkey3D.h
  Config/Mapping/HotkeyControllerProfile.cpp
  Config/Mapping/HotkeyControllerProfile.h
  Config/Mapping/HotkeyDebugging.cpp
  Config/Mapping/HotkeyDebugging.h
  Config/Mapping/HotkeyGeneral.cpp
  Config/Mapping/HotkeyGeneral.h
  Config/Mapping/HotkeyGraphics.cpp
  Config/Mapping/HotkeyGraphics.h
  Config/Mapping/HotkeyStates.cpp
  Config/Mapping/HotkeyStates.h
  Config/Mapping/HotkeyStatesOther.cpp
  Config/Mapping/HotkeyStatesOther.h
  Config/Mapping/HotkeyTAS.cpp
  Config/Mapping/HotkeyTAS.h
  Config/Mapping/HotkeyWii.cpp
  Config/Mapping/HotkeyWii.h
  Config/Mapping/IOWindow.cpp
  Config/Mapping/MappingButton.cpp
  Config/Mapping/MappingButton.h
  Config/Mapping/MappingCommon.cpp
  Config/Mapping/MappingCommon.h
  Config/Mapping/MappingIndicator.cpp
  Config/Mapping/MappingIndicator.h
  Config/Mapping/MappingNumeric.cpp
  Config/Mapping/MappingNumeric.h
  Config/Mapping/MappingWidget.cpp
  Config/Mapping/MappingWidget.h
  Config/Mapping/MappingWindow.cpp
  Config/Mapping/MappingWindow.h
  Config/Mapping/WiimoteEmuExtension.cpp
  Config/Mapping/WiimoteEmuExtension.h
  Config/Mapping/WiimoteEmuGeneral.cpp
  Config/Mapping/WiimoteEmuGeneral.h
  Config/Mapping/WiimoteEmuMotionControl.cpp
  Config/Mapping/WiimoteEmuMotionControl.h
  Config/NewPatchDialog.cpp
  Config/NewPatchDialog.h
  Config/PatchesWidget.cpp
  Config/PatchesWidget.h
  Config/PropertiesDialog.cpp
  Config/PropertiesDialog.h
  Config/SettingsWindow.cpp
  Config/SettingsWindow.h
  Config/VerifyWidget.cpp
  Config/VerifyWidget.h
  Debugger/BreakpointWidget.cpp
  Debugger/BreakpointWidget.h
  Debugger/CodeViewWidget.cpp
  Debugger/CodeViewWidget.h
  Debugger/CodeWidget.cpp
  Debugger/CodeWidget.h
  Debugger/JITWidget.cpp
  Debugger/JITWidget.h
  Debugger/MemoryViewWidget.cpp
  Debugger/MemoryViewWidget.h
  Debugger/MemoryWidget.cpp
  Debugger/MemoryWidget.h
  Debugger/NewBreakpointDialog.cpp
  Debugger/NewBreakpointDialog.h
  Debugger/PatchInstructionDialog.cpp
  Debugger/PatchInstructionDialog.h
  Debugger/RegisterColumn.cpp
  Debugger/RegisterColumn.h
  Debugger/RegisterWidget.cpp
  Debugger/RegisterWidget.h
  Debugger/WatchWidget.cpp
  Debugger/WatchWidget.h
  GameList/GameList.cpp
  GameList/GameList.h
  GameList/GameListModel.cpp
  GameList/GameListModel.h
  GameList/GameTracker.cpp
  GameList/GameTracker.h
  GameList/GridProxyModel.cpp
  GameList/GridProxyModel.h
  GameList/ListProxyModel.cpp
  GameList/ListProxyModel.h
  GCMemcardManager.cpp
  GCMemcardManager.h
  QtUtils/BlockUserInputFilter.cpp
  QtUtils/BlockUserInputFilter.h
  NetPlay/ChunkedProgressDialog.cpp
  NetPlay/ChunkedProgressDialog.h
  NetPlay/GameListDialog.cpp
  NetPlay/GameListDialog.h
  NetPlay/MD5Dialog.cpp
  NetPlay/MD5Dialog.h
  NetPlay/NetPlayBrowser.cpp
  NetPlay/NetPlayBrowser.h
  NetPlay/NetPlayDialog.cpp
  NetPlay/NetPlayDialog.h
  NetPlay/NetPlaySetupDialog.cpp
  NetPlay/NetPlaySetupDialog.h
  NetPlay/PadMappingDialog.cpp
  NetPlay/PadMappingDialog.h
  QtUtils/DoubleClickEventFilter.cpp
  QtUtils/DoubleClickEventFilter.h
  QtUtils/ElidedButton.cpp
  QtUtils/ElidedButton.h
  QtUtils/FileOpenEventFilter.cpp
  QtUtils/FileOpenEventFilter.h
  QtUtils/FlowLayout.cpp
  QtUtils/FlowLayout.h
  QtUtils/ModalMessageBox.cpp
  QtUtils/ModalMessageBox.h
  QtUtils/ImageConverter.cpp
  QtUtils/ImageConverter.h
  QtUtils/WindowActivationEventFilter.cpp
  QtUtils/WindowActivationEventFilter.h
  QtUtils/WinIconHelper.cpp
  QtUtils/WinIconHelper.h
  QtUtils/WrapInScrollArea.cpp
  QtUtils/WrapInScrollArea.h
  QtUtils/AspectRatioWidget.cpp
  QtUtils/AspectRatioWidget.h
  ResourcePackManager.cpp
  ResourcePackManager.h
  Settings/AdvancedPane.cpp
  Settings/AdvancedPane.h
  Settings/AudioPane.cpp
  Settings/AudioPane.h
  Settings/GameCubePane.cpp
  Settings/GameCubePane.h
  Settings/GeneralPane.cpp
  Settings/GeneralPane.h
  Settings/InterfacePane.cpp
  Settings/InterfacePane.h
  Settings/PathPane.cpp
  Settings/PathPane.h
  Settings/WiiPane.cpp
  Settings/WiiPane.h
  Settings/USBDeviceAddToWhitelistDialog.cpp
  Settings/USBDeviceAddToWhitelistDialog.h
  TAS/GCTASInputWindow.cpp
  TAS/GCTASInputWindow.h
  TAS/WiiTASInputWindow.cpp
  TAS/WiiTASInputWindow.h
  TAS/TASCheckBox.cpp
  TAS/TASCheckBox.h
  TAS/TASInputWindow.cpp
  TAS/TASInputWindow.h
  TAS/StickWidget.cpp
  TAS/StickWidget.h
  TAS/IRWidget.cpp
  TAS/IRWidget.h
  Updater.cpp
  Updater.h
)

if (NOT WIN32)
  target_sources(dolphin-emu PRIVATE
    QtUtils/SignalDaemon.cpp
    QtUtils/SignalDaemon.h
  )
endif()

target_compile_definitions(dolphin-emu
PRIVATE
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_CAST_FROM_ASCII
  -DQT_NO_CAST_TO_ASCII
)

target_include_directories(dolphin-emu
PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
  ${Qt5Gui_PRIVATE_INCLUDE_DIRS}
)

target_link_libraries(dolphin-emu
PRIVATE
  core
  Qt5::Widgets
  uicommon
)

if(WIN32)
  target_sources(dolphin-emu PRIVATE DolphinQt.manifest DolphinQt.rc)

  set_target_properties(dolphin-emu PROPERTIES
    DEBUG_POSTFIX    D
    OUTPUT_NAME      Dolphin
    WIN32_EXECUTABLE TRUE
  )

  target_compile_options(dolphin-emu PRIVATE "-D_SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING")

  # Copy Sys dir
  add_custom_command(TARGET dolphin-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Data/Sys" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Sys"
  )

  # Copy qt.conf
  add_custom_command(TARGET dolphin-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf.win" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qt.conf"
  )

  # Delegate to Qt's official deployment binary on Windows to copy over the necessary Qt-specific libraries, etc.
  get_target_property(MOC_EXECUTABLE_LOCATION Qt5::moc IMPORTED_LOCATION)
  get_filename_component(QT_BINARY_DIRECTORY "${MOC_EXECUTABLE_LOCATION}" DIRECTORY)
  find_program(WINDEPLOYQT_EXE windeployqt HINTS "${QT_BINARY_DIRECTORY}")

  # Note: We set the PATH for the duration of this command so that the
  #       deployment application is able to locate the Qt libraries to copy.
  #       if the necessary paths aren't already set beforehand.
  #
  #       For example, consider a hypothetical emulation project named Orca.
  #       Orca supplies its own version of Qt instead of having developers actually
  #       install the officially supported Qt libraries -- a method that would make
  #       wrangling around with Qt through CMake much nicer and lessen the external
  #       library maintenance burden of the project, but alas.
  #
  #       In this case, as Qt is not installed through the official binary, this also
  #       means proper path variables will not be set up, thus the need to ensure they're
  #       always set up.
  #
  add_custom_command(TARGET dolphin-emu POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E env PATH="${QT_BINARY_DIRECTORY}"
            "${WINDEPLOYQT_EXE}" --libdir="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
                                 --plugindir="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/QtPlugins"
                                 $<IF:$<CONFIG:Debug>,--debug,--release>
                                 --no-translations
                                 --no-compiler-runtime
                                 "$<TARGET_FILE:dolphin-emu>"
  )
endif()

# Handle localization
find_package(Gettext)

if(WIN32 AND NOT Gettext_FOUND)
  message(STATUS "Using Gettext from Externals")
  set(GETTEXT_MSGFMT_EXECUTABLE "${CMAKE_SOURCE_DIR}/Externals/gettext/msgfmt.exe")
endif()

if(GETTEXT_MSGFMT_EXECUTABLE)
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

    if(WIN32)
      add_custom_command(OUTPUT ${mo}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${mo} ${po}
        COMMAND ${CMAKE_COMMAND} -E copy ${mo} ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Languages/${lang}/dolphin-emu.mo
        DEPENDS ${po}
      )
    else()
      add_custom_command(OUTPUT ${mo}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${mo_dir}
        COMMAND ${GETTEXT_MSGMERGE_EXECUTABLE} --quiet --update --backup=none -s ${po} ${pot_file}
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o ${mo} ${po}
        DEPENDS ${po}
      )
    endif()
  endforeach()
endif()

if(APPLE)
  include(BundleUtilities)
  set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Dolphin.app)

  # Ask for an application bundle.
  set_target_properties(dolphin-emu PROPERTIES
    MACOSX_BUNDLE true
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    OUTPUT_NAME Dolphin
    )

  # Copy qt.conf into the bundle
  target_sources(dolphin-emu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf")
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/qt.conf" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # Copy icon into the bundle
  target_sources(dolphin-emu PRIVATE "${CMAKE_SOURCE_DIR}/Data/Dolphin.icns")
  set_source_files_properties("${CMAKE_SOURCE_DIR}/Data/Dolphin.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # Copy Qt plugins into the bundle
  get_target_property(qtcocoa_location Qt5::QCocoaIntegrationPlugin LOCATION)
  target_sources(dolphin-emu PRIVATE "${qtcocoa_location}")
  set_source_files_properties("${qtcocoa_location}" PROPERTIES MACOSX_PACKAGE_LOCATION MacOS/platforms)

  get_target_property(qtmacstyle_location Qt5::QMacStylePlugin LOCATION)
  target_sources(dolphin-emu PRIVATE "${qtmacstyle_location}")
  set_source_files_properties("${qtmacstyle_location}" PROPERTIES MACOSX_PACKAGE_LOCATION MacOS/styles)

  # Copy resources into the bundle
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Data/Sys")
  file(GLOB_RECURSE resources RELATIVE "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Data/Sys/*")
  foreach(res ${resources})
    target_sources(dolphin-emu PRIVATE "${CMAKE_SOURCE_DIR}/Data/${res}")
    get_filename_component(resdir "${res}" DIRECTORY)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/Data/${res}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/${resdir}")
    source_group("Resources" FILES "${CMAKE_SOURCE_DIR}/Data/${res}")
  endforeach()

  # Copy MoltenVK into the bundle
  target_sources(dolphin-emu PRIVATE "${CMAKE_SOURCE_DIR}/Externals/MoltenVK/libvulkan.dylib")
  set_source_files_properties("${CMAKE_SOURCE_DIR}/Externals/MoltenVK/libvulkan.dylib" PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)

  # Update library references to make the bundle portable
  include(DolphinPostprocessBundle)
  dolphin_postprocess_bundle(dolphin-emu)
else()
  install(TARGETS dolphin-emu RUNTIME DESTINATION ${bindir})
endif()

if(USE_DISCORD_PRESENCE)
  target_compile_definitions(dolphin-emu PRIVATE -DUSE_DISCORD_PRESENCE)
endif()
