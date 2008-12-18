#ifndef _SDLWINDOW_H
#define _SDLWINDOW_H

#include "GLWindow.h"
#include <GL/glxew.h>
#include <SDL.h>
#include <GL/gl.h>

class SDLWindow : public GLWindow 
{
public:
    int nXoff, nYoff; // screen offset
    // Since it can Stretch to fit Window, we need two different multiplication values
    float MValueX, MValueY; 

    virtual void SwapBuffers();
    virtual void SetWindowText(const char *text);
    virtual bool PeekMessages();
    virtual void Update();
    virtual bool MakeCurrent();
    
    ~SDLWindow();
    SDLWindow(int _iwidth, int _iheight);

};
#endif
