if(NOT DEFINED QT_DEFAULT_MAJOR_VERSION)
    set(QT_DEFAULT_MAJOR_VERSION 5)
endif()

if (NOT TARGET Qt5::qmake)
    add_executable(Qt5::qmake IMPORTED)

    set(imported_location "${_qt5Core_install_prefix}/bin/qmake.exe")
    _qt5_Core_check_file_exists(${imported_location})

    set_target_properties(Qt5::qmake PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()

if (NOT TARGET Qt5::moc)
    add_executable(Qt5::moc IMPORTED)

    set(imported_location "${_qt5Core_install_prefix}/bin/moc.exe")
    _qt5_Core_check_file_exists(${imported_location})

    set_target_properties(Qt5::moc PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
    # For CMake automoc feature
    get_target_property(QT_MOC_EXECUTABLE Qt5::moc LOCATION)
endif()

if (NOT TARGET Qt5::rcc)
    add_executable(Qt5::rcc IMPORTED)

    set(imported_location "${_qt5Core_install_prefix}/bin/rcc.exe")
    _qt5_Core_check_file_exists(${imported_location})

    set_target_properties(Qt5::rcc PROPERTIES
        IMPORTED_LOCATION ${imported_location}
    )
endif()

set(CMAKE_AUTOMOC_MACRO_NAMES Q_OBJECT Q_GADGET Q_NAMESPACE Q_NAMESPACE_EXPORT)

set(Qt5Core_QMAKE_EXECUTABLE Qt5::qmake)
set(Qt5Core_MOC_EXECUTABLE Qt5::moc)
set(Qt5Core_RCC_EXECUTABLE Qt5::rcc)

set_property(TARGET Qt5::Core PROPERTY INTERFACE_QT_MAJOR_VERSION 5)
set_property(TARGET Qt5::Core PROPERTY INTERFACE_QT_COORD_TYPE double)
set_property(TARGET Qt5::Core APPEND PROPERTY
  COMPATIBLE_INTERFACE_STRING QT_MAJOR_VERSION QT_COORD_TYPE
)

include("${CMAKE_CURRENT_LIST_DIR}/Qt5CoreConfigExtrasMkspecDir.cmake")

foreach(_dir ${_qt5_corelib_extra_includes})
    _qt5_Core_check_file_exists(${_dir})
endforeach()

list(APPEND Qt5Core_INCLUDE_DIRS ${_qt5_corelib_extra_includes})
set_property(TARGET Qt5::Core APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${_qt5_corelib_extra_includes})
set(_qt5_corelib_extra_includes)

# Targets using Qt need to use the POSITION_INDEPENDENT_CODE property. The
# Qt5_POSITION_INDEPENDENT_CODE variable is used in the # qt5_use_module
# macro to add it.
set(Qt5_POSITION_INDEPENDENT_CODE True)

# On x86 and x86-64 systems with ELF binaries (especially Linux), due to
# a new optimization in GCC 5.x in combination with a recent version of
# GNU binutils, compiling Qt applications with -fPIE is no longer
# enough.
# Applications now need to be compiled with the -fPIC option if the Qt option
# "reduce relocations" is active. For backward compatibility only, Qt accepts
# the use of -fPIE for GCC 4.x versions.
set_property(TARGET Qt5::Core APPEND PROPERTY INTERFACE_COMPILE_OPTIONS )

# TODO Qt6: Remove
set(Qt5Core_EXECUTABLE_COMPILE_FLAGS "")



set_property(TARGET Qt5::Core APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS $<$<NOT:$<CONFIG:Debug>>:QT_NO_DEBUG>)

set_property(TARGET Qt5::Core PROPERTY INTERFACE_COMPILE_FEATURES cxx_decltype)



set(Qt5Core_QTMAIN_LIBRARIES Qt5::WinMain)

if (NOT TARGET Qt5::WinMain)
    add_library(Qt5::WinMain STATIC IMPORTED)

    set_property(TARGET Qt5::WinMain APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
    set(imported_location "${_qt5Core_install_prefix}/lib/qtmain.lib")

    set_target_properties(Qt5::WinMain PROPERTIES
        IMPORTED_LOCATION_RELEASE ${imported_location}
    )

    set_property(TARGET Qt5::WinMain APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)

    set(imported_location "${_qt5Core_install_prefix}/lib/qtmaind.lib")

    set_target_properties(Qt5::WinMain PROPERTIES
        IMPORTED_LOCATION_DEBUG ${imported_location}
    )

    if (NOT Qt5_NO_LINK_QTMAIN)
        set(_isExe $<STREQUAL:$<TARGET_PROPERTY:TYPE>,EXECUTABLE>)
        set(_isWin32 $<BOOL:$<TARGET_PROPERTY:WIN32_EXECUTABLE>>)
        set(_isNotExcluded $<NOT:$<BOOL:$<TARGET_PROPERTY:Qt5_NO_LINK_QTMAIN>>>)
        set(_isPolicyNEW $<TARGET_POLICY:CMP0020>)
        get_target_property(_configs Qt5::Core IMPORTED_CONFIGURATIONS)
        set_property(TARGET Qt5::Core APPEND PROPERTY
            INTERFACE_LINK_LIBRARIES
                $<$<AND:${_isExe},${_isWin32},${_isNotExcluded},${_isPolicyNEW}>:Qt5::WinMain>
        )
        # For backward compatibility with CMake < 2.8.12
        foreach(_config ${_configs})
            set_property(TARGET Qt5::Core APPEND PROPERTY
                IMPORTED_LINK_INTERFACE_LIBRARIES_${_config}
                    $<$<AND:${_isExe},${_isWin32},${_isNotExcluded},${_isPolicyNEW}>:Qt5::WinMain>
            )
        endforeach()
        unset(_configs)
        unset(_isExe)
        unset(_isWin32)
        unset(_isNotExcluded)
        unset(_isPolicyNEW)
    endif()
endif()

get_filename_component(_Qt5CoreConfigDir ${CMAKE_CURRENT_LIST_FILE} PATH)

set(_Qt5CTestMacros "${_Qt5CoreConfigDir}/Qt5CTestMacros.cmake")

if (ANDROID_PLATFORM)
    include("${CMAKE_CURRENT_LIST_DIR}/Qt5AndroidSupport.cmake")
endif()

_qt5_Core_check_file_exists(${_Qt5CTestMacros})

# Create versionless tool targets.
foreach(__qt_tool qmake moc rcc)
    if(NOT "${QT_NO_CREATE_VERSIONLESS_TARGETS}" AND NOT TARGET Qt::${__qt_tool}
       AND TARGET Qt5::${__qt_tool})
        add_executable(Qt::${__qt_tool} IMPORTED)
        get_target_property(__qt_imported_location Qt5::${__qt_tool} IMPORTED_LOCATION)
        set_target_properties(Qt::${__qt_tool}
                              PROPERTIES IMPORTED_LOCATION "${__qt_imported_location}")
    endif()
endforeach()

# Add a versionless target for WinMain.
if(NOT "${QT_NO_CREATE_VERSIONLESS_TARGETS}" AND NOT TARGET Qt::WinMain)
    add_library(Qt::WinMain INTERFACE IMPORTED)
    set_target_properties(Qt::WinMain PROPERTIES
        INTERFACE_LINK_LIBRARIES "Qt5::WinMain"
    )
endif()
