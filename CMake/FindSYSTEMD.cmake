find_package(PkgConfig QUIET)
pkg_check_modules(PC_SYSTEMD QUIET "libsystemd")

if (PC_SYSTEMD_FOUND)
	add_definitions(${PC_SYSTEMD_CFLAGS} ${PC_SYSTEMD_CFLAGS_OTHER})
endif(PC_SYSTEMD_FOUND)

find_path(
    SYSTEMD_INCLUDE_DIRS
    NAMES systemd/sd-daemon.h
    HINTS ${PC_SYSTEMD_INCLUDEDIR} ${PC_SYSTEMD_INCLUDE_DIRS}
)

find_library(
    SYSTEMD_LIBRARIES
    NAMES systemd
    HINTS ${PC_SYSTEMD_LIBDIR} ${PC_SYSTEMD_LIBRARY_DIRS}
)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
    SYSTEMD
    REQUIRED_VARS SYSTEMD_LIBRARIES SYSTEMD_INCLUDE_DIRS
)
mark_as_advanced(
    SYSTEMD_FOUND
    SYSTEMD_LIBRARIES SYSTEMD_INCLUDE_DIRS
)
