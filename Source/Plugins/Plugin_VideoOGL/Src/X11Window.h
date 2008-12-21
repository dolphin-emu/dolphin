#ifndef _X11WINDOW_H
#define _X11WINDOW_H

#include "GLWindow.h"
#if defined HAVE_X11 && HAVE_X11
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

    virtual void SwapBuffers();
    virtual void SetWindowText(const char *text);
    virtual bool PeekMessages();
    virtual void Update();
    virtual bool MakeCurrent();

    static bool valid() { return true; }
    ~X11Window();
    X11Window(int _iwidth, int _iheight);
};
#else 
class X11Window : public GLWindow 
{
public:
    X11Window(int _iwidth, int _iheight) {}
};
#endif
#endif
