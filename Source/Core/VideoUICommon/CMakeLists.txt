set(SRCS	Src/VideoConfigDiag.cpp
			Src/DebuggerPanel.cpp)

add_library(videouicommon STATIC ${SRCS})
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-fPIC)
endif()
