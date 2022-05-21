include_directories(include)
include_directories(libcharset/include)

set(SRCS	lib/iconv.c
		lib/relocatable.c
		libcharset/lib/localcharset.c
)

add_library(iconv STATIC ${SRCS})
dolphin_disable_warnings_msvc(iconv)
