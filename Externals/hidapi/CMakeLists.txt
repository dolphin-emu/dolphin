dolphin_fetchcontent_declare(
  hidapi
  GIT_REPOSITORY https://github.com/libusb/hidapi.git
  GIT_TAG d6b2a974608dec3b76fb1e36c189f22b9cf3650c # hidapi-0.15.0
  GIT_SHALLOW TRUE
)

set(BUILD_SHARED_LIBS OFF)
set(HIDAPI_USE_ICONV OFF CACHE BOOL "" FORCE)

if(LINUX)
  find_package(LIBUDEV QUIET)
  if(NOT LIBUDEV_FOUND)
    set(HIDAPI_WITH_HIDRAW OFF)
  endif()
endif()

if(TARGET LibUSB::LibUSB)
  if(NOT TARGET usb-1.0)
    add_library(usb-1.0 INTERFACE)
    target_link_libraries(usb-1.0 INTERFACE LibUSB::LibUSB)
  endif()
endif()

FetchContent_MakeAvailable(hidapi)
