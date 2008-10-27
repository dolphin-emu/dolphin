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

#include "Globals.h"

#ifdef _WIN32
#include "OS/Win32.h"
#endif

#if defined(__APPLE__) && !defined(OSX64)
#include <SDL.h>
#endif

#if !defined(OSX64)
#include "GUI/ConfigDlg.h"
#endif

#include "Config.h"
#include "LookUpTables.h"
#include "ImageWrite.h"
#include "Render.h"
#include "GLUtil.h"
#include "Fifo.h"
#include "OpcodeDecoding.h"
#include "TextureMngr.h"
#include "BPStructs.h"
#include "VertexLoader.h"
#include "VertexLoaderManager.h"
#include "VertexManager.h"
#include "PixelShaderManager.h"
#include "VertexShaderManager.h"
#include "XFB.h"
#include "XFBConvert.h"

#include "VideoState.h"
#if !defined(OSX64)
#include "Debugger/Debugger.h" // for the CDebugger class
#endif
SVideoInitialize g_VideoInitialize;
#define VERSION_STRING "0.1"

// Create debugging window. We can't use Show() here as usual because then DLL_PROCESS_DETACH will
// be called immediately. And if we use ShowModal() we block the main video window from appearing.
// So I've made a separate function called DoDllDebugger() that creates the window.
#if defined(OSX64)
void DllDebugger(HWND _hParent) { }
void DoDllDebugger() { }
#else
CDebugger* m_frame;
void DllDebugger(HWND _hParent)
{
	if(m_frame) // if we have created it, let us show it again
	{
		m_frame->Show();
	}
	else
	{
		wxMessageBox(_T("The debugging window will open after you start a game."));
	}
}

void DoDllDebugger()
{
	m_frame = new CDebugger(NULL);
	m_frame->Show();
}
#endif

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

void DllConfig(HWND _hParent)
{
#if defined(_WIN32)
	wxWindow win;
	win.SetHWND(_hParent);
	ConfigDialog frame(&win);

	DWORD iModeNum = 0;
	DEVMODE dmi;
	ZeroMemory(&dmi, sizeof(dmi));
	dmi.dmSize = sizeof(dmi);
	std::string resos[100];
	int i = 0;
	
	while (EnumDisplaySettings(NULL, iModeNum++, &dmi) != 0)
	{	
		char szBuffer[100];
		sprintf(szBuffer,"%dx%d", dmi.dmPelsWidth, dmi.dmPelsHeight);
		//making a string cause char*[] to char was a baaad idea
		std::string strBuffer(szBuffer);
		//create a check loop to check every pointer of resos to see if the res is added or not
		int b = 0;
		bool resFound = false;
		while (b < i && !resFound)
		{
			//is the res already added?
			resFound = (resos[b] == strBuffer);
			b++;
		}
		if (!resFound)
		//and add the res
		{
			resos[i] = strBuffer;
			i++;
			frame.AddFSReso(szBuffer);			
			frame.AddWindowReso(szBuffer);
		}
        ZeroMemory(&dmi, sizeof(dmi));
	}
	frame.ShowModal();
	win.SetHWND(0);

#elif defined(__linux__)
	ConfigDialog frame(NULL);
	g_Config.Load();
    int glxMajorVersion, glxMinorVersion;
    int vidModeMajorVersion, vidModeMinorVersion;
    GLWin.dpy = XOpenDisplay(0);
    glXQueryVersion(GLWin.dpy, &glxMajorVersion, &glxMinorVersion);
    XF86VidModeQueryVersion(GLWin.dpy, &vidModeMajorVersion, &vidModeMinorVersion);
	//Get all full screen resos for the config dialog
	XF86VidModeModeInfo **modes = NULL;
	int modeNum = 0;
	int bestMode = 0;

	//set best mode to current
	bestMode = 0;
	XF86VidModeGetAllModeLines(GLWin.dpy, GLWin.screen, &modeNum, &modes);
	int px = 0, py = 0;
	if (modeNum > 0 && modes != NULL) 
	{
		for (int i = 0; i < modeNum; i++) 
		{
			if(px != modes[i]->hdisplay && py != modes[i]->vdisplay)
			{
				char temp[32];
				sprintf(temp,"%dx%d", modes[i]->hdisplay, modes[i]->vdisplay);
				frame.AddFSReso(temp);
				frame.AddWindowReso(temp);//Add same to Window ones, since they should be nearly all that's needed
				px = modes[i]->hdisplay;//Used to remove repeating from different screen depths
				py = modes[i]->vdisplay;
			}
		}
        }    
	XFree(modes);
	frame.ShowModal();
#else
	//TODO
#endif
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
	InitXFBConvTables();
    g_Config.Load();
    
    if (!OpenGL_Create(g_VideoInitialize, 640, 480)) { //640x480 will be the default if all else fails//
        g_VideoInitialize.pLog("Renderer::Create failed\n", TRUE);
        return;
    }
    _pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
    _pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;
    _pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;

	Renderer::AddMessage("Dolphin OpenGL Video Plugin v" VERSION_STRING ,5000);
}

