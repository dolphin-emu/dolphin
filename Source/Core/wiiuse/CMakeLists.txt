set(SRCS	Src/ir.cpp
			Src/wiiuse.cpp)

if(APPLE)
	set(SRCS ${SRCS} Src/io_osx.m)
elseif(UNIX AND BLUEZ_FOUND)
	set(SRCS ${SRCS} Src/io_nix.cpp)
	set(LIBS ${LIBS} bluetooth)
elseif(WIN32)
	set(SRCS ${SRCS} Src/io_win.cpp)
else()
	set(SRCS ${SRCS} Src/io_dummy.cpp)
endif()

add_library(wiiuse STATIC ${SRCS})
target_link_libraries(wiiuse ${LIBS})
