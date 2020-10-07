
add_library(Qt5::QWindowsVistaStylePlugin MODULE IMPORTED)


_populate_Widgets_plugin_properties(QWindowsVistaStylePlugin RELEASE "styles/qwindowsvistastyle.dll" TRUE)
_populate_Widgets_plugin_properties(QWindowsVistaStylePlugin DEBUG "styles/qwindowsvistastyled.dll" TRUE)

list(APPEND Qt5Widgets_PLUGINS Qt5::QWindowsVistaStylePlugin)
set_property(TARGET Qt5::Widgets APPEND PROPERTY QT_ALL_PLUGINS_styles Qt5::QWindowsVistaStylePlugin)
set_property(TARGET Qt5::QWindowsVistaStylePlugin PROPERTY QT_PLUGIN_TYPE "styles")
set_property(TARGET Qt5::QWindowsVistaStylePlugin PROPERTY QT_PLUGIN_EXTENDS "")
set_property(TARGET Qt5::QWindowsVistaStylePlugin PROPERTY QT_PLUGIN_CLASS_NAME "QWindowsVistaStylePlugin")
