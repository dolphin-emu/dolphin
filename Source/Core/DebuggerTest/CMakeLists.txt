set (SRCS Main.cpp)

set (LIBS ${LIBS}
          uicommon
          common
          core
          videocommon
          videoogl
          videosoftware)

if (ANDROID)
	set (LIBS ${LIBS}
	          log
	          android)
endif()

add_executable(DebuggerTest ${SRCS})
target_link_libraries(DebuggerTest ${LIBS} sfml-network sfml-system)
