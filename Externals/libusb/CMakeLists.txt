set(SRCS
	libusb/core.c
	libusb/descriptor.c
	libusb/hotplug.c
	libusb/io.c
	libusb/os/poll_windows.c
	libusb/os/threads_windows.c
	libusb/os/windows_usb.c
	libusb/sync.c)
add_library(usb STATIC ${SRCS})
target_include_directories(usb SYSTEM PUBLIC libusb)
target_include_directories(usb SYSTEM BEFORE PRIVATE msvc)
