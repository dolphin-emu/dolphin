
add_library(Qt5::QQmlPreviewServiceFactory MODULE IMPORTED)


_populate_Qml_plugin_properties(QQmlPreviewServiceFactory RELEASE "qmltooling/qmldbg_preview.dll" TRUE)
_populate_Qml_plugin_properties(QQmlPreviewServiceFactory DEBUG "qmltooling/qmldbg_previewd.dll" TRUE)

list(APPEND Qt5Qml_PLUGINS Qt5::QQmlPreviewServiceFactory)
set_property(TARGET Qt5::Qml APPEND PROPERTY QT_ALL_PLUGINS_qmltooling Qt5::QQmlPreviewServiceFactory)
set_property(TARGET Qt5::QQmlPreviewServiceFactory PROPERTY QT_PLUGIN_TYPE "qmltooling")
set_property(TARGET Qt5::QQmlPreviewServiceFactory PROPERTY QT_PLUGIN_EXTENDS "")
set_property(TARGET Qt5::QQmlPreviewServiceFactory PROPERTY QT_PLUGIN_CLASS_NAME "QQmlPreviewServiceFactory")
