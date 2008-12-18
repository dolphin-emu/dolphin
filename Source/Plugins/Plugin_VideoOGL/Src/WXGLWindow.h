#ifndef _WXGLWINDOW_H
#define _WXGLWINDOW_H

#include "GLWindow.h"
#include "wx/wx.h"
#include "wx/glcanvas.h"

class WXGLWindow : public GLWindow 
{
private:
    wxGLCanvas *glCanvas;
    wxFrame *frame;
    wxGLContext *glCtxt;

public:

    int nXoff, nYoff; // screen offset
    // Since it can Stretch to fit Window, we need two different multiplication values
    float MValueX, MValueY; 

    virtual void SwapBuffers();
    virtual void SetWindowText(const char *text);
    virtual bool PeekMessages();
    virtual void Update();
    virtual bool MakeCurrent();
    
    ~WXGLWindow();
    WXGLWindow(int _iwidth, int _iheight);

};
#endif
