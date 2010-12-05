// Copyright (C) 2003 Dolphin Project.

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


#include "IniFile.h"
#include "Debugger.h"
#include "FileUtil.h"

#include "VideoConfig.h"
#include "TextureCacheBase.h"
#include "PixelShaderGen.h"
#include "VertexShaderGen.h"
#include "NativeVertexFormat.h"

GFXDebuggerBase *g_pdebugger = NULL;
volatile bool GFXDebuggerPauseFlag = false;
volatile PauseEvent GFXDebuggerToPauseAtNext = NOT_PAUSE;
volatile int GFXDebuggerEventToPauseCount = 0;

void UpdateFPSDisplay(const char *text);
extern NativeVertexFormat *g_nativeVertexFmt;

void GFXDebuggerUpdateScreen()
{
	// TODO: Implement this in a plugin-independent way
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

void GFXDebuggerCheckAndPause(bool update)
{
	if (GFXDebuggerPauseFlag)
	{
		g_pdebugger->OnPause();
		while( GFXDebuggerPauseFlag )
		{
			UpdateFPSDisplay("Paused by Video Debugger");

			if (update)	GFXDebuggerUpdateScreen();
			Sleep(5);
		}
		g_pdebugger->OnContinue();
	}
}

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
	sprintf(filename, "%s/dump_ps.txt", path);

	std::string output;
	bool useDstAlpha = bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && bpmem.zcontrol.pixel_format == PIXELFMT_RGBA6_Z24;
	if (!useDstAlpha)
	{
		output = "Destination alpha disabled:\n";
		output += GeneratePixelShaderCode(DSTALPHA_NONE, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
	}
	else
	{
		if(g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
		{
			output = "Using dual source blending for destination alpha:\n";
			output += GeneratePixelShaderCode(DSTALPHA_DUAL_SOURCE_BLEND, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
		}
		else
		{
			output = "Using two passes for emulating destination alpha:\n";
			output += GeneratePixelShaderCode(DSTALPHA_NONE, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
			output += "\n\nDestination alpha pass shader:\n";
			output += GeneratePixelShaderCode(DSTALPHA_ALPHA_PASS, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
		}
	}

	File::CreateEmptyFile(filename);
	File::WriteStringToFile(true, output.c_str(), filename);
}

void GFXDebuggerBase::DumpVertexShader(const char* path)
{
	char filename[MAX_PATH];
	sprintf(filename, "%s/dump_vs_consts.txt", path);

	File::CreateEmptyFile(filename);
	File::WriteStringToFile(true, GenerateVertexShaderCode(g_nativeVertexFmt->m_components, g_ActiveConfig.backend_info.APIType), filename);
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
