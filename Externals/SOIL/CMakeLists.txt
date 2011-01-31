set(SRCS	image_DXT.c
			image_helper.c
			SOIL.c
			stb_image_aug.c)

add_library(SOIL STATIC ${SRCS})
