# This file is only used for Android (because I'm lazy).
# We don't use CMake on Windows and just require libusb to be installed everywhere else.

set(SRCS
	core.c
	descriptor.c
	hotplug.c
	io.c
	strerror.c
	sync.c
)

include_directories(BEFORE ../android)

add_subdirectory(os)
add_library(usb-1.0 STATIC ${SRCS})
target_link_libraries(usb-1.0 usb-os)
