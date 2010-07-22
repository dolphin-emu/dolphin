#include <sys/param.h>

#if defined __APPLE__
#include "include/SDL_config_macosx.h"
#elif defined BSD4_4
#include "SDL/SDL_config_bsd.h"
#elif defined __linux__
#include "SDL/SDL_config_linux.h"
#endif

#define SDL_AUDIO_DISABLED 1
#define SDL_CDROM_DISABLED 1
#define SDL_CPUINFO_DISABLED 1
#define SDL_EVENTS_DISABLED 1
#define SDL_FILE_DISABLED 1
#undef SDL_JOYSTICK_DISABLED
#define SDL_LOADSO_DISABLED 1
#define SDL_THREADS_DISABLED 1
#define SDL_TIMERS_DISABLED 1
#define SDL_VIDEO_DISABLED 1

