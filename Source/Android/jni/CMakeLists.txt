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
add_custom_command(TARGET ${SHARED_LIB} POST_BUILD
	COMMAND mkdir ARGS -p ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/jniLibs/${ANDROID_NDK_ABI_NAME}
)
add_custom_command(TARGET ${SHARED_LIB} POST_BUILD
	COMMAND mkdir ARGS -p ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/
)
add_custom_command(TARGET ${SHARED_LIB} POST_BUILD
	COMMAND cp ARGS ${LIBRARY_OUTPUT_PATH_ROOT}/libs/${ANDROID_NDK_ABI_NAME}/lib${SHARED_LIB}.so ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/jniLibs/${ANDROID_NDK_ABI_NAME}/
)
add_custom_command(TARGET ${SHARED_LIB} POST_BUILD
	COMMAND ${CMAKE_STRIP} ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/jniLibs/${ANDROID_NDK_ABI_NAME}/lib${SHARED_LIB}.so
)
add_custom_command(TARGET ${SHARED_LIB} POST_BUILD
	COMMAND cp ARGS ${CMAKE_SOURCE_DIR}/Data/Sys/GC/* ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/
)
add_custom_command(TARGET ${SHARED_LIB} POST_BUILD
	COMMAND cp ARGS -r ${CMAKE_SOURCE_DIR}/Data/Sys/Shaders ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/
)

set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${SHARED_LIB})
