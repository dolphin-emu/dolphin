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
    if (!surface) {
	PanicAlert("Can't ge t surface to update");
	return;
    }
    
    //    SetSize(surface->w, surface->h);
    updateDim();
	
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
	PanicAlert("Couldn't get video info");
	SDL_Quit();
	return false;
    }
    // Compute video mode flags.
    const int videoFlags = SDL_OPENGL
	| ( videoInfo->hw_available ? SDL_HWSURFACE : SDL_SWSURFACE )
	| ( g_Config.bFullscreen ? SDL_FULLSCREEN : 0);
    // Set vide mode.
    // TODO: Can we use this field or is a separate field needed?
    SDL_Surface *screen = SDL_SetVideoMode(GetXwin(), GetYwin(), 
					   0, videoFlags);
    if (!screen) {
	PanicAlert("Couldn't set video mode");
	SDL_Quit();
	return false;
    }

    return true;
}

SDLWindow::~SDLWindow() {
    SDL_Quit();
}

SDLWindow::SDLWindow() : GLWindow() {

    //init sdl video
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
	PanicAlert("Failed to init SDL: %s", SDL_GetError());
	SDL_Quit();
	//	return NULL;
    }
    
    //setup ogl to use double buffering
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
}
