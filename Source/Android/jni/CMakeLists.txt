list(APPEND LIBS core uicommon)

set(SRCS ButtonManager.cpp
         MainAndroid.cpp)

if(ANDROID)
	set(DOLPHIN_EXE main)
	add_library(${DOLPHIN_EXE} SHARED ${SRCS})
	target_link_libraries(${DOLPHIN_EXE}
	log
	android
	"-Wl,--no-warn-mismatch"
	"-Wl,--whole-archive"
	${LIBS}
	"-Wl,--no-whole-archive"
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND mkdir ARGS -p ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/jniLibs/${ANDROID_NDK_ABI_NAME}
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND mkdir ARGS -p ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND cp ARGS ${LIBRARY_OUTPUT_PATH_ROOT}/libs/${ANDROID_NDK_ABI_NAME}/lib${DOLPHIN_EXE}.so ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/jniLibs/${ANDROID_NDK_ABI_NAME}/
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND cp ARGS ${CMAKE_SOURCE_DIR}/Data/Sys/GC/* ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/
	)
	add_custom_command(TARGET ${DOLPHIN_EXE} POST_BUILD
		COMMAND cp ARGS -r ${CMAKE_SOURCE_DIR}/Data/Sys/Shaders ${CMAKE_SOURCE_DIR}/Source/Android/app/src/main/assets/
	)

	set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${DOLPHIN_EXE})
endif()
