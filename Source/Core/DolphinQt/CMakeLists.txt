if(POLICY CMP0084)
  # Disable trying to search for Qt3/4 if what we actually want is not found
  cmake_policy(SET CMP0084 NEW)
endif()

if (MSVC)
  if(_M_ARM_64)
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/Externals/Qt/Qt6.5.1/ARM64")
  else()
    list(APPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/Externals/Qt/Qt6.5.1/x64")
  endif()
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Svg)
message(STATUS "Found Qt version ${Qt6_VERSION}")

set_property(TARGET Qt6::Core PROPERTY INTERFACE_COMPILE_FEATURES "")

add_executable(dolphin-emu
  AboutDialog.cpp
  AboutDialog.h
  CheatSearchFactoryWidget.cpp
  CheatSearchFactoryWidget.h
  CheatSearchWidget.cpp
  CheatSearchWidget.h
  CheatsManager.cpp
  CheatsManager.h
  Achievements/AchievementBox.cpp
  Achievements/AchievementBox.h
  Achievements/AchievementHeaderWidget.cpp
  Achievements/AchievementHeaderWidget.h
  Achievements/AchievementLeaderboardWidget.cpp
  Achievements/AchievementLeaderboardWidget.h
  Achievements/AchievementProgressWidget.cpp
  Achievements/AchievementProgressWidget.h
  Achievements/AchievementSettingsWidget.cpp
  Achievements/AchievementSettingsWidget.h
  Achievements/AchievementsWindow.cpp
  Achievements/AchievementsWindow.h
  Config/ARCodeWidget.cpp
  Config/ARCodeWidget.h
  Config/CheatCodeEditor.cpp
  Config/CheatCodeEditor.h
  Config/CheatWarningWidget.cpp
  Config/CheatWarningWidget.h
  Config/CommonControllersWidget.cpp
  Config/CommonControllersWidget.h
  Config/ConfigControls/ConfigBool.cpp
  Config/ConfigControls/ConfigBool.h
  Config/ConfigControls/ConfigChoice.cpp
  Config/ConfigControls/ConfigChoice.h
  Config/ConfigControls/ConfigInteger.cpp
  Config/ConfigControls/ConfigInteger.h
  Config/ConfigControls/ConfigRadio.cpp
  Config/ConfigControls/ConfigRadio.h
  Config/ConfigControls/ConfigFloatSlider.cpp
  Config/ConfigControls/ConfigFloatSlider.h
  Config/ConfigControls/ConfigSlider.cpp
  Config/ConfigControls/ConfigSlider.h
  Config/ControllerInterface/ControllerInterfaceWindow.cpp
  Config/ControllerInterface/ControllerInterfaceWindow.h
  Config/ControllerInterface/DualShockUDPClientAddServerDialog.cpp
  Config/ControllerInterface/DualShockUDPClientAddServerDialog.h
  Config/ControllerInterface/DualShockUDPClientWidget.cpp
  Config/ControllerInterface/DualShockUDPClientWidget.h
  Config/ControllerInterface/ServerStringValidator.cpp
  Config/ControllerInterface/ServerStringValidator.h
  Config/ControllersWindow.cpp
  Config/ControllersWindow.h
  Config/FilesystemWidget.cpp
  Config/FilesystemWidget.h
  Config/FreeLookWidget.cpp
  Config/FreeLookWidget.h
  Config/FreeLookWindow.cpp
  Config/FreeLookWindow.h
  Config/GameConfigEdit.cpp
  Config/GameConfigEdit.h
  Config/GameConfigHighlighter.cpp
  Config/GameConfigHighlighter.h
  Config/GameConfigWidget.cpp
  Config/GameConfigWidget.h
  Config/GamecubeControllersWidget.cpp
  Config/GamecubeControllersWidget.h
  Config/GeckoCodeWidget.cpp
  Config/GeckoCodeWidget.h
  Config/Graphics/AdvancedWidget.cpp
  Config/Graphics/AdvancedWidget.h
  Config/Graphics/EnhancementsWidget.cpp
  Config/Graphics/EnhancementsWidget.h
  Config/Graphics/GeneralWidget.cpp
  Config/Graphics/GeneralWidget.h
  Config/Graphics/GraphicsWindow.cpp
  Config/Graphics/GraphicsWindow.h
  Config/Graphics/HacksWidget.cpp
  Config/Graphics/HacksWidget.h
  Config/Graphics/ColorCorrectionConfigWindow.cpp
  Config/Graphics/ColorCorrectionConfigWindow.h
  Config/Graphics/PostProcessingConfigWindow.cpp
  Config/Graphics/PostProcessingConfigWindow.h
  Config/GraphicsModListWidget.cpp
  Config/GraphicsModListWidget.h
  Config/GraphicsModWarningWidget.cpp
  Config/GraphicsModWarningWidget.h
  Config/HardcoreWarningWidget.cpp
  Config/HardcoreWarningWidget.h
  Config/InfoWidget.cpp
  Config/InfoWidget.h
  Config/LogConfigWidget.cpp
  Config/LogConfigWidget.h
  Config/LogWidget.cpp
  Config/LogWidget.h
  Config/Mapping/FreeLookGeneral.cpp
  Config/Mapping/FreeLookGeneral.h
  Config/Mapping/FreeLookRotation.cpp
  Config/Mapping/FreeLookRotation.h
  Config/Mapping/GBAPadEmu.cpp
  Config/Mapping/GBAPadEmu.h
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
  Config/Mapping/HotkeyGBA.cpp
  Config/Mapping/HotkeyGBA.h
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
  Config/Mapping/HotkeyUSBEmu.cpp
  Config/Mapping/HotkeyUSBEmu.h
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
  Config/Mapping/WiimoteEmuExtensionMotionInput.cpp
  Config/Mapping/WiimoteEmuExtensionMotionInput.h
  Config/Mapping/WiimoteEmuExtensionMotionSimulation.cpp
  Config/Mapping/WiimoteEmuExtensionMotionSimulation.h
  Config/Mapping/WiimoteEmuGeneral.cpp
  Config/Mapping/WiimoteEmuGeneral.h
  Config/Mapping/WiimoteEmuMotionControl.cpp
  Config/Mapping/WiimoteEmuMotionControl.h
  Config/Mapping/WiimoteEmuMotionControlIMU.cpp
  Config/Mapping/WiimoteEmuMotionControlIMU.h
  Config/NewPatchDialog.cpp
  Config/NewPatchDialog.h
  Config/PatchesWidget.cpp
  Config/PatchesWidget.h
  Config/PropertiesDialog.cpp
  Config/PropertiesDialog.h
  Config/SettingsWindow.cpp
  Config/SettingsWindow.h
  Config/ToolTipControls/BalloonTip.cpp
  Config/ToolTipControls/BalloonTip.h
  Config/ToolTipControls/ToolTipCheckBox.cpp
  Config/ToolTipControls/ToolTipCheckBox.h
  Config/ToolTipControls/ToolTipComboBox.cpp
  Config/ToolTipControls/ToolTipComboBox.h
  Config/ToolTipControls/ToolTipPushButton.cpp
  Config/ToolTipControls/ToolTipPushButton.h
  Config/ToolTipControls/ToolTipRadioButton.cpp
  Config/ToolTipControls/ToolTipRadioButton.h
  Config/ToolTipControls/ToolTipSlider.cpp
  Config/ToolTipControls/ToolTipSlider.h
  Config/ToolTipControls/ToolTipSpinBox.cpp
  Config/ToolTipControls/ToolTipSpinBox.h
  Config/ToolTipControls/ToolTipWidget.h
  Config/VerifyWidget.cpp
  Config/VerifyWidget.h
  Config/WiimoteControllersWidget.cpp
  Config/WiimoteControllersWidget.h
  ConvertDialog.cpp
  ConvertDialog.h
  Debugger/AssembleInstructionDialog.cpp
  Debugger/AssembleInstructionDialog.h
  Debugger/AssemblerWidget.cpp
  Debugger/AssemblerWidget.h
  Debugger/AssemblyEditor.cpp
  Debugger/AssemblyEditor.h
  Debugger/BranchWatchDialog.cpp
  Debugger/BranchWatchDialog.h
  Debugger/BranchWatchTableModel.cpp
  Debugger/BranchWatchTableModel.h
  Debugger/BreakpointDialog.cpp
  Debugger/BreakpointDialog.h
  Debugger/BreakpointWidget.cpp
  Debugger/BreakpointWidget.h
  Debugger/CodeViewWidget.cpp
  Debugger/CodeViewWidget.h
  Debugger/CodeWidget.cpp
  Debugger/CodeWidget.h
  Debugger/GekkoSyntaxHighlight.cpp
  Debugger/GekkoSyntaxHighlight.h
  Debugger/JITWidget.cpp
  Debugger/JITWidget.h
  Debugger/MemoryViewWidget.cpp
  Debugger/MemoryViewWidget.h
  Debugger/MemoryWidget.cpp
  Debugger/MemoryWidget.h
  Debugger/NetworkWidget.cpp
  Debugger/NetworkWidget.h
  Debugger/PatchInstructionDialog.cpp
  Debugger/PatchInstructionDialog.h
  Debugger/RegisterColumn.cpp
  Debugger/RegisterColumn.h
  Debugger/RegisterWidget.cpp
  Debugger/RegisterWidget.h
  Debugger/ThreadWidget.cpp
  Debugger/ThreadWidget.h
  Debugger/WatchWidget.cpp
  Debugger/WatchWidget.h
  DiscordHandler.cpp
  DiscordHandler.h
  DiscordJoinRequestDialog.cpp
  DiscordJoinRequestDialog.h
  FIFO/FIFOAnalyzer.cpp
  FIFO/FIFOAnalyzer.h
  FIFO/FIFOPlayerWindow.cpp
  FIFO/FIFOPlayerWindow.h
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
  GCMemcardCreateNewDialog.cpp
  GCMemcardCreateNewDialog.h
  GCMemcardManager.cpp
  GCMemcardManager.h
  Host.cpp
  Host.h
  HotkeyScheduler.cpp
  HotkeyScheduler.h
  InfinityBase/InfinityBaseWindow.cpp
  InfinityBase/InfinityBaseWindow.h
  Main.cpp
  MainWindow.cpp
  MainWindow.h
  MenuBar.cpp
  MenuBar.h
  NetPlay/ChunkedProgressDialog.cpp
  NetPlay/ChunkedProgressDialog.h
  NetPlay/GameDigestDialog.cpp
  NetPlay/GameDigestDialog.h
  NetPlay/GameListDialog.cpp
  NetPlay/GameListDialog.h
  NetPlay/NetPlayBrowser.cpp
  NetPlay/NetPlayBrowser.h
  NetPlay/NetPlayDialog.cpp
  NetPlay/NetPlayDialog.h
  NetPlay/NetPlaySetupDialog.cpp
  NetPlay/NetPlaySetupDialog.h
  NetPlay/PadMappingDialog.cpp
  NetPlay/PadMappingDialog.h
  NANDRepairDialog.cpp
  NANDRepairDialog.h
  NKitWarningDialog.cpp
  NKitWarningDialog.h
  QtUtils/AspectRatioWidget.cpp
  QtUtils/AspectRatioWidget.h
  QtUtils/BlockUserInputFilter.cpp
  QtUtils/BlockUserInputFilter.h
  QtUtils/ClearLayoutRecursively.cpp
  QtUtils/ClearLayoutRecursively.h
  QtUtils/DolphinFileDialog.cpp
  QtUtils/DolphinFileDialog.h
  QtUtils/DoubleClickEventFilter.cpp
  QtUtils/DoubleClickEventFilter.h
  QtUtils/ElidedButton.cpp
  QtUtils/ElidedButton.h
  QtUtils/FileOpenEventFilter.cpp
  QtUtils/FileOpenEventFilter.h
  QtUtils/FromStdString.h
  QtUtils/ImageConverter.cpp
  QtUtils/ImageConverter.h
  QtUtils/ModalMessageBox.cpp
  QtUtils/ModalMessageBox.h
  QtUtils/NonDefaultQPushButton.cpp
  QtUtils/NonDefaultQPushButton.h
  QtUtils/ParallelProgressDialog.h
  QtUtils/PartiallyClosableTabWidget.cpp
  QtUtils/PartiallyClosableTabWidget.h
  QtUtils/SetWindowDecorations.cpp
  QtUtils/SetWindowDecorations.h
  QtUtils/SignalBlocking.h
  QtUtils/UTF8CodePointCountValidator.cpp
  QtUtils/UTF8CodePointCountValidator.h
  QtUtils/ViewportLock.cpp
  QtUtils/ViewportLock.h
  QtUtils/WindowActivationEventFilter.cpp
  QtUtils/WindowActivationEventFilter.h
  QtUtils/WrapInScrollArea.cpp
  QtUtils/WrapInScrollArea.h
  RenderWidget.cpp
  RenderWidget.h
  ResourcePackManager.cpp
  ResourcePackManager.h
  Resources.cpp
  Resources.h
  RiivolutionBootWidget.cpp
  RiivolutionBootWidget.h
  SearchBar.cpp
  SearchBar.h
  Settings.cpp
  Settings.h
  Settings/AdvancedPane.cpp
  Settings/AdvancedPane.h
  Settings/AudioPane.cpp
  Settings/AudioPane.h
  Settings/BroadbandAdapterSettingsDialog.cpp
  Settings/BroadbandAdapterSettingsDialog.h
  Settings/GameCubePane.cpp
  Settings/GameCubePane.h
  Settings/GeneralPane.cpp
  Settings/GeneralPane.h
  Settings/InterfacePane.cpp
  Settings/InterfacePane.h
  Settings/PathPane.cpp
  Settings/PathPane.h
  Settings/USBDeviceAddToWhitelistDialog.cpp
  Settings/USBDeviceAddToWhitelistDialog.h
  Settings/WiiPane.cpp
  Settings/WiiPane.h
  SkylanderPortal/SkylanderModifyDialog.cpp
  SkylanderPortal/SkylanderModifyDialog.h
  SkylanderPortal/SkylanderPortalWindow.cpp
  SkylanderPortal/SkylanderPortalWindow.h
  TAS/GCTASInputWindow.cpp
  TAS/GCTASInputWindow.h
  TAS/GBATASInputWindow.cpp
  TAS/GBATASInputWindow.h
  TAS/IRWidget.cpp
  TAS/IRWidget.h
  TAS/StickWidget.cpp
  TAS/StickWidget.h
  TAS/TASCheckBox.cpp
  TAS/TASCheckBox.h
  TAS/TASControlState.cpp
  TAS/TASControlState.h
  TAS/TASInputWindow.cpp
  TAS/TASInputWindow.h
  TAS/TASSlider.cpp
  TAS/TASSlider.h
  TAS/TASSpinBox.cpp
  TAS/TASSpinBox.h
  TAS/WiiTASInputWindow.cpp
  TAS/WiiTASInputWindow.h
  ToolBar.cpp
  ToolBar.h
  Translation.cpp
  Translation.h
  Updater.cpp
  Updater.h
  WiiUpdate.cpp
  WiiUpdate.h
)

