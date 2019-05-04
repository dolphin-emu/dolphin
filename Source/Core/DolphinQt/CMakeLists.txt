if (MSVC)
set(Qt5_Base "${CMAKE_SOURCE_DIR}/Externals/Qt/Qt5.11.1/5.11.1/msvc2017_64")
set(Qt5_DIR "${Qt5_Base}/lib/cmake/Qt5")
endif()

find_package(Qt5 5.9 REQUIRED COMPONENTS Gui Widgets)

set_property(TARGET Qt5::Core PROPERTY INTERFACE_COMPILE_FEATURES "")
message(STATUS "Found Qt version ${Qt5Core_VERSION}")

set(CMAKE_AUTOMOC ON)

add_executable(dolphin-emu
  AboutDialog.cpp
  CheatsManager.cpp
  DiscordHandler.cpp
  DiscordJoinRequestDialog.cpp
  FIFO/FIFOPlayerWindow.cpp
  FIFO/FIFOAnalyzer.cpp
  HotkeyScheduler.cpp
  Host.cpp
  Main.cpp
  MainWindow.cpp
  MenuBar.cpp
  RenderWidget.cpp
  Resources.cpp
  SearchBar.cpp
  Settings.cpp
  ToolBar.cpp
  Translation.cpp
  WiiUpdate.cpp
  WiiUpdate.h
  Config/CheatCodeEditor.cpp
  Config/ARCodeWidget.cpp
  Config/CheatWarningWidget.cpp
  Config/ControllersWindow.cpp
  Config/FilesystemWidget.cpp
  Config/GameConfigEdit.cpp
  Config/GameConfigHighlighter.cpp
  Config/GameConfigWidget.cpp
  Config/GeckoCodeWidget.cpp
  Config/Graphics/AdvancedWidget.cpp
  Config/Graphics/EnhancementsWidget.cpp
  Config/Graphics/GeneralWidget.cpp
  Config/Graphics/HacksWidget.cpp
  Config/Graphics/GraphicsBool.cpp
  Config/Graphics/GraphicsChoice.cpp
  Config/Graphics/GraphicsRadio.cpp
  Config/Graphics/GraphicsSlider.cpp
  Config/Graphics/GraphicsWidget.cpp
  Config/Graphics/GraphicsWindow.cpp
  Config/Graphics/PostProcessingConfigWindow.cpp
  Config/Graphics/SoftwareRendererWidget.cpp
  Config/InfoWidget.cpp
  Config/LogConfigWidget.cpp
  Config/LogWidget.cpp
  Config/Mapping/GCKeyboardEmu.cpp
  Config/Mapping/GCMicrophone.cpp
  Config/Mapping/GCPadEmu.cpp
  Config/Mapping/GCPadWiiUConfigDialog.cpp
  Config/Mapping/Hotkey3D.cpp
  Config/Mapping/HotkeyControllerProfile.cpp
  Config/Mapping/HotkeyDebugging.cpp
  Config/Mapping/HotkeyGeneral.cpp
  Config/Mapping/HotkeyGraphics.cpp
  Config/Mapping/HotkeyStates.cpp
  Config/Mapping/HotkeyStatesOther.cpp
  Config/Mapping/HotkeyTAS.cpp
  Config/Mapping/HotkeyWii.cpp
  Config/Mapping/IOWindow.cpp
  Config/Mapping/MappingButton.cpp
  Config/Mapping/MappingCommon.cpp
  Config/Mapping/MappingIndicator.cpp
  Config/Mapping/MappingNumeric.cpp
  Config/Mapping/MappingWidget.cpp
  Config/Mapping/MappingWindow.cpp
  Config/Mapping/WiimoteEmuExtension.cpp
  Config/Mapping/WiimoteEmuGeneral.cpp
  Config/Mapping/WiimoteEmuMotionControl.cpp
  Config/NewPatchDialog.cpp
  Config/PatchesWidget.cpp
  Config/PropertiesDialog.cpp
  Config/SettingsWindow.cpp
  Config/VerifyWidget.cpp
  Debugger/BreakpointWidget.cpp
  Debugger/CodeViewWidget.cpp
  Debugger/CodeWidget.cpp
  Debugger/JITWidget.cpp
  Debugger/MemoryViewWidget.cpp
  Debugger/MemoryWidget.cpp
  Debugger/NewBreakpointDialog.cpp
  Debugger/PatchInstructionDialog.cpp
  Debugger/RegisterColumn.cpp
  Debugger/RegisterWidget.cpp
  Debugger/WatchWidget.cpp
  GameList/GameList.cpp
  GameList/GameListModel.cpp
  GameList/GameTracker.cpp
  GameList/GridProxyModel.cpp
  GameList/ListProxyModel.cpp
  GCMemcardManager.cpp
  QtUtils/BlockUserInputFilter.cpp
  NetPlay/ChunkedProgressDialog.cpp
  NetPlay/GameListDialog.cpp
  NetPlay/MD5Dialog.cpp
  NetPlay/NetPlayBrowser.cpp
  NetPlay/NetPlayDialog.cpp
  NetPlay/NetPlaySetupDialog.cpp
  NetPlay/PadMappingDialog.cpp
  QtUtils/DoubleClickEventFilter.cpp
  QtUtils/ElidedButton.cpp
  QtUtils/FlowLayout.cpp
  QtUtils/ModalMessageBox.cpp
  QtUtils/ImageConverter.cpp
  QtUtils/WindowActivationEventFilter.cpp
  QtUtils/WinIconHelper.cpp
  QtUtils/WrapInScrollArea.cpp
  QtUtils/AspectRatioWidget.cpp
  ResourcePackManager.cpp
  Settings/AdvancedPane.cpp
  Settings/AudioPane.cpp
  Settings/GameCubePane.cpp
  Settings/GeneralPane.cpp
  Settings/InterfacePane.cpp
  Settings/PathPane.cpp
  Settings/WiiPane.cpp
  Settings/USBDeviceAddToWhitelistDialog.cpp
  TAS/GCTASInputWindow.cpp
  TAS/WiiTASInputWindow.cpp
  TAS/TASCheckBox.cpp
  TAS/TASInputWindow.cpp
  TAS/StickWidget.cpp
  TAS/IRWidget.cpp
  Updater.cpp
)

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
  imgui
)

