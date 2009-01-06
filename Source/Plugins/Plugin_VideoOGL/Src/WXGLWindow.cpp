#include "WXGLWindow.h"

void WXGLWindow::SwapBuffers() {
    glCanvas->SwapBuffers();
}

void WXGLWindow::SetWindowText(const char *text) {
    frame->SetTitle(wxString::FromAscii(text));
}

bool WXGLWindow::PeekMessages() {
    // TODO implmenent
    return false;
}

void WXGLWindow::Update() {
    updateDim();
}

bool WXGLWindow::MakeCurrent() {
    glCanvas->SetCurrent(*glCtxt);
    return true;
}
    
WXGLWindow::~WXGLWindow() {
    delete glCanvas;
    delete frame;
}

WXGLWindow::WXGLWindow() : GLWindow() {

    updateDim();

    int args[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 16, 0};

    wxSize size(GetWidth(), GetHeight());
    if (!g_Config.renderToMainframe || 
        g_VideoInitialize.pWindowHandle == NULL) {
        frame = new wxFrame((wxWindow *)g_VideoInitialize.pWindowHandle, 
                                  -1, _("Dolphin"), wxPoint(0,0), size);
    } else {
        frame = new wxFrame((wxWindow *)NULL, 
                                  -1, _("Dolphin"), wxPoint(0,0), size);
    }
    glCanvas = new wxGLCanvas(frame, wxID_ANY, args,
                                    wxPoint(0,0), size, wxSUNKEN_BORDER);
    glCtxt = new wxGLContext(glCanvas);

    frame->Show(TRUE);
    glCanvas->Show(TRUE);

    glCanvas->SetCurrent(*glCtxt);
}

