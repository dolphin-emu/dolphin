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
    float FactorW  = 640.0f / (float)GetWidth();
    float FactorH  = 480.0f / (float)GetHeight();
    float Max = (FactorW < FactorH) ? FactorH : FactorW;
    //AR = (float)nBackbufferWidth / (float)nBackbufferHeight;

    if (g_Config.bStretchToFit) {
	MValueX = 1;
	MValueY = 1;
        nXoff = 0;
        nYoff = 0;
    } else {
        MValueX = 1.0f / Max;
        MValueY = 1.0f / Max;
        nXoff = (int)((GetWidth() - (640 * MValueX)) / 2);
        nYoff = (int)((GetHeight() - (480 * MValueY)) / 2);
    }
}

bool WXGLWindow::MakeCurrent() {
    glCanvas->SetCurrent(*glCtxt);
    return true;
}
    
WXGLWindow::~WXGLWindow() {
    delete glCanvas;
    delete frame;
}

WXGLWindow::WXGLWindow(int _iwidth, int _iheight) {
    int _twidth,  _theight;
    if(g_Config.bFullscreen) {
        if(strlen(g_Config.iFSResolution) > 1) {
            sscanf(g_Config.iFSResolution, "%dx%d", &_twidth, &_theight);
        } else {// No full screen reso set, fall back to default reso
            _twidth = _iwidth;
            _theight = _iheight;
        }
    } else {// Going Windowed
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
	nXoff = 0;
	nYoff = 0;
    } else {
	MValueX = 1.0f / Max;
	MValueY = 1.0f / Max;
	nXoff = (int)((_twidth - (640 * MValueX)) / 2);
	nYoff = (int)((_theight - (480 * MValueY)) / 2);
    }

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