if (WIN32)
  target_sources(dolphin-emu PRIVATE
    Styles/Dark/dark.qrc
  )
endif()

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
  ${Qt6Gui_PRIVATE_INCLUDE_DIRS}
)

target_link_libraries(dolphin-emu
PRIVATE
  core
  Qt6::Widgets
  uicommon
  imgui
  implot
)

if (WIN32)
  target_link_libraries(dolphin-emu
  PRIVATE
    gdi32.lib
    shell32.lib
    dwmapi.lib # Needed to set window decorations for dark theme
  )
endif()

if (MSVC)
  set(qtGui "")
  set(qtGuiPriv "")
  set(qtWidgetsPriv "")
  list(TRANSFORM Qt6Gui_INCLUDE_DIRS PREPEND "/external:I" OUTPUT_VARIABLE qtGui)
  list(TRANSFORM Qt6Gui_PRIVATE_INCLUDE_DIRS PREPEND "/external:I" OUTPUT_VARIABLE qtGuiPriv)
  list(TRANSFORM Qt6Widgets_PRIVATE_INCLUDE_DIRS PREPEND "/external:I" OUTPUT_VARIABLE qtWidgetsPriv)
  target_compile_options(dolphin-emu PRIVATE "${qtGui}")
  target_compile_options(dolphin-emu PRIVATE "${qtGuiPriv}")
  target_compile_options(dolphin-emu PRIVATE "${qtWidgets}")

  # Qt 6.3.0 headers use std::aligned_storage instead of alignas
  target_compile_definitions(dolphin-emu PRIVATE _SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING)

  # Qt6 requires RTTI
  remove_cxx_flag_from_target(dolphin-emu "/GR-")
  target_compile_options(dolphin-emu PRIVATE "/GR")
