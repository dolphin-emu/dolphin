#
# Try to find DRM library and include path.
# Once done this will define
#
# DRM_FOUND
# DRM_INCLUDE_PATH
# DRM_LIBRARY
#

FIND_PATH(DRM_INCLUDE_PATH
  NAMES
  drm.h
  PATHS
  ${CMAKE_INCLUDE_PATH}/include/libdrm/
  ~/include/libdrm/
  /usr/include/libdrm/
  /usr/local/include/libdrm/
  /sw/include/libdrm/
  /opt/local/include/libdrm/
  DOC "The directory where drm.h resides")
FIND_LIBRARY(DRM_LIBRARY
  NAMES DRM drm
  PATHS
  ${CMAKE_LIBRARY_PATH}/lib/
  ~/lib/
  /usr/lib64
  /usr/lib
  /usr/local/lib64
  /usr/local/lib
  /sw/lib
  /opt/local/lib
  DOC "The DRM library")

IF(DRM_INCLUDE_PATH)
  INCLUDE_DIRECTORIES(${DRM_INCLUDE_PATH})
  SET(DRM_FOUND 1 CACHE STRING "Set to 1 if DRM is found, 0 otherwise")
ELSE(DRM_INCLUDE_PATH)
  SET(DRM_FOUND 0 CACHE STRING "Set to 1 if DRM is found, 0 otherwise")
ENDIF(DRM_INCLUDE_PATH)

MARK_AS_ADVANCED(DRM_FOUND)
