list(APPEND LIBS core uicommon)

set(SRCS ButtonManager.cpp
         MainAndroid.cpp)

set(SHARED_LIB main)
add_library(${SHARED_LIB} SHARED ${SRCS})
target_link_libraries(${SHARED_LIB}
log
android
"-Wl,--no-warn-mismatch"
"-Wl,--whole-archive"
${LIBS}
"-Wl,--no-whole-archive"
)

file(MAKE_DIRECTORY ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/)
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/Sys/)
file(COPY ${CMAKE_SOURCE_DIR}/Data/Sys DESTINATION ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/)
file(REMOVE_RECURSE ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/Sys/Resources/)  # not used on Android

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${SHARED_LIB})