endif()

if(WIN32)
  target_sources(dolphin-emu PRIVATE DolphinQt.manifest DolphinQt.rc)

  set_target_properties(dolphin-emu PROPERTIES
    DEBUG_POSTFIX    D
    OUTPUT_NAME      Dolphin
    WIN32_EXECUTABLE TRUE
  )

  # Copy Sys dir
  add_custom_command(TARGET dolphin-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Sys"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Data/Sys" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Sys"
  )

  # Copy COPYING
  add_custom_command(TARGET dolphin-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/COPYING" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/COPYING"
  )

  # Copy Licenses dir
  add_custom_command(TARGET dolphin-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/LICENSES" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Licenses"
  )

  # Copy qt.conf
  add_custom_command(TARGET dolphin-emu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf.win" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/qt.conf"
  )

  # Delegate to Qt's official deployment binary on Windows to copy over the necessary Qt-specific libraries, etc.
  get_target_property(MOC_EXECUTABLE_LOCATION Qt6::moc IMPORTED_LOCATION)
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
                                 --no-system-d3d-compiler
                                 --no-opengl-sw
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
    if(WIN32)
      set(mo_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Languages)
      set(mo ${mo_dir}/${lang}.mo)
    else()
      set(mo_dir ${CMAKE_CURRENT_BINARY_DIR}/${lang})
      set(mo ${mo_dir}/dolphin-emu.mo)
    endif()

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
  set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/DolphinQt.app)

  # Ask for an application bundle.
  set_target_properties(dolphin-emu PROPERTIES
    MACOSX_BUNDLE true
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/Info.plist"
    XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY ""
    OUTPUT_NAME DolphinQt
    )

  # Copy qt.conf into the bundle
  target_sources(dolphin-emu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf")
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/qt.conf" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # Copy icon into the bundle
  target_sources(dolphin-emu PRIVATE "${CMAKE_SOURCE_DIR}/Data/Dolphin.icns")
  set_source_files_properties("${CMAKE_SOURCE_DIR}/Data/Dolphin.icns" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # Manually include plugins
  find_package(Qt6QMacStylePlugin REQUIRED PATHS ${Qt6Widgets_DIR})
  find_package(Qt6QCocoaIntegrationPlugin REQUIRED PATHS ${Qt6Gui_DIR})

  # Copy Qt plugins into the bundle
  get_target_property(qtcocoa_location Qt6::QCocoaIntegrationPlugin LOCATION)
  target_sources(dolphin-emu PRIVATE "${qtcocoa_location}")
  set_source_files_properties("${qtcocoa_location}" PROPERTIES MACOSX_PACKAGE_LOCATION MacOS/platforms)

  get_target_property(qtmacstyle_location Qt6::QMacStylePlugin LOCATION)
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
  if(ENABLE_VULKAN)
    if(USE_BUNDLED_MOLTENVK)
      add_dependencies(dolphin-emu MoltenVK)
      ExternalProject_Get_Property(MoltenVK SOURCE_DIR)
      target_sources(dolphin-emu PRIVATE "${SOURCE_DIR}/Package/Release/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib")
      set_source_files_properties("${SOURCE_DIR}/Package/Release/MoltenVK/dynamic/dylib/macOS/libMoltenVK.dylib" PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks GENERATED ON)
    else()
      find_file(MOLTENVK_DYLIB NAMES libMoltenVK.dylib PATH_SUFFIXES lib)
      if(NOT MOLTENVK_DYLIB)
        message(FATAL_ERROR "Couldn't find libMoltenVK.dylib.  Enable USE_BUNDLED_MOLTENVK?")
      endif()
      target_sources(dolphin-emu PRIVATE ${MOLTENVK_DYLIB})
      set_source_files_properties(${MOLTENVK_DYLIB} PROPERTIES MACOSX_PACKAGE_LOCATION Frameworks)
    endif()
  endif()

  if(NOT SKIP_POSTPROCESS_BUNDLE)
    # Update library references to make the bundle portable
    include(DolphinPostprocessBundle)
    dolphin_postprocess_bundle(dolphin-emu)
    # Fix rpath
    add_custom_command(TARGET dolphin-emu
      POST_BUILD COMMAND
      ${CMAKE_INSTALL_NAME_TOOL} -add_rpath "@executable_path/../Frameworks/"
      $<TARGET_FILE:dolphin-emu>)
  endif()

  if(MACOS_CODE_SIGNING)
    add_custom_command(TARGET dolphin-emu
      POST_BUILD
      COMMAND "${CMAKE_SOURCE_DIR}/Tools/mac-codesign.sh"
      "-e" "${CMAKE_CURRENT_SOURCE_DIR}/DolphinEmu$<$<CONFIG:Debug>:Debug>.entitlements"
      "${MACOS_CODE_SIGNING_IDENTITY}"
      "${BUNDLE_PATH}"
    )
  endif()
else()
  install(TARGETS dolphin-emu RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(USE_MGBA)
  target_sources(dolphin-emu PRIVATE
    GBAHost.cpp
    GBAHost.h
    GBAWidget.cpp
    GBAWidget.h
  )
endif()

if(USE_DISCORD_PRESENCE)
  target_compile_definitions(dolphin-emu PRIVATE -DUSE_DISCORD_PRESENCE)
endif()

if(USE_RETRO_ACHIEVEMENTS)
  target_link_libraries(dolphin-emu PRIVATE rcheevos)
  target_compile_definitions(dolphin-emu PRIVATE -DUSE_RETRO_ACHIEVEMENTS)
endif()