set_target_properties(dolphin-emu PROPERTIES PROJECT_LABEL "Dolphin")

include(SourceGroupByFolder)
source_group_by_folder(dolphin-emu)

if(WIN32)
  if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(Dolphin_NAME "DolphinD")
  else()
   set(Dolphin_NAME "Dolphin")
  endif()

  set_target_properties(dolphin-emu PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME ${Dolphin_NAME}
  )
  target_sources(dolphin-emu PRIVATE DolphinQt.manifest DolphinQt.rc)
  target_compile_options(dolphin-emu PRIVATE /D _SILENCE_CXX17_RESULT_OF_DEPRECATION_WARNING)

  # Copy files for Windows

  # Copy Sys directory
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Data/Sys")
  file(GLOB_RECURSE resources RELATIVE "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Data/Sys/*")
  foreach(res ${resources})
    configure_file("${CMAKE_SOURCE_DIR}/Data/${res}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/${res}" COPYONLY)
    configure_file("${CMAKE_SOURCE_DIR}/Data/${res}" "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/${res}" COPYONLY)
  endforeach()

  # Copy qt.conf.win
  configure_file(qt.conf.win "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/qt.conf" COPYONLY)
  configure_file(qt.conf.win "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/qt.conf" COPYONLY)

  file(GLOB_RECURSE Qt5_PLUGIN_STYLES_DLLs "${Qt5_Base}/plugins/styles/*.dll")
  file(GLOB_RECURSE Qt5_PLUGIN_PLATFORMS_DLLs "${Qt5_Base}/plugins/platforms/*.dll")
  file(GLOB_RECURSE Qt5_PLUGIN_IMAGEFORMATS_DLLs "${Qt5_Base}/plugins/imageformats/*.dll")
  file(GLOB_RECURSE Qt5_PLUGIN_STYLES_DEBUG_DLLs "${Qt5_Base}/plugins/styles/*d.dll")
  file(GLOB_RECURSE Qt5_PLUGIN_PLATFORMS_DEBUG_DLLs "${Qt5_Base}/plugins/platforms/*d.dll")
  file(GLOB_RECURSE Qt5_PLUGIN_IMAGEFORMATS_DEBUG_DLLs "${Qt5_Base}/plugins/imageformats/*d.dll")

  # Filter out debug DLLs
  list(FILTER Qt5_PLUGIN_STYLES_DLLs EXCLUDE REGEX ".*d.dll")
  list(FILTER Qt5_PLUGIN_PLATFORMS_DLLs EXCLUDE REGEX ".*d.dll")
  list(FILTER Qt5_PLUGIN_IMAGEFORMATS_DLLs EXCLUDE REGEX ".*d.dll")

  # Copy release plugins
  foreach(DLL ${Qt5_PLUGIN_STYLES_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/QtPlugins/styles/${DLL_NAME}" COPYONLY)
  endforeach()
  foreach(DLL ${Qt5_PLUGIN_PLATFORMS_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/QtPlugins/platforms/${DLL_NAME}" COPYONLY)
  endforeach()
  foreach(DLL ${Qt5_PLUGIN_IMAGEFORMATS_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/QtPlugins/imageformats/${DLL_NAME}" COPYONLY)
  endforeach()

  # Copy debug plugins
  foreach(DLL ${Qt5_PLUGIN_STYLES_DEBUG_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/QtPlugins/styles/${DLL_NAME}" COPYONLY)
  endforeach()
  foreach(DLL ${Qt5_PLUGIN_PLATFORMS_DEBUG_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/QtPlugins/platforms/${DLL_NAME}" COPYONLY)
  endforeach()
  foreach(DLL ${Qt5_PLUGIN_IMAGEFORMATS_DEBUG_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/QtPlugins/imageformats/${DLL_NAME}" COPYONLY)
  endforeach()

  # Copy DLLs
  file(GLOB Qt5_DLLs ${Qt5_Base}/bin/Qt5*.dll)
  file(GLOB Qt5_DEBUG_DLLs ${Qt5_Base}/bin/Qt5*d.dll)

  # Filter out debug DLLs
  list(FILTER Qt5_DLLs EXCLUDE REGEX ".*Qt5.*d.dll")

  foreach(DLL ${Qt5_DEBUG_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Debug/${DLL_NAME}" COPYONLY)
  endforeach()
  foreach(DLL ${Qt5_DLLs})
    get_filename_component(DLL_NAME ${DLL} NAME)
    configure_file(${DLL} "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/Release/${DLL_NAME}" COPYONLY)
  endforeach()
else()
  target_sources(dolphin-emu PRIVATE QtUtils/SignalDaemon.cpp)
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
