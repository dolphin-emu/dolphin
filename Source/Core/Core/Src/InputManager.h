#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H
#include "Common.h"

#if defined HAVE_SDL && HAVE_SDL
#include <SDL.h>
#endif

class InputManager {

public:
    bool Init();
    bool Shutdown();

    InputManager(): sdlInit(false) {}
    ~InputManager() {}

private:
    bool sdlInit;
};
#endif
