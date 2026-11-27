dolphin_fetchcontent_declare(
  libusb
  GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
  GIT_TAG c8477c10ac2ac6b1718d4d498e102b9f18b776f5 # v1.0.30-0
  GIT_SHALLOW TRUE
)

set(LIBUSB_INSTALL_TARGETS OFF)
set(LIBUSB_ENABLE_WINDOWS_HOTPLUG ON)

FetchContent_MakeAvailable(libusb)

# We don't create an ALIAS here so that dependencies (SDL) that try_compile libusb
# don't fail due to LibUSB::LibUSB being an alias target
add_library(LibUSB::LibUSB INTERFACE IMPORTED GLOBAL)
target_link_libraries(LibUSB::LibUSB
  INTERFACE
  usb-1.0
)
target_include_directories(LibUSB::LibUSB
  INTERFACE
  ${libusb_SOURCE_DIR}/libusb
)
