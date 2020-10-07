host_build {
    QT_CPU_FEATURES.i386 = sse sse2
} else {
    QT_CPU_FEATURES.arm64 = 
}
QT.global_private.enabled_features = alloca_malloc_h alloca gui network release_tools relocatable sql testlib widgets xml
QT.global_private.disabled_features = sse2 alloca_h android-style-assets avx2 private_tests dbus dbus-linked dlopen gc_binaries libudev posix_fallocate reduce_exports reduce_relocations stack-protector-strong system-zlib zstd
QT_COORD_TYPE = double
CONFIG += cross_compile compile_examples largefile precompile_header
QT_BUILD_PARTS += libs
QT_HOST_CFLAGS_DBUS += 
HOST_QT_TOOLS = c:\\src\\dolphin-qt\\dist\\dolphin.amd64_5.15.0\\bin
