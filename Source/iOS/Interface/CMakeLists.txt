add_library(main STATIC
  MainiOS.mm
)

target_link_libraries(main
PRIVATE
  core
  uicommon
)

target_link_libraries(main
PRIVATE
${COCOA_TOUCH_LIBRARY}
${OPENGL_LIBRARY}
)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} main)

