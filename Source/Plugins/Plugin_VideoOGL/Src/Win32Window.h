#ifndef _WIN32WINDOW_H
#define _WIN32WINDOW_H

#include "GLWindow.h"

#ifdef _WIN32
class Win32Window : public GLWindow 
{
    virtual void SwapBuffers();
    virtual void SetWindowText(const char *text);
    virtual bool PeekMessages();
    virtual void Update();
    virtual bool MakeCurrent();
    
    static bool valid() { return true; }
    ~Win32Window();
    Win32Window();
    static sf::Key::Code KeysymToSF(KeySym Sym);
};

#else

class Win32Window : public GLWindow 
{
 public:
    Win32Window {}
};

#endif
