#ifndef _SDLWINDOW_H
#define _SDLWINDOW_H

#include "GLWindow.h"
#if defined HAVE_SDL && HAVE_SDL
#include <SDL.h>

class SDLWindow : public GLWindow 
{
public:
    float MValueX, MValueY; 

    virtual void SwapBuffers();
    virtual void SetWindowText(const char *text);
    virtual bool PeekMessages();
    virtual void Update();
    virtual bool MakeCurrent();
    
    ~SDLWindow();
    SDLWindow(int _iwidth, int _iheight);

};
#else
class SDLWindow : public GLWindow 
{
 public:
    SDLWindow(int _iwidth, int _iheight) {}
};
#endif
#endif
