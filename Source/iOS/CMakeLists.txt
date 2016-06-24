list(APPEND LIBS core uicommon)

if(APPLE_IOS)
	file(COPY ${CMAKE_SOURCE_DIR}/Source/iOS/ DESTINATION ${PROJECT_BINARY_DIR})
	file(COPY ${PROJECT_SOURCE_DIR}/Data/Controller/hdpi/ DESTINATION ${PROJECT_BINARY_DIR}/DolphiniOS/Images/Controllers/GameCube/Retina)
endif()