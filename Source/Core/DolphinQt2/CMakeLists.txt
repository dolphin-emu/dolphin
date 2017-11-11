find_package(Qt5Widgets)
find_package(Qt5Gui)

if(NOT Qt5Widgets_FOUND OR NOT Qt5Gui_FOUND)
  message(FATAL_ERROR "Could not find Qt5, which is required to build the DolphinQt2 frontend. If you don't want to build the Qt frontend, pass -DENABLE_QT2=False to
CMake.")
endif()

set_property(TARGET Qt5::Core PROPERTY INTERFACE_COMPILE_FEATURES "")
message(STATUS "Found Qt version ${Qt5Core_VERSION}")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

include_directories(${Qt5Gui_PRIVATE_INCLUDE_DIRS})

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
set(CMAKE_AUTOMOC ON)

set(SRCS
  AboutDialog.cpp
  HotkeyScheduler.cpp
  Host.cpp
  InDevelopmentWarning.cpp
  Main.cpp
  MainWindow.cpp
  MenuBar.cpp
  RenderWidget.cpp
  Resources.cpp
  Settings.cpp
  ToolBar.cpp
  Translation.cpp
  WiiUpdate.cpp
  WiiUpdate.h
  Config/CheatWarningWidget.cpp
  Config/ControllersWindow.cpp
  Config/FilesystemWidget.cpp
  Config/GeckoCodeWidget.cpp
  Config/Graphics/AdvancedWidget.cpp
  Config/Graphics/EnhancementsWidget.cpp
  Config/Graphics/GeneralWidget.cpp
  Config/Graphics/HacksWidget.cpp
  Config/Graphics/GraphicsBool.cpp
  Config/Graphics/GraphicsChoice.cpp
  Config/Graphics/GraphicsSlider.cpp
  Config/Graphics/GraphicsWidget.cpp
  Config/Graphics/GraphicsWindow.cpp
  Config/Graphics/SoftwareRendererWidget.cpp
  Config/InfoWidget.cpp
  Config/LogConfigWidget.cpp
  Config/LogWidget.cpp
  Config/Mapping/GCKeyboardEmu.cpp
  Config/Mapping/GCPadEmu.cpp
  Config/Mapping/GCPadWiiUConfigDialog.cpp
  Config/Mapping/Hotkey3D.cpp
  Config/Mapping/HotkeyGeneral.cpp
  Config/Mapping/HotkeyGraphics.cpp
  Config/Mapping/HotkeyStates.cpp
  Config/Mapping/HotkeyTAS.cpp
  Config/Mapping/HotkeyWii.cpp
  Config/Mapping/IOWindow.cpp
  Config/Mapping/MappingBool.cpp
  Config/Mapping/MappingButton.cpp
  Config/Mapping/MappingCommon.cpp
  Config/Mapping/MappingNumeric.cpp
  Config/Mapping/MappingWidget.cpp
  Config/Mapping/MappingWindow.cpp
  Config/Mapping/WiimoteEmuExtension.cpp
  Config/Mapping/WiimoteEmuGeneral.cpp
  Config/Mapping/WiimoteEmuMotionControl.cpp
  Config/PropertiesDialog.cpp
  Config/SettingsWindow.cpp
  GameList/GameFileCache.cpp
  GameList/GameFile.cpp
  GameList/GameList.cpp
  GameList/GameListModel.cpp
  GameList/GameTracker.cpp
  GameList/GridProxyModel.cpp
  GameList/ListProxyModel.cpp
  QtUtils/BlockUserInputFilter.cpp
  NetPlay/GameListDialog.cpp
  NetPlay/MD5Dialog.cpp
  NetPlay/NetPlayDialog.cpp
  NetPlay/NetPlaySetupDialog.cpp
  NetPlay/PadMappingDialog.cpp
  QtUtils/DoubleClickEventFilter.cpp
  QtUtils/ElidedButton.cpp
  QtUtils/ListTabWidget.cpp
  QtUtils/WindowActivationEventFilter.cpp
  Settings/AdvancedPane.cpp
  Settings/AudioPane.cpp
  Settings/GeneralPane.cpp
  Settings/InterfacePane.cpp
  Settings/PathPane.cpp
)

list(APPEND LIBS core uicommon)

if(WIN32)
  list(APPEND SRCS DolphinQt2.manifest)
endif()

set(DOLPHINQT2_BINARY dolphin-emu-qt2)

add_executable(${DOLPHINQT2_BINARY} ${SRCS} ${UI_HEADERS})
target_link_libraries(${DOLPHINQT2_BINARY} ${LIBS} Qt5::Widgets)

# Handle localization
find_package(Gettext)
if(GETTEXT_MSGMERGE_EXECUTABLE AND GETTEXT_MSGFMT_EXECUTABLE)
  set(pot_file "${CMAKE_SOURCE_DIR}/Languages/po/dolphin-emu.pot")
  file(GLOB LINGUAS ${CMAKE_SOURCE_DIR}/Languages/po/*.po)

  target_sources(dolphin-emu-qt2 PRIVATE ${pot_file} ${LINGUAS})
  source_group("Localization" FILES ${LINGUAS})
  source_group("Localization\\\\Generated" FILES ${pot_file})

  foreach(po ${LINGUAS})
    get_filename_component(lang ${po} NAME_WE)
    set(mo_dir ${CMAKE_CURRENT_BINARY_DIR}/${lang})
    set(mo ${mo_dir}/dolphin-emu.mo)

    target_sources(dolphin-emu-qt2 PRIVATE ${mo})
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
  # Note: This is copied from DolphinQt, based on the DolphinWX version.

  include(BundleUtilities)
  set(BUNDLE_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${DOLPHINQT2_BINARY}.app)

  # Ask for an application bundle.
  set_target_properties(${DOLPHINQT2_BINARY} PROPERTIES
    MACOSX_BUNDLE true
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in
    )

  # Copy qt.conf into the bundle
  target_sources(${DOLPHINQT2_BINARY} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/qt.conf")
  set_source_files_properties("${CMAKE_CURRENT_SOURCE_DIR}/qt.conf" PROPERTIES MACOSX_PACKAGE_LOCATION Resources)

  # Copy Qt plugins into the bundle
  get_target_property(qtcocoa_location Qt5::QCocoaIntegrationPlugin LOCATION)
  target_sources(${DOLPHINQT2_BINARY} PRIVATE "${qtcocoa_location}")
  set_source_files_properties("${qtcocoa_location}" PROPERTIES MACOSX_PACKAGE_LOCATION MacOS/platforms)

  # Copy resources into the bundle
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/Data/Sys")
  file(GLOB_RECURSE resources RELATIVE "${CMAKE_SOURCE_DIR}/Data" "${CMAKE_SOURCE_DIR}/Data/Sys/*")
  foreach(res ${resources})
    target_sources(${DOLPHINQT2_BINARY} PRIVATE "${CMAKE_SOURCE_DIR}/Data/${res}")
    get_filename_component(resdir "${res}" DIRECTORY)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/Data/${res}" PROPERTIES
      MACOSX_PACKAGE_LOCATION "Resources/${resdir}")
    source_group("Resources" FILES "${CMAKE_SOURCE_DIR}/Data/${res}")
  endforeach()

  # Update library references to make the bundle portable
  include(DolphinPostprocessBundle)
  dolphin_postprocess_bundle(${DOLPHINQT2_BINARY})
else()
  install(TARGETS ${DOLPHINQT2_BINARY} RUNTIME DESTINATION ${bindir})
endif()
