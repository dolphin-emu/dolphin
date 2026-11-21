function(dolphin_add_win_resources TARGET DESCRIPTION)
  set(DOLPHIN_RC_ICON "${PROJECT_SOURCE_DIR}/Data/Dolphin.ico")
  set(DOLPHIN_RC_DESCRIPTION "${DESCRIPTION}")
  set(DOLPHIN_RC_FILENAME "$<TARGET_FILE_NAME:${TARGET}>")
  if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(DOLPHIN_RC_FILEFLAGS "VS_FF_DEBUG | VS_FF_PRERELEASE")
  else()
    set(DOLPHIN_RC_FILEFLAGS "0x0L")
  endif()

  set(rc_out "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}.rc")

  file(READ "${PROJECT_SOURCE_DIR}/Data/Dolphin.rc.in" contents)
  string(CONFIGURE "${contents}" contents @ONLY)
  file(GENERATE OUTPUT "${rc_out}" CONTENT "${contents}" TARGET ${TARGET})

  add_dependencies(${TARGET} dolphin_scmrev)
  set_source_files_properties("${rc_out}" PROPERTIES
    OBJECT_DEPENDS "${PROJECT_BINARY_DIR}/Source/Core/Common/scmrev.h"
  )

  target_sources(${TARGET} PRIVATE "${rc_out}")
endfunction()
