
add_library(Qt5::QQmlDebugServerFactory MODULE IMPORTED)


_populate_Qml_plugin_properties(QQmlDebugServerFactory RELEASE "qmltooling/qmldbg_server.dll" TRUE)
_populate_Qml_plugin_properties(QQmlDebugServerFactory DEBUG "qmltooling/qmldbg_serverd.dll" TRUE)

list(APPEND Qt5Qml_PLUGINS Qt5::QQmlDebugServerFactory)
set_property(TARGET Qt5::Qml APPEND PROPERTY QT_ALL_PLUGINS_qmltooling Qt5::QQmlDebugServerFactory)
set_property(TARGET Qt5::QQmlDebugServerFactory PROPERTY QT_PLUGIN_TYPE "qmltooling")
set_property(TARGET Qt5::QQmlDebugServerFactory PROPERTY QT_PLUGIN_EXTENDS "")
set_property(TARGET Qt5::QQmlDebugServerFactory PROPERTY QT_PLUGIN_CLASS_NAME "QQmlDebugServerFactory")
