// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include <wx/wx.h>
#include <wx/filepicker.h>
#include <wx/notebook.h>
#include <wx/dialog.h>
#include <wx/aboutdlg.h>

#include "Globals.h"

#include "GUI/ConfigDlg.h"

#include "Render.h"
#include "GLInit.h"
#include "Fifo.h"
#include "OpcodeDecoding.h"
#include "TextureMngr.h"
#include "BPStructs.h"

#ifdef _WIN32
#include "OS\Win32.h"
#else
//#include "Linux/Linux.h"
#endif
#include "VertexLoader.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"




HINSTANCE g_hInstance = NULL;
SVideoInitialize g_VideoInitialize;
#define VERSION_STRING "0.1"

class wxDLLApp : public wxApp
{
        bool OnInit()
        {
                return true;
        }
};
IMPLEMENT_APP_NO_MAIN(wxDLLApp) 

WXDLLIMPEXP_BASE void wxSetInstance(HINSTANCE hInst);

#ifdef _WIN32
BOOL APIENTRY DllMain(HINSTANCE hinstDLL,	// DLL module handle
                      DWORD dwReason,		// reason called
                      LPVOID lpvReserved)	// reserved
{
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
		{       //use wxInitialize() if you don't want GUI instead of the following 12 lines
			wxSetInstance((HINSTANCE)hinstDLL);
			int argc = 0;
			char **argv = NULL;
			wxEntryStart(argc, argv);
			if ( !wxTheApp || !wxTheApp->CallOnInit() )
				return FALSE;
		}
		break; 

    case DLL_PROCESS_DETACH:
        CloseConsole();
		wxEntryCleanup(); //use wxUninitialize() if you don't want GUI 
        break;
    default:
        break;
    }

    g_hInstance = hinstDLL;
    return TRUE;
}
#endif
void SysMessage(const char *fmt, ...) {
    va_list list;
    char msg[512];

    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);

    if (msg[strlen(msg)-1] == '\n') msg[strlen(msg)-1] = 0;
    wxMessageBox(wxString::FromAscii(msg));
}

void GetDllInfo (PLUGIN_INFO* _PluginInfo) 
{
    _PluginInfo->Version = 0x0100;
    _PluginInfo->Type = PLUGIN_TYPE_VIDEO;
#ifdef DEBUGFAST 
    sprintf(_PluginInfo->Name, "Dolphin OGL (DebugFast) " VERSION_STRING);
#else
#ifndef _DEBUG
    sprintf(_PluginInfo->Name, "Dolphin OGL " VERSION_STRING);
#else
    sprintf(_PluginInfo->Name, "Dolphin OGL (Debug) " VERSION_STRING);
#endif
#endif
}

void DllAbout(HWND _hParent) 
{
	wxAboutDialogInfo info;
	info.AddDeveloper(_T("zerofrog(@gmail.com)"));
	info.SetDescription(_T("Vertex/Pixel Shader 2.0 or higher, framebuffer objects, multiple render targets"));
	wxAboutBox(info);
}

void DllConfig(HWND _hParent)
{
    #ifdef _WIN32
	wxWindow win;
	win.SetHWND((WXHWND)_hParent);
	win.Enable(false);  

	ConfigDialog frame(&win);
	frame.ShowModal();
	#else
	ConfigDialog frame(NULL);
	frame.ShowModal();
	#endif

/*	wxWindow win;
	win.SetHWND((WXHWND)_hParent);
	win.Enable(false);  

	ConfigDialog frame(&win);
	frame.ShowModal();

	win.Enable(true);
	win.SetHWND(0); */
}

void Video_Initialize(SVideoInitialize* _pVideoInitialize)
{
    if (_pVideoInitialize == NULL)
        return;

#ifdef _WIN32
//    OpenConsole();
#endif
   
    frameCount = 0;
    g_VideoInitialize = *_pVideoInitialize;
    InitLUTs();
    g_Config.Load();

    if (!OpenGL_Create(g_VideoInitialize, g_Res[g_Config.iWindowedRes][0], g_Res[g_Config.iWindowedRes][1])) {
        g_VideoInitialize.pLog("Renderer::Create failed\n", TRUE);
        return;
    }
    _pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
    _pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;
    _pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;
}

#ifdef _WIN32
HANDLE g_hthread;
#endif

void Video_Prepare(void)
{
	OpenGL_MakeCurrent();
	if (!Renderer::Create2()) {
        g_VideoInitialize.pLog("Renderer::Create2 failed\n", TRUE);
        return;
	}

    TextureMngr::Init();

    BPInit();
    VertexManager::Init();
    OpcodeDecoder_Init();
    Fifo_Init();
    VertexShaderMngr::Init();
    PixelShaderMngr::Init();
    GL_REPORT_ERRORD();
}
 
void Video_Shutdown(void) 
{
    VertexShaderMngr::Shutdown();
    PixelShaderMngr::Shutdown();
    Fifo_Shutdown();
    VertexManager::Destroy();
    TextureMngr::Shutdown();
    OpcodeDecoder_Shutdown();
    Renderer::Shutdown();
	OpenGL_Shutdown();
}


void DebugLog(const char* _fmt, ...)
{
#ifdef _DEBUG
    
    char* Msg = (char*)alloca(strlen(_fmt)+512);
    va_list ap;

    va_start( ap, _fmt );
    vsnprintf( Msg, strlen(_fmt)+512, _fmt, ap );
    va_end( ap );

    g_VideoInitialize.pLog(Msg, FALSE);
#endif
}

bool ScreenShot(TCHAR *File) 
{
    char str[64];
    int left = 200, top = 15;
    sprintf(str, "Dolphin OGL " VERSION_STRING);

    Renderer::ResetGLState();
//    Renderer::DrawText(str, left+1, top+1, 0xff000000);
//    Renderer::DrawText(str, left, top, 0xffc0ffff);
    Renderer::RestoreGLState();

    if (Renderer::SaveRenderTarget(File, 0)) {
        char str[255];
        sprintf(str, "saved %s\n", File);
        Renderer::AddMessage(str, 500);
    	return true;
    }
	return false;
}

BOOL Video_Screenshot(TCHAR* _szFilename)
{
    if (ScreenShot(_szFilename))
        return TRUE;

    return FALSE;
}

void Video_UpdateXFB(BYTE* _pXFB, DWORD _dwWidth, DWORD _dwHeight)
{
}
