if(QT5_STRICT_PLUGIN_GLOB OR Qt5Qml_STRICT_PLUGIN_GLOB)
    file(GLOB _qt5qml_other_plugins ${CMAKE_CURRENT_LIST_DIR}/Qt5Qml_*Factory.cmake)

    foreach(_other_plugin ${_qt5qml_other_plugins})
        include(${_other_plugin} OPTIONAL)
    endforeach()
endif()
