add_library(usb STATIC EXCLUDE_FROM_ALL
    libusb/core.c
    libusb/core.c
    libusb/descriptor.c
    libusb/hotplug.c
    libusb/io.c
    libusb/strerror.c
    libusb/sync.c
)
set_target_properties(usb PROPERTIES VERSION 1.0.19)
if(WIN32)
    target_include_directories(usb BEFORE PUBLIC libusb PRIVATE msvc)
else()
target_include_directories(usb
    # turns out other projects also have "config.h", so make sure the
    # LibUSB one comes first
    BEFORE

    PUBLIC libusb
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)
endif()

if(WIN32 OR CYGWIN)
    target_sources(usb PRIVATE
      libusb/os/threads_windows.c
      libusb/os/windows_winusb.c
      libusb/os/windows_usbdk.c
      libusb/os/windows_nt_common.c
    )
    set(OS_WINDOWS TRUE)
elseif(APPLE)
    target_sources(usb PRIVATE libusb/os/darwin_usb.c)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    find_library(IOKIT_LIBRARY IOKit)
    find_library(OBJC_LIBRARY objc)
    target_link_libraries(usb PRIVATE
        ${COREFOUNDATION_LIBRARY}
        ${IOKIT_LIBRARY}
        ${OBJC_LIBRARY}
    )
    set(OS_DARWIN TRUE)
# # Dolphin on Android doesn't use libusb.
#elseif(ANDROID)
#    target_sources(usb PRIVATE
#        libusb/os/linux_usbfs.c
#        libusb/os/linux_netlink.c
#    )
#    find_library(LOG_LIBRARY log)
#    target_link_libraries(usb PRIVATE ${LOG_LIBRARY})
#    set(OS_LINUX TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    target_sources(usb PRIVATE libusb/os/linux_usbfs.c)
    find_package(Libudev)
    if(LIBUDEV_FOUND)
        target_sources(usb PRIVATE libusb/os/linux_udev.c)
        target_link_libraries(usb PRIVATE "${LIBUDEV_LIBRARIES}")
        target_include_directories(usb PRIVATE "${LIBUDEV_INCLUDE_DIR}")
        set(HAVE_LIBUDEV TRUE)
        set(USE_UDEV TRUE)
    else()
        target_sources(usb PRIVATE libusb/os/linux_netlink.c)
    endif()
    set(OS_LINUX TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
    target_sources(usb PRIVATE libusb/os/netbsd_usb.c)
    set(OS_NETBSD TRUE)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    target_sources(usb PRIVATE libusb/os/openbsd_usb.c)
    set(OS_OPENBSD TRUE)
endif()

if(UNIX)
    target_sources(usb PRIVATE
        libusb/os/poll_posix.c
        libusb/os/threads_posix.c
    )
    find_package(Threads REQUIRED)
    if(THREADS_HAVE_PTHREAD_ARG)
      target_compile_options(usb PUBLIC "-pthread")
    endif()
    if(CMAKE_THREAD_LIBS_INIT)
      target_link_libraries(usb PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
    endif()
    set(THREADS_POSIX TRUE)
elseif(WIN32)
    target_sources(usb PRIVATE
        libusb/os/poll_windows.c
        libusb/os/threads_windows.c
    )
endif()

include(CheckFunctionExists)
include(CheckIncludeFiles)
include(CheckTypeSize)
check_include_files(asm/types.h HAVE_ASM_TYPES_H)
check_function_exists(gettimeofday HAVE_GETTIMEOFDAY)
check_include_files(linux/filter.h HAVE_LINUX_FILTER_H)
check_include_files(linux/netlink.h HAVE_LINUX_NETLINK_H)
check_include_files(poll.h HAVE_POLL_H)
check_include_files(signal.h HAVE_SIGNAL_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_type_size("struct timespec" STRUCT_TIMESPEC)
check_function_exists(syslog HAVE_SYSLOG_FUNC)
check_include_files(syslog.h HAVE_SYSLOG_H)
check_include_files(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)

set(CMAKE_EXTRA_INCLUDE_FILES poll.h)
check_type_size("nfds_t" nfds_t)
unset(CMAKE_EXTRA_INCLUDE_FILES)
if(HAVE_NFDS_T)
    set(POLL_NFDS_TYPE "nfds_t")
else()
    set(POLL_NFDS_TYPE "unsigned int")
endif()

check_include_files(sys/timerfd.h USBI_TIMERFD_AVAILABLE)


configure_file(config.h.in config.h)