void Video_DoState(unsigned char **ptr, int mode) {

	// Clear all caches that touch RAM
	TextureMngr::Invalidate();
	// DisplayListManager::Invalidate();

	VertexLoaderManager::MarkAllDirty();

	PointerWrap p(ptr, mode);
	VideoCommon_DoState(p);

	// Refresh state.
	if (mode == 1)  // read
		BPReload();
}

// This is called after Video_Initialize() from the Core
void Video_Prepare(void)
{
    OpenGL_MakeCurrent();
    if (!Renderer::Create2()) {
        g_VideoInitialize.pLog("Renderer::Create2 failed\n", TRUE);
        PanicAlert("Can't create opengl renderer. You might be missing some required opengl extensions, check the logs for more info");
        exit(1);
    }

    TextureMngr::Init();

    BPInit();
    VertexManager::Init();
    Fifo_Init(); // must be done before OpcodeDecoder_Init()
    OpcodeDecoder_Init();
    VertexShaderMngr::Init();
    PixelShaderMngr::Init();
    GL_REPORT_ERRORD();
    VertexLoaderManager::Init();
}

void Video_Shutdown(void) 
{
    VertexLoaderManager::Shutdown();
    VertexShaderMngr::Shutdown();
    PixelShaderMngr::Shutdown();
    Fifo_Shutdown();
    VertexManager::Shutdown();
    TextureMngr::Shutdown();
    OpcodeDecoder_Shutdown();
    Renderer::Shutdown();
    OpenGL_Shutdown();
}

void Video_Stop(void) 
{
    Fifo_Stop();
}

void Video_EnterLoop()
{
	Fifo_EnterLoop(g_VideoInitialize);
}

void DebugLog(const char* _fmt, ...)
{
#if defined(_DEBUG) || defined(DEBUGFAST)
    
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
    Renderer::RenderText(str, left+1, top+1, 0xff000000);
    Renderer::RenderText(str, left, top, 0xffc0ffff);
    Renderer::RestoreGLState();

    if (Renderer::SaveRenderTarget(File, 0)) {
        char msg[255];
        sprintf(msg, "saved %s\n", File);
        Renderer::AddMessage(msg, 500);
    	return true;
    }
	return false;
}
#if defined(OSX64)
unsigned int Video_Screenshot(TCHAR* _szFilename)
#else
BOOL Video_Screenshot(TCHAR* _szFilename)
#endif
{
    if (ScreenShot(_szFilename))
        return TRUE;

    return FALSE;
}

void Video_UpdateXFB(u8* _pXFB, u32 _dwWidth, u32 _dwHeight, s32 _dwYOffset)
{
	if(g_Config.bUseXFB)
	{
		XFB_Draw(_pXFB, _dwWidth, _dwHeight, _dwYOffset);
	}
}

void Video_AddMessage(const char* pstr, u32 milliseconds)
{
	Renderer::AddMessage(pstr,milliseconds);
}
