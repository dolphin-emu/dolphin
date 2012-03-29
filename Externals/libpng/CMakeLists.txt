set(SRCS
	"png.c"
	"pngerror.c"
	"pngget.c"
	"pngmem.c"
	"pngpread.c"
	"pngread.c"
	"pngrio.c"
	"pngrtran.c"
	"pngrutil.c"
	"pngset.c"
	"pngtrans.c"
	"pngwio.c"
	"pngwrite.c"
	"pngwtran.c"
	"pngwutil.c"
	)

add_library(png STATIC ${SRCS})
