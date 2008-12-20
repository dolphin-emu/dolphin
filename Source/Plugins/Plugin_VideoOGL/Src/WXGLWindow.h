#ifndef _WXGLWINDOW_H
#define _WXGLWINDOW_H

#include "GLWindow.h"
#if defined USE_WX && USE_WX
#include "wx/wx.h"
#include "wx/glcanvas.h"

class WXGLWindow : public GLWindow 
{
private:
    wxGLCanvas *glCanvas;
    wxFrame *frame;
    wxGLContext *glCtxt;

public:

    float MValueX, MValueY; 

    virtual void SwapBuffers();
    virtual void SetWindowText(const char *text);
    virtual bool PeekMessages();
    virtual void Update();
    virtual bool MakeCurrent();
    
    ~WXGLWindow();
    WXGLWindow(int _iwidth, int _iheight);

};
#else 
class WXGLWindow : public GLWindow 
{
 public:
    WXGLWindow(int _iwidth, int _iheight) {}
};
#endif
#endif
