# FindFFmpeg
# ----------
#
# Find the native FFmpeg includes and libraries
#
# This module defines the following variables:
#
#  FFmpeg_INCLUDE_<component>: where to find <component>.h
#  FFmpeg_LIBRARY_<component>: where to find the <component> library
#  FFmpeg_INCLUDES: aggregate all the include paths
#  FFmpeg_LIBRARIES: aggregate all the paths to the libraries
#  FFmpeg_FOUND: True if all components have been found
#
# This module defines the following targets, which are prefered over variables:
#
#  FFmpeg::<component>: Target to use <component> directly, with include path,
#    library and dependencies set up. If you are using a static build, you are
#    responsible for adding any external dependencies (such as zlib, bzlib...).
#
# <component> can be one of:
#   avcodec
#   avdevice
#   avfilter
#   avformat
#   postproc
#   swresample
#   swscale
#

set(_FFmpeg_ALL_COMPONENTS
  avcodec
  avdevice
  avfilter
  avformat
  avutil
  postproc
  swresample
  swscale
)

set(_FFmpeg_DEPS_avcodec avutil)
set(_FFmpeg_DEPS_avdevice avcodec avformat avutil)
set(_FFmpeg_DEPS_avfilter avutil)
set(_FFmpeg_DEPS_avformat avcodec avutil)
set(_FFmpeg_DEPS_postproc avutil)
set(_FFmpeg_DEPS_swresample avutil)
set(_FFmpeg_DEPS_swscale avutil)

function(find_ffmpeg LIBNAME)
  if(DEFINED ENV{FFMPEG_DIR})
    set(FFMPEG_DIR $ENV{FFMPEG_DIR})
  endif()

  if(FFMPEG_DIR)
    list(APPEND INCLUDE_PATHS
      ${FFMPEG_DIR}
      ${FFMPEG_DIR}/ffmpeg
      ${FFMPEG_DIR}/lib${LIBNAME}
      ${FFMPEG_DIR}/include/lib${LIBNAME}
      ${FFMPEG_DIR}/include/ffmpeg
      ${FFMPEG_DIR}/include
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )
    list(APPEND LIB_PATHS
      ${FFMPEG_DIR}
      ${FFMPEG_DIR}/lib
      ${FFMPEG_DIR}/lib${LIBNAME}
      NO_DEFAULT_PATH
      NO_CMAKE_FIND_ROOT_PATH
    )
  else()
    list(APPEND INCLUDE_PATHS
      /usr/local/include/ffmpeg
      /usr/local/include/lib${LIBNAME}
      /usr/include/ffmpeg
      /usr/include/lib${LIBNAME}
      /usr/include/ffmpeg/lib${LIBNAME}
    )

    list(APPEND LIB_PATHS
      /usr/local/lib
      /usr/lib
    )
  endif()

  find_path(FFmpeg_INCLUDE_${LIBNAME} lib${LIBNAME}/${LIBNAME}.h
    HINTS ${INCLUDE_PATHS}
  )

  find_library(FFmpeg_LIBRARY_${LIBNAME} ${LIBNAME}
    HINTS ${LIB_PATHS}
  )

  if(NOT FFMPEG_DIR AND (NOT FFmpeg_LIBRARY_${LIBNAME} OR NOT FFmpeg_INCLUDE_${LIBNAME}))
    # Didn't find it in the usual paths, try pkg-config
    find_package(PkgConfig QUIET)
    pkg_check_modules(FFmpeg_PKGCONFIG_${LIBNAME} QUIET lib${LIBNAME})

    find_path(FFmpeg_INCLUDE_${LIBNAME} lib${LIBNAME}/${LIBNAME}.h
      ${FFmpeg_PKGCONFIG_${LIBNAME}_INCLUDE_DIRS}
    )

    find_library(FFmpeg_LIBRARY_${LIBNAME} ${LIBNAME}
      ${FFmpeg_PKGCONFIG_${LIBNAME}_LIBRARY_DIRS}
    )
  endif()

  if(FFmpeg_INCLUDE_${LIBNAME} AND FFmpeg_LIBRARY_${LIBNAME})
    set(FFmpeg_INCLUDE_${LIBNAME} "${FFmpeg_INCLUDE_${LIBNAME}}" PARENT_SCOPE)
    set(FFmpeg_LIBRARY_${LIBNAME} "${FFmpeg_LIBRARY_${LIBNAME}}" PARENT_SCOPE)
    set(FFmpeg_${c}_FOUND TRUE PARENT_SCOPE)
    if(NOT FFmpeg_FIND_QUIETLY)
      message("--  Found ${LIBNAME}: ${FFmpeg_INCLUDE_${LIBNAME}} ${FFmpeg_LIBRARY_${LIBNAME}}")
    endif()
  endif()
endfunction()

foreach(c ${_FFmpeg_ALL_COMPONENTS})
  find_ffmpeg(${c})
endforeach()

foreach(c ${_FFmpeg_ALL_COMPONENTS})
  if(FFmpeg_${c}_FOUND)
    list(APPEND FFmpeg_INCLUDES ${FFmpeg_INCLUDE_${c}})
    list(APPEND FFmpeg_LIBRARIES ${FFmpeg_LIBRARY_${c}})

    add_library(FFmpeg::${c} IMPORTED UNKNOWN)
    set_target_properties(FFmpeg::${c} PROPERTIES
      IMPORTED_LOCATION ${FFmpeg_LIBRARY_${c}}
      INTERFACE_INCLUDE_DIRECTORIES ${FFmpeg_INCLUDE_${c}}
    )
    if(_FFmpeg_DEPS_${c})
      set(deps)
      foreach(dep ${_FFmpeg_DEPS_${c}})
        list(APPEND deps FFmpeg::${dep})
      endforeach()

      set_target_properties(FFmpeg::${c} PROPERTIES
        INTERFACE_LINK_LIBRARIES "${deps}"
      )
      unset(deps)
    endif()
  endif()
endforeach()

if(FFmpeg_INCLUDES)
  list(REMOVE_DUPLICATES FFmpeg_INCLUDES)
endif()

foreach(c ${FFmpeg_FIND_COMPONENTS})
  list(APPEND _FFmpeg_REQUIRED_VARS FFmpeg_INCLUDE_${c} FFmpeg_LIBRARY_${c})
endforeach()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(FFmpeg
  REQUIRED_VARS ${_FFmpeg_REQUIRED_VARS}
  HANDLE_COMPONENTS
)

foreach(c ${_FFmpeg_ALL_COMPONENTS})
  unset(_FFmpeg_DEPS_${c})
endforeach()
unset(_FFmpeg_ALL_COMPONENTS)
unset(_FFmpeg_REQUIRED_VARS)
