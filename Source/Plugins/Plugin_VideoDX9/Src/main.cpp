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

#include <tchar.h>
#include <windows.h> 
#include <d3dx9.h>

#include "Common.h"

#include "svnrev.h"
#include "resource.h"
#include "main.h"
#include "Globals.h"
#include "Fifo.h"
#include "OpcodeDecoding.h"
#include "TextureCache.h"
#include "BPStructs.h"
#include "VertexHandler.h"
#include "TransformEngine.h"
#include "DlgSettings.h"
#include "D3DPostprocess.h"
#include "D3DTexture.h"
#include "D3DUtil.h"
#include "W32Util/Misc.h"
#include "EmuWindow.h"
#include "VideoState.h"

#include "Utils.h"

HINSTANCE g_hInstance = NULL;
SVideoInitialize g_VideoInitialize;
int initCount = 0;

BOOL APIENTRY DllMain(	HINSTANCE hinstDLL,	// DLL module handle
						DWORD dwReason,		// reason called
						LPVOID lpvReserved)	// reserved
{
	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		break;
	case DLL_PROCESS_DETACH:
		break;
	default:
		break;
	}

	g_hInstance = hinstDLL;
	return TRUE;
}

BOOL Callback_PeekMessages()
{
	//TODO: peek message
	MSG msg;
	while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT)
			return FALSE;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return TRUE;
}


void UpdateFPSDisplay(const char *text)
{
	char temp[512];
	sprintf_s(temp, 512, "SVN R%i: DX9: %s", SVN_REV, text);
    SetWindowText( EmuWindow::GetWnd(), temp);
}


bool Init()
{
	g_Config.Load();

	if (initCount == 0)
	{
        // create the window
        if ( !g_Config.renderToMainframe || g_VideoInitialize.pWindowHandle == NULL ) // ignore parent for this plugin
        {
            g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create(NULL, g_hInstance, "Loading - Please wait.");
        }
		else
		{
			g_VideoInitialize.pWindowHandle = (void*)EmuWindow::Create((HWND)g_VideoInitialize.pWindowHandle, g_hInstance, "Loading - Please wait.");
		}

		if ( g_VideoInitialize.pWindowHandle == NULL )
		{
			MessageBox(GetActiveWindow(), "An error has occurred while trying to create the window.", "Fatal Error", MB_OK);
			return false;
		}

		EmuWindow::Show();
		g_VideoInitialize.pPeekMessages = Callback_PeekMessages;
		g_VideoInitialize.pUpdateFPSDisplay = UpdateFPSDisplay;

		if (FAILED(D3D::Init()))
		{
			MessageBox(GetActiveWindow(), "Unable to initialize Direct3D. Please make sure that you have DirectX 9.0c correctly installed.", "Fatal Error", MB_OK);
			return false;
		}
		InitLUTs();

	}
	initCount++;

	return true;
}


void DeInit()
{
	initCount--;
	if (initCount==0)
	{
		D3D::Shutdown();
        EmuWindow::Close();
	}
}

// ====================================================================================

void GetDllInfo (PLUGIN_INFO* _PluginInfo) 
{
	_PluginInfo->Version = 0x0100;
	_PluginInfo->Type = PLUGIN_TYPE_VIDEO;
#ifdef DEBUGFAST 
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Video Plugin DebugFast (DX9)");
#else
#ifndef _DEBUG
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Video Plugin (DX9)");
#else
	sprintf_s(_PluginInfo->Name, 100, "Dolphin Video Plugin Debug (DX9)");
#endif
#endif
}

void DllAbout(HWND _hParent) 
{
	DialogBox(g_hInstance,(LPCSTR)IDD_ABOUT,_hParent,(DLGPROC)AboutProc);
}

void DllConfig(HWND _hParent)
{
	if (Init())
	{
		DlgSettings_Show(g_hInstance,_hParent);
		DeInit();
	}
}

