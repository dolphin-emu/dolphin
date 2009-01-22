#include "InputManager.h"

bool InputManager::Init() {
#ifdef HAVE_SDL
    // Move also joystick opening code here.
    if (! sdlInit) {
	/* SDL 1.3 use DirectInput instead of the old Microsoft Multimeda API,
	   and with this we need the SDL_INIT_VIDEO flag as well */
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
	    PanicAlert("Could not initialize SDL! (%s)\n", SDL_GetError());
	} else {
	    sdlInit = true;
	}
    }
#endif

    return true;
}

bool InputManager::Shutdown() {
#ifdef HAVE_SDL
    SDL_Quit();
    sdlInit = false;
#endif

    return true;
}
