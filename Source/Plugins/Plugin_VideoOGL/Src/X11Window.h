#ifndef _X11WINDOW_H
#define _X11WINDOW_H

#include "GLWindow.h"
#include <GL/glxew.h>
#include <GL/gl.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

#include <X11/extensions/xf86vmode.h>

class X11Window : public GLWindow 
{
public:
    int screen;
    Window win;
    Display *dpy;
    GLXContext ctx;
    XSetWindowAttributes attr;
    Bool fs;
    Bool doubleBuffered;
    XF86VidModeModeInfo deskMode;
    int nBackbufferWidth, nBackbufferHeight;
    int nXoff, nYoff; // screen offset
    // Since it can Stretch to fit Window, we need two different multiplication values
    float MValueX, MValueY; 

    virtual void SwapBuffers();
    virtual void SetWindowText(const char *text);
    virtual bool PeekMessages();
    virtual void Update();
    virtual bool MakeCurrent();
    
    ~X11Window();
    X11Window(int _iwidth, int _iheight);
};

#endif
