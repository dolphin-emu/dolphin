set(SRCS	image_DXT.c
			image_helper.c
			SOIL.c
			stb_image_aug.c)

add_library(SOIL STATIC ${SRCS})
if(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
	add_definitions(-fPIC)
endif()
