add_library(FatFs STATIC
	ff.c
	ffunicode.c
	diskio.h
	ff.h
	ffconf.h
)

target_include_directories(FatFs
PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}
)
