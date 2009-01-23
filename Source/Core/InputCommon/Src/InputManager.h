#ifndef INPUTMANAGER_H
#define INPUTMANAGER_H
#include "Common.h"

#if defined HAVE_SDL && HAVE_SDL
#include <SDL.h>


struct ControllerInfo {		// CONNECTED WINDOWS DEVICES INFO
	int NumAxes;			// Amount of Axes
	int NumButtons;			// Amount of Buttons
	int NumBalls;			// Amount of Balls
	int NumHats;			// Amount of Hats (POV)
	const char *Name;		// Joypad/stickname
	int ID;				// SDL joystick device ID
	SDL_Joystick *joy;		// SDL joystick device
};
#endif

class InputManager {

public:
    bool Init();
    bool Shutdown();

    InputManager(): sdlInit(false) {}
    ~InputManager() {   
	if(m_joyinfo)
	    delete [] m_joyinfo;
    }

private:
    bool sdlInit;
    int ScanDevices();
    
    // sdl specific
    bool SDLInit();
    void SDLShutdown();
    int SDLScanDevices();
    ControllerInfo *m_joyinfo;
    int numjoy;
};
#endif
