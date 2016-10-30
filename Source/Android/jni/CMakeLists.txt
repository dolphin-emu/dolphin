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
file(COPY ${CMAKE_SOURCE_DIR}/Data/Sys/GameSettings ${CMAKE_SOURCE_DIR}/Data/Sys/GC ${CMAKE_SOURCE_DIR}/Data/Sys/Wii ${CMAKE_SOURCE_DIR}/Data/Sys/Shaders DESTINATION ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${SHARED_LIB})
