set(NOGUI_SRCS Main.cpp)

if(USE_X11)
	set(NOGUI_SRCS ${NOGUI_SRCS} ../DolphinWX/X11Utils.cpp)
endif()

list(APPEND LIBS core uicommon)

if(APPLE)
	set(DOLPHIN_EXE Dolphin-nogui)
else()
	set(DOLPHIN_EXE dolphin-emu-nogui)
endif()

if(USE_X11 OR ENABLE_HEADLESS)
	add_executable(${DOLPHIN_EXE} ${SRCS} ${NOGUI_SRCS})
	target_link_libraries(${DOLPHIN_EXE} ${LIBS})
	set(CPACK_PACKAGE_EXECUTABLES ${CPACK_PACKAGE_EXECUTABLES} ${DOLPHIN_EXE})
	install(TARGETS ${DOLPHIN_EXE} RUNTIME DESTINATION ${bindir})
endif()
