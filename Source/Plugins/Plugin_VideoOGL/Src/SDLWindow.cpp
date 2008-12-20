#include "SDLWindow.h"

void SDLWindow::SwapBuffers() {
    SDL_GL_SwapBuffers();
}

void SDLWindow::SetWindowText(const char *text) {
    SDL_WM_SetCaption(text, NULL);
}

bool SDLWindow::PeekMessages() {
    // TODO implement
    return false;
}

void SDLWindow::Update() {

    SDL_Surface *surface = SDL_GetVideoSurface();
    if (!surface) return;

    float FactorW  = 640.0f / (float)surface->w;
    float FactorH  = 480.0f / (float)surface->h;
    float Max = (FactorW < FactorH) ? FactorH : FactorW;
    //    AR = (float)surface->w / (float)surface->h;;
    
    if (g_Config.bStretchToFit) {
	MValueX = 1;
	MValueY = 1;
	SetOffset(0,0);
    } else {
        MValueX = 1.0f / Max;
        MValueY = 1.0f / Max;
	SetOffset((int)((surface->w - (640 * MValueX)) / 2),
		  (int)((surface->h - (480 * MValueY)) / 2));
    }
    SetSize(surface->w, surface->h);
    
}

bool SDLWindow::MakeCurrent() {
    /* Note: The reason for having the call to SDL_SetVideoMode in here instead
       of in OpenGL_Create() is that "make current" is part of the video
       mode setting and is not available as a separate call in SDL. We
       have to do "make current" here because this method runs in the CPU
       thread while OpenGL_Create() runs in a diferent thread and "make
       current" has to be done in the same thread that will be making
       calls to OpenGL. */
 
    // Fetch video info.
    const SDL_VideoInfo *videoInfo = SDL_GetVideoInfo();
    if (!videoInfo) {
	// TODO: Display an error message.
	SDL_Quit();
	return false;
    }
    // Compute video mode flags.
    const int videoFlags = SDL_OPENGL
	| ( videoInfo->hw_available ? SDL_HWSURFACE : SDL_SWSURFACE )
	| ( g_Config.bFullscreen ? SDL_FULLSCREEN : 0);
    // Set vide mode.
    // TODO: Can we use this field or is a separate field needed?
    SDL_Surface *screen = SDL_SetVideoMode(GetWidth(), GetHeight(), 
					   0, videoFlags);
    if (!screen) {
	//TODO : Display an error message
	SDL_Quit();
	return false;
    }

    return true;
}

SDLWindow::~SDLWindow() {
    SDL_Quit();
}

SDLWindow::SDLWindow(int _iwidth, int _iheight) {
    int _twidth,  _theight;
    if(g_Config.bFullscreen) {
	if(strlen(g_Config.iFSResolution) > 1) {
            sscanf(g_Config.iFSResolution, "%dx%d", &_twidth, &_theight);
        } else { // No full screen reso set, fall back to default reso
            _twidth = _iwidth;
            _theight = _iheight;
        }
    } else { // Going Windowed
        if(strlen(g_Config.iWindowedRes) > 1) {
            sscanf(g_Config.iWindowedRes, "%dx%d", &_twidth, &_theight);
        } else {// No Window reso set, fall back to default
            _twidth = _iwidth;
            _theight = _iheight;
        }
    }

    SetSize(_iwidth, _theight);

    float FactorW  = 640.0f / (float)_twidth;
    float FactorH  = 480.0f / (float)_theight;
    float Max = (FactorW < FactorH) ? FactorH : FactorW;
    
    if(g_Config.bStretchToFit) {
	MValueX = 1.0f / FactorW;
	MValueY = 1.0f / FactorH;
	SetOffset(0,0);
    } else {
	MValueX = 1.0f / Max;
	MValueY = 1.0f / Max;
	SetOffset((int)((_twidth - (640 * MValueX)) / 2),
		  (int)((_theight - (480 * MValueY)) / 2));
    }

    //init sdl video
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	//TODO : Display an error message
	SDL_Quit();
	//	return NULL;
    }
    
    //setup ogl to use double buffering
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}
