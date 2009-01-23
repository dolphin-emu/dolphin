#include "InputManager.h"

bool InputManager::Init() {

    if (! SDLInit())
	return false;
    
    ScanDevices();
    return true;
}

bool InputManager::Shutdown() {
    SDLShutdown();
    sdlInit = false;

    return true;
}

bool InputManager::SDLInit() {
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
    return sdlInit;
#endif
    
    return true;
}
void InputManager::SDLShutdown() {
    for(int i = 0; i < numjoy; i++ ) {
	if (SDL_JoystickOpened(m_joyinfo[i].ID))
	    SDL_JoystickClose(m_joyinfo[i].joy);
    }
    SDL_Quit();
}
    
int InputManager::ScanDevices() {

    int res = SDLScanDevices();
    return res;
}

int InputManager::SDLScanDevices() {
#if defined HAVE_SDL && HAVE_SDL 
    numjoy = SDL_NumJoysticks();
	
    if(numjoy == 0) {	
	PanicAlert("No Joystick detected!\n");
	return 0;
    }

    if(m_joyinfo)
	delete [] m_joyinfo;

    m_joyinfo = new ControllerInfo[numjoy];
    
#ifdef _DEBUG
    fprintf(pFile, "Scanning for devices\n");
    fprintf(pFile, "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯\n");
#endif

    for(int i = 0; i < numjoy; i++ ) {
	m_joyinfo[i].joy			= SDL_JoystickOpen(i);
	m_joyinfo[i].ID			= i;
	m_joyinfo[i].NumAxes		= SDL_JoystickNumAxes(m_joyinfo[i].joy);
	m_joyinfo[i].NumButtons	= SDL_JoystickNumButtons(m_joyinfo[i].joy);
	m_joyinfo[i].NumBalls		= SDL_JoystickNumBalls(m_joyinfo[i].joy);
	m_joyinfo[i].NumHats		= SDL_JoystickNumHats(m_joyinfo[i].joy);
	m_joyinfo[i].Name			= SDL_JoystickName(i);
	
	printf("ID: %d\n", i);
	printf("Name: %s\n", m_joyinfo[i].Name);
	printf("Buttons: %d\n", m_joyinfo[i].NumButtons);
	printf("Axises: %d\n", m_joyinfo[i].NumAxes);
	printf("Hats: %d\n", m_joyinfo[i].NumHats);
	printf("Balls: %d\n\n", m_joyinfo[i].NumBalls);
	
	// Close if opened
	if(SDL_JoystickOpened(i))
	    SDL_JoystickClose(m_joyinfo[i].joy);
    }

    return numjoy;
#else
    return 0;
#endif
}
