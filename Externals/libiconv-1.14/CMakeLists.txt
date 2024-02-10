add_library(iconv STATIC
	lib/iconv.c
	lib/relocatable.c
	libcharset/lib/localcharset.c
)
target_include_directories(iconv
	PUBLIC
		include
	PRIVATE
		libcharset/include
)
dolphin_disable_warnings_msvc(iconv)
add_library(Iconv::Iconv ALIAS iconv)
