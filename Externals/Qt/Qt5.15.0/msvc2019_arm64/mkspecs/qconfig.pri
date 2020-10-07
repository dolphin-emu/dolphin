host_build {
    QT_ARCH = i386
    QT_BUILDABI = i386-little_endian-ilp32
    QT_TARGET_ARCH = arm64
    QT_TARGET_BUILDABI = arm64-little_endian-lp64
} else {
    QT_ARCH = arm64
    QT_BUILDABI = arm64-little_endian-lp64
}
QT.global.enabled_features = shared cross_compile shared debug_and_release build_all c++11 c++14 c++17 c++1z thread future signaling_nan
QT.global.disabled_features = framework rpath appstore-compliant c++2a c99 c11 concurrent force_asserts pkg-config separate_debug_info simulator_and_device static
QT_CONFIG += shared shared debug_and_release release debug build_all c++11 c++14 c++17 c++1z no-pkg-config release_tools stl
CONFIG += shared cross_compile shared debug no_plugin_manifest
QT_VERSION = 5.15.0
QT_MAJOR_VERSION = 5
QT_MINOR_VERSION = 15
QT_PATCH_VERSION = 0
QT_MSVC_MAJOR_VERSION = 19
QT_MSVC_MINOR_VERSION = 27
QT_MSVC_PATCH_VERSION = 29111
QT_EDITION = OpenSource
