dolphin_fetchcontent_declare(
  libusb
  GIT_REPOSITORY https://github.com/libusb/libusb.git
  GIT_TAG 15a7ebb4d426c5ce196684347d2b7cafad862626 # v1.0.29
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(libusb)

add_library(usb STATIC EXCLUDE_FROM_ALL
  ${libusb_SOURCE_DIR}/libusb/core.c
  ${libusb_SOURCE_DIR}/libusb/descriptor.c
  ${libusb_SOURCE_DIR}/libusb/hotplug.c
  ${libusb_SOURCE_DIR}/libusb/io.c
  ${libusb_SOURCE_DIR}/libusb/strerror.c
  ${libusb_SOURCE_DIR}/libusb/sync.c
)

set_target_properties(usb PROPERTIES VERSION 1.0.26)
if(WIN32)
  target_include_directories(usb BEFORE PUBLIC ${libusb_SOURCE_DIR}/libusb PRIVATE ${libusb_SOURCE_DIR}/msvc)
else()
target_include_directories(usb
  # turns out other projects also have "config.h", so make sure the
  # LibUSB one comes first
  BEFORE

  PUBLIC ${libusb_SOURCE_DIR}/libusb
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}"
)
endif()

if(WIN32 OR CYGWIN)
  target_sources(usb PRIVATE
    ${libusb_SOURCE_DIR}/libusb/os/threads_windows.c
    ${libusb_SOURCE_DIR}/libusb/os/windows_common.c
    ${libusb_SOURCE_DIR}/libusb/os/windows_usbdk.c
    ${libusb_SOURCE_DIR}/libusb/os/windows_winusb.c
    ${libusb_SOURCE_DIR}/libusb/os/events_windows.c
  )
  set(PLATFORM_WINDOWS TRUE)
elseif(APPLE)
  target_sources(usb PRIVATE ${libusb_SOURCE_DIR}/libusb/os/darwin_usb.c)
  find_library(COREFOUNDATION_LIBRARY CoreFoundation)
  find_library(IOKIT_LIBRARY IOKit)
  find_library(OBJC_LIBRARY objc)
  find_library(SECURITY_LIBRARY Security)
  target_link_libraries(usb PRIVATE
    ${COREFOUNDATION_LIBRARY}
    ${IOKIT_LIBRARY}
    ${OBJC_LIBRARY}
    ${SECURITY_LIBRARY}
  )
# # Dolphin on Android doesn't use libusb.
#elseif(ANDROID)
#  target_sources(usb PRIVATE
#    ${libusb_SOURCE_DIR}/libusb/os/linux_usbfs.c
#    ${libusb_SOURCE_DIR}/libusb/os/linux_netlink.c
#  )
#  find_library(LOG_LIBRARY log)
#  target_link_libraries(usb PRIVATE ${LOG_LIBRARY})
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
  target_sources(usb PRIVATE ${libusb_SOURCE_DIR}/libusb/os/linux_usbfs.c)
  find_package(LIBUDEV)
  if(LIBUDEV_FOUND)
    target_sources(usb PRIVATE ${libusb_SOURCE_DIR}/libusb/os/linux_udev.c)
    target_link_libraries(usb PRIVATE "${LIBUDEV_LIBRARIES}")
    target_include_directories(usb PRIVATE "${LIBUDEV_INCLUDE_DIR}")
    set(HAVE_LIBUDEV TRUE)
  else()
    target_sources(usb PRIVATE ${libusb_SOURCE_DIR}/libusb/os/linux_netlink.c)
  endif()
elseif(${CMAKE_SYSTEM_NAME} MATCHES "NetBSD")
  target_sources(usb PRIVATE ${libusb_SOURCE_DIR}/libusb/os/netbsd_usb.c)
elseif(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
  target_sources(usb PRIVATE ${libusb_SOURCE_DIR}/libusb/os/openbsd_usb.c)
endif()

if(UNIX)
  target_sources(usb PRIVATE
    ${libusb_SOURCE_DIR}/libusb/os/events_posix.c
    ${libusb_SOURCE_DIR}/libusb/os/threads_posix.c
  )
  find_package(Threads REQUIRED)
  if(THREADS_HAVE_PTHREAD_ARG)
    target_compile_options(usb PUBLIC "-pthread")
  endif()
  if(CMAKE_THREAD_LIBS_INIT)
    target_link_libraries(usb PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
  endif()
  set(PLATFORM_POSIX TRUE)
endif()

# Create config.h

include(CheckFunctionExists)
include(CheckIncludeFiles)
#include(CheckLibraryExists)
include(CheckTypeSize)
include(CheckSymbolExists)

check_include_files(asm/types.h HAVE_ASM_TYPES_H)
check_function_exists(clock_gettime HAVE_CLOCK_GETTIME)
check_symbol_exists(EFD_CLOEXEC "sys/eventfd.h" HAVE_DECL_EFD_CLOEXEC)
check_symbol_exists(EFD_NONBLOCK "sys/eventfd.h" HAVE_DECL_EFD_NONBLOCK)
check_symbol_exists(TFD_CLOEXEC "sys/timerfd.h" HAVE_DECL_TFD_CLOEXEC)
check_symbol_exists(TFD_NONBLOCK "sys/timerfd.h" HAVE_DECL_TFD_NONBLOCK)
check_include_files(dlfcn.h HAVE_DLFCN_H)
check_include_files(sys/eventfd.h HAVE_EVENTFD)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(IOKit/usb/IOUSBHostFamilyDefinitions.h HAVE_IOKIT_USB_IOUSBHOSTFAMILYDEFINITIONS_H)
#check_library_exists(udev HAVE_LIBUDEV)
check_include_files(memory.h HAVE_MEMORY_H)

set(CMAKE_EXTRA_INCLUDE_FILES poll.h)
check_type_size("nfds_t" NFDS_T)
unset(CMAKE_EXTRA_INCLUDE_FILES)

check_function_exists(pipe2 HAVE_PIPE2)
check_function_exists(pthread_condattr_setclock HAVE_PTHREAD_CONDATTR_SETCLOCK)
check_function_exists(pthread_setname_np HAVE_PTHREAD_SETNAME_NP)
check_function_exists(pthread_threadid_np HAVE_PTHREAD_THREADID_NP)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdlib.h HAVE_STDLIB_H)
check_include_files(strings.h HAVE_STRINGS_H)
check_include_files(string.h HAVE_STRING_H)
check_type_size("struct timespec" STRUCT_TIMESPEC)
check_function_exists(syslog HAVE_SYSLOG)
check_include_files(sys/stat.h HAVE_SYS_STAT_H)
check_include_files(sys/time.h HAVE_SYS_TIME_H)
check_include_files(sys/types.h HAVE_SYS_TYPES_H)
check_include_files(sys/timerfd.h HAVE_TIMERFD)
check_include_files(unistd.h HAVE_UNISTD_H)

configure_file(config.h.in config.h)

# We don't create an ALIAS here so that dependencies (SDL) that try_compile libusb
# don't fail due to LibUSB::LibUSB being an alias target
add_library(LibUSB::LibUSB INTERFACE IMPORTED GLOBAL)
target_link_libraries(LibUSB::LibUSB
  INTERFACE
    usb
)
target_include_directories(LibUSB::LibUSB
  INTERFACE
    ${libusb_SOURCE_DIR}/libusb
)
