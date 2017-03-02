find_package(Qt5Widgets REQUIRED)
message(STATUS "Found Qt version ${Qt5Core_VERSION}")

include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_definitions(-DQT_USE_QSTRINGBUILDER -DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII)
set(CMAKE_AUTOMOC ON)

set(SRCS
  AboutDialog.cpp
  Host.cpp
  InDevelopmentWarning.cpp
  Main.cpp
  MainWindow.cpp
  MenuBar.cpp
  RenderWidget.cpp
  Resources.cpp
  Settings.cpp
  ToolBar.cpp
  Config/FilesystemWidget.cpp
  Config/InfoWidget.cpp
  Config/PathDialog.cpp
  Config/PropertiesDialog.cpp
  Config/SettingsWindow.cpp
  GameList/GameFile.cpp
  GameList/GameList.cpp
  GameList/GameListModel.cpp
  GameList/GameTracker.cpp
  GameList/ListProxyModel.cpp
  GameList/TableDelegate.cpp
)

list(APPEND LIBS core uicommon)

if(WIN32)
  list(APPEND SRCS DolphinQt2.manifest)
endif()

set(DOLPHINQT2_BINARY dolphin-emu-qt2)

add_executable(${DOLPHINQT2_BINARY} ${SRCS} ${UI_HEADERS})
target_link_libraries(${DOLPHINQT2_BINARY} ${LIBS} Qt5::Widgets)

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
