include_directories(..)
add_library(usb-os STATIC
	linux_netlink.c
	linux_usbfs.c
	poll_posix.c
	threads_posix.c
)
