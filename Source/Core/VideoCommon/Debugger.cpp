// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <string>

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/Thread.h"

#include "VideoCommon/Debugger.h"
#include "VideoCommon/NativeVertexFormat.h"
#include "VideoCommon/PixelShaderGen.h"
#include "VideoCommon/TextureCacheBase.h"
#include "VideoCommon/VertexShaderGen.h"
#include "VideoCommon/VideoConfig.h"

GFXDebuggerBase *g_pdebugger = nullptr;
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
		D3D::dev->SetDepthStencilSurface(nullptr);

		D3D::dev->StretchRect(FramebufferManager::GetEFBColorRTSurface(), nullptr,
			D3D::GetBackBufferSurface(), nullptr,
			D3DTEXF_LINEAR);

		D3D::dev->EndScene();
		D3D::dev->Present(nullptr, nullptr, nullptr, nullptr);

		D3D::dev->SetRenderTarget(0, FramebufferManager::GetEFBColorRTSurface());
		D3D::dev->SetDepthStencilSurface(FramebufferManager::GetEFBDepthRTSurface());
		D3D::dev->BeginScene();
	}
	else
	{
		D3D::dev->EndScene();
		D3D::dev->Present(nullptr, nullptr, nullptr, nullptr);
		D3D::dev->BeginScene();
	}*/
}

// GFX thread
void GFXDebuggerCheckAndPause(bool update)
{
	if (GFXDebuggerPauseFlag)
	{
		g_pdebugger->OnPause();
		while ( GFXDebuggerPauseFlag )
		{
			if (update) GFXDebuggerUpdateScreen();
			Common::SleepCurrentThread(5);
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


void GFXDebuggerBase::DumpPixelShader(const std::string& path)
{
	const std::string filename = StringFromFormat("%sdump_ps.txt", path.c_str());

	std::string output;
	bool useDstAlpha = !g_ActiveConfig.bDstAlphaPass && bpmem.dstalpha.enable && bpmem.blendmode.alphaupdate && bpmem.zcontrol.pixel_format == PEControl::RGBA6_Z24;
	if (!useDstAlpha)
	{
		output = "Destination alpha disabled:\n";
///		output += GeneratePixelShaderCode(DSTALPHA_NONE, g_ActiveConfig.backend_info.APIType, g_nativeVertexFmt->m_components);
	}
	else
	{
		if (g_ActiveConfig.backend_info.bSupportsDualSourceBlend)
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
	File::WriteStringToFile(output, filename);
}

void GFXDebuggerBase::DumpVertexShader(const std::string& path)
{
	const std::string filename = StringFromFormat("%sdump_vs.txt", path.c_str());

	File::CreateEmptyFile(filename);
///	File::WriteStringToFile(GenerateVertexShaderCode(g_nativeVertexFmt->m_components, g_ActiveConfig.backend_info.APIType), filename);
}

void GFXDebuggerBase::DumpPixelShaderConstants(const std::string& path)
{
	// TODO
}

void GFXDebuggerBase::DumpVertexShaderConstants(const std::string& path)
{
	// TODO
}

void GFXDebuggerBase::DumpTextures(const std::string& path)
{
	// TODO
}

void GFXDebuggerBase::DumpFrameBuffer(const std::string& path)
{
	// TODO
}

void GFXDebuggerBase::DumpGeometry(const std::string& path)
{
	// TODO
}

void GFXDebuggerBase::DumpVertexDecl(const std::string& path)
{
	// TODO
}

void GFXDebuggerBase::DumpMatrices(const std::string& path)
{
	// TODO
}

void GFXDebuggerBase::DumpStats(const std::string& path)
{
	// TODO
}
