add_definitions(-DSDL_VIDEO_DISABLED=1)
add_definitions(-DSDL_EVENTS_DISABLED=1)

set(SRCS	src/SDL.c
			src/SDL_error.c
			src/SDL_fatal.c
			src/joystick/SDL_joystick.c)

# TODO: for BSD: add usbhid to libs, add joystick/bsd/SDL_sysjoystick.c, stdlib/SDL_malloc.c and stdlib/SDL_string.c to sources

if(APPLE)
	set(SRCS ${SRCS} src/joystick/darwin/SDL_sysjoystick.c)
elseif(UNIX)
	set(SRCS ${SRCS} src/joystick/linux/SDL_sysjoystick.c)
elseif(WIN32)
	set(SRCS ${SRCS} src/joystick/win32/SDL_mmjoystick.c)
else()
	set(SRCS ${SRCS} src/joystick/dummy/SDL_sysjoystick.c)
endif()

add_library(SDL STATIC ${SRCS})
