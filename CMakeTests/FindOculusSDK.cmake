# - Find Oculus SDK library
# This module defines
#  OCULUSSDK_INCLUDE_DIR, usually LibOVR/Include
#  OCULUSSDK_SRC_DIR, usually LibOVR/Src
#  OCULUSSDK_FOUND, If false, do not try to use Oculus SDK.

if (NOT OCULUSSDK_FOUND)
	#pkg_check_modules (LIBUSB_PKG libusb-1.0)

	find_path(OCULUSSDK_INCLUDE_DIR NAMES OVR.h
		PATHS
		/usr/include/ovr-0.4.3/LibOVR/Include
		/usr/include/ovr-0.4.4/LibOVR/Include
		/usr/include/ovr-0.5.0.1/LibOVR/Include
		/usr/include
		/usr/local/include/ovr-0.4.3/LibOVR/Include
		/usr/local/include/ovr-0.4.4/LibOVR/Include
		/usr/local/include/ovr-0.5.0.1/LibOVR/Include
		/usr/local/include
		Externals/LibOVR/Include
		Externals/
	)

	find_path(OCULUSSDK_SRC_DIR NAMES OVR_CAPI_GL.h
		PATHS
		/usr/include/ovr-0.4.3/LibOVR/Src
		/usr/include/ovr-0.4.4/LibOVR/Src
		/usr/include/ovr-0.5.0.1/LibOVR/Include
		/usr/include
		/usr/local/include/ovr-0.4.3/LibOVR/Src
		/usr/local/include/ovr-0.4.4/LibOVR/Src
		/usr/local/include/ovr-0.5.0.1/LibOVR/Src
		/usr/local/include
		Externals/LibOVR/Src
		Externals/
	)

	if(OCULUSSDK_INCLUDE_DIR AND OCULUSSDK_SRC_DIR)
		set(OCULUSSDK_FOUND TRUE CACHE INTERNAL "Oculus SDK found")
		message(STATUS "Found Oculus SDK: ${OCULUSSDK_INCLUDE_DIR}, ${OCULUSSDK_SRC_DIR}")
	else()
		set(LIBUSB_FOUND FALSE CACHE INTERNAL "Oculus SDK found")
		message(STATUS "Oculus SDK not found.")
	endif(OCULUSSDK_INCLUDE_DIR AND OCULUSSDK_SRC_DIR)

	mark_as_advanced(OCULUSSDK_INCLUDE_DIR OCULUSSDK_SRC_DIR)
endif ()