void Video_Initialize(SVideoInitialize* _pVideoInitialize)
{
    if (_pVideoInitialize == NULL)
        return;

	frameCount = 0;
	g_VideoInitialize = *_pVideoInitialize;
	Init();
    _pVideoInitialize->pPeekMessages = g_VideoInitialize.pPeekMessages;
    _pVideoInitialize->pUpdateFPSDisplay = g_VideoInitialize.pUpdateFPSDisplay;
    _pVideoInitialize->pWindowHandle = g_VideoInitialize.pWindowHandle;

	Renderer::AddMessage("Dolphin Direct3D9 Video Plugin.",5000);

}

void Video_DoState(unsigned char **ptr, int mode) {
	// Clear all caches
	TextureCache::Invalidate();

	PointerWrap p(ptr, mode);
	VideoCommon_DoState(p);
	//PanicAlert("Saving/Loading state from DirectX9");
}


void Video_EnterLoop()
{
	Fifo_EnterLoop(g_VideoInitialize);
}

void Video_Prepare(void)
{
	Renderer::Init(g_VideoInitialize);

	TextureCache::Init();

	BPInit();
	CVertexHandler::Init();
	Fifo_Init();
	OpcodeDecoder_Init();
}

void Video_Shutdown(void) 
{
	Fifo_Shutdown();
	CVertexHandler::Shutdown();
	TextureCache::Shutdown();
	Renderer::Shutdown();
	OpcodeDecoder_Shutdown();
	DeInit();
}

void Video_Stop(void) 
{
}

void Video_UpdateXFB(u8* /*_pXFB*/, u32 /*_dwWidth*/, u32 /*_dwHeight*/)
{
	/*
	ConvertXFB(tempBuffer, _pXFB, _dwWidth, _dwHeight);

	// blubb
	static LPDIRECT3DTEXTURE9 pTexture = D3D::CreateTexture2D((BYTE*)tempBuffer, _dwWidth, _dwHeight, _dwWidth, D3DFMT_A8R8G8B8);

	D3D::ReplaceTexture2D(pTexture, (BYTE*)tempBuffer, _dwWidth, _dwHeight, _dwWidth, D3DFMT_A8R8G8B8);
	D3D::dev->SetTexture(0, pTexture);

	D3D::quad2d(0,0,(float)Postprocess::GetWidth(),(float)Postprocess::GetHeight(), 0xFFFFFFFF);

	D3D::EndFrame();
	D3D::BeginFrame();*/
}


void DebugLog(const char* _fmt, ...)
{
#ifdef _DEBUG
	char Msg[512];
	va_list ap;

	va_start( ap, _fmt );
	vsprintf( Msg, _fmt, ap );
	va_end( ap );

	g_VideoInitialize.pLog(Msg, FALSE);
#endif
}

void __Log(int log, const char *format, ...)
{
//	char temp[512];
	//va_list args;
	//va_start(args, format);
	//CharArrayFromFormatV(temp, 512, format, args);
	//va_end(args);

	DebugLog(format); //"%s", temp);
}


HRESULT ScreenShot(TCHAR *File) 
{
	if (D3D::dev == NULL)
		return S_FALSE;

	D3DDISPLAYMODE DisplayMode;
	if (FAILED(D3D::dev->GetDisplayMode(0, &DisplayMode)))
		return S_FALSE;

	LPDIRECT3DSURFACE9 surf;
	if (FAILED(D3D::dev->CreateOffscreenPlainSurface(DisplayMode.Width, DisplayMode.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &surf, NULL)))
		return S_FALSE;

	if (FAILED(D3D::dev->GetFrontBufferData(0, surf)))
	{
		surf->Release(); 
		return S_FALSE;
	}

	RECT rect;
    ::GetWindowRect(EmuWindow::GetWnd(), &rect);
	if (FAILED(D3DXSaveSurfaceToFile(File, D3DXIFF_JPG, surf, NULL, &rect))) 
	{
		surf->Release();
		return S_FALSE;
	}

	surf->Release();

	return S_OK;
}

BOOL Video_Screenshot(TCHAR* _szFilename)
{
	if (ScreenShot(_szFilename) == S_OK)
		return TRUE;

	return FALSE;
}

void Video_AddMessage(const char* pstr, u32 milliseconds)
{
	Renderer::AddMessage(pstr,milliseconds);
}
