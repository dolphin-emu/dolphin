set(SRCS	Src/ir.c
			Src/wiiuse.c)

if(APPLE)
	set(SRCS ${SRCS} Src/io_osx.m)
elseif(UNIX)
	set(SRCS ${SRCS} Src/io_nix.c)
	set(LIBS ${LIBS} bluetooth)
elseif(WIN32)
	set(SRCS ${SRCS} Src/io_win.c)
endif()

add_library(wiiuse STATIC ${SRCS})
target_link_libraries(wiiuse ${LIBS})
