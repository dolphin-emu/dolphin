// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "IniFile.h"
#include "Debugger.h"
#include "FileUtil.h"

#include "VideoConfig.h"
#include "TextureCacheBase.h"
#include "PixelShaderGen.h"
#include "VertexShaderGen.h"
#include "NativeVertexFormat.h"

//void UpdateFPSDisplay(const char *text);
extern NativeVertexFormat *g_nativeVertexFmt;

GFXDebuggerBase *g_pdebugger = NULL;
volatile bool GFXDebuggerPauseFlag = false; // if true, the GFX thread will be spin locked until it's false again
volatile PauseEvent GFXDebuggerToPauseAtNext = NOT_PAUSE; // Event which will trigger spin locking the GFX thread
volatile int GFXDebuggerEventToPauseCount = 0; // Number of events to wait for until GFX thread will be paused

void GFXDebuggerUpdateScreen()
{
	// TODO: Implement this in a backend-independent way
/*	// update screen
	if (D3D::bFrameInProgress)
	{
		D3D::dev->SetRenderTarget(0, D3D::GetBackBufferSurface());
		D3D::dev->SetDepthStencilSurface(NULL);

		D3D::dev->StretchRect(FramebufferManager::GetEFBColorRTSurface(), NULL,
			D3D::GetBackBufferSurface(), NULL,
			D3DTEXF_LINEAR);

		D3D::dev->EndScene();
		D3D::dev->Present(NULL, NULL, NULL, NULL);

		D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		D3D::dev->BeginScene();
	}
	else
	{
		D3D::dev->EndScene();
		D3D::dev->Present(NULL, NULL, NULL, NULL);
		D3D::dev->BeginScene();
	}*/
}

// GFX thread
void GFXDebuggerCheckAndPause(bool update)
{
	if (GFXDebuggerPauseFlag)
	{
		g_pdebugger->OnPause();
		while( GFXDebuggerPauseFlag )
		{
			g_video_backend->UpdateFPSDisplay("Paused by Video Debugger");

			if (update)	GFXDebuggerUpdateScreen();
			SLEEP(5);
		}
		g_pdebugger->OnContinue();
	}
}

// GFX thread
void GFXDebuggerToPause(bool update)
{
	GFXDebuggerToPauseAtNext = NOT_PAUSE;
	GFXDebuggerPauseFlag = true;
	GFXDebuggerCheckAndPause(update);
}

void ContinueGFXDebugger()
{
	GFXDebuggerPauseFlag = false;
}


void GFXDebuggerBase::DumpPixelShader(const char* path)
{
	char filename[MAX_PATH];
	sprintf(filename, "%sdump_ps.txt", path);

	std::string output;
	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;
	if (!useDstAlpha)
	{
		output = "Destination alpha disabled:\n";
///		output += GeneratePixelShaderCode(DSTALPHA_NONE, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
	}
	else
	{
		if(g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
		{
			output = "Using dual source blending for destination alpha:\n";
///			output += GeneratePixelShaderCode(DSTALPHA_DUAL_SOURCE_BLEND, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
		}
		else
		{
			output = "Using two passes for emulating destination alpha:\n";
///			output += GeneratePixelShaderCode(DSTALPHA_NONE, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
			output += "\n\nDestination alpha pass shader:\n";
///			output += GeneratePixelShaderCode(DSTALPHA_ALPHA_PASS, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
		}
	}

	File::CreateEmptyFile(filename);
	File::WriteStringToFile(true, output, filename);
}

void GFXDebuggerBase::DumpVertexShader(const char* path)
{
	char filename[MAX_PATH];
	sprintf(filename, "%sdump_vs.txt", path);

	File::CreateEmptyFile(filename);
///	File::WriteStringToFile(true, GenerateVertexShaderCode(g_nativeVertexFmt->m_components, g_ActiveConfig.backend_info.APIType), filename);
}

void GFXDebuggerBase::DumpPixelShaderConstants(const char* path)
{
	// TODO
}

void GFXDebuggerBase::DumpVertexShaderConstants(const char* path)
{
	// TODO
}

void GFXDebuggerBase::DumpTextures(const char* path)
{
	// TODO
}

void GFXDebuggerBase::DumpFrameBuffer(const char* path)
{
	// TODO
}

void GFXDebuggerBase::DumpGeometry(const char* path)
{
	// TODO
}

void GFXDebuggerBase::DumpVertexDecl(const char* path)
{
	// TODO
}

void GFXDebuggerBase::DumpMatrices(const char* path)
{
	// TODO
}

void GFXDebuggerBase::DumpStats(const char* path)
{
	// TODO
}
