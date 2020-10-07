
add_library(Qt5::QSGD3D12Adaptation MODULE IMPORTED)


_populate_Quick_plugin_properties(QSGD3D12Adaptation RELEASE "scenegraph/qsgd3d12backend.dll" TRUE)
_populate_Quick_plugin_properties(QSGD3D12Adaptation DEBUG "scenegraph/qsgd3d12backendd.dll" TRUE)

list(APPEND Qt5Quick_PLUGINS Qt5::QSGD3D12Adaptation)
set_property(TARGET Qt5::Quick APPEND PROPERTY QT_ALL_PLUGINS_scenegraph Qt5::QSGD3D12Adaptation)
set_property(TARGET Qt5::QSGD3D12Adaptation PROPERTY QT_PLUGIN_TYPE "scenegraph")
set_property(TARGET Qt5::QSGD3D12Adaptation PROPERTY QT_PLUGIN_EXTENDS "")
set_property(TARGET Qt5::QSGD3D12Adaptation PROPERTY QT_PLUGIN_CLASS_NAME "QSGD3D12Adaptation")
