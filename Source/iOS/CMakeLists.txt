list(APPEND LIBS core uicommon)

if(APPLE_IOS)
	file(COPY ${CMAKE_SOURCE_DIR}/Source/iOS/ DESTINATION ${PROJECT_BINARY_DIR})
endif()