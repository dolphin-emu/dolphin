// Copyright (C) 2003-2009 Dolphin Project.

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


#include "Common.h"

#if defined(HAVE_WX) && HAVE_WX
#include "VideoConfigDialog.h"
#endif // HAVE_WX

#include "CommandProcessor.h"
#include "OpcodeDecoder.h"
#include "SWVideoConfig.h"
#include "PixelEngine.h"
#include "CommandProcessor.h"
#include "BPMemLoader.h"
#include "XFMemLoader.h"
#include "Clipper.h"
#include "Rasterizer.h"
#include "Renderer.h"
#include "../../../Core/VideoCommon/Src/LookUpTables.h"
#include "HwRasterizer.h"
#include "LogManager.h"
#include "EfbInterface.h"
#include "DebugUtil.h"
#include "FileUtil.h"
#include "VideoBackend.h"

namespace SW
{

std::string VideoBackend::GetName()
{
	return "Software Renderer";
}

void *DllDebugger(void *_hParent, bool Show)
{
	return NULL;
}

void VideoBackend::ShowConfig(void *_hParent)
{
#if defined(HAVE_WX) && HAVE_WX
	VideoConfigDialog* const diag = new VideoConfigDialog((wxWindow*)_hParent, "Software", "gfx_software");
	diag->ShowModal();
	diag->Destroy();
#endif
}

void VideoBackend::Initialize()
{
    g_SWVideoConfig.Load((std::string(File::GetUserPath(D_CONFIG_IDX)) + "gfx_software.ini").c_str());

    InitBPMemory();
    InitXFMemory();
    CommandProcessor::Init();
    PixelEngine::Init();
    OpcodeDecoder::Init();
    Clipper::Init();
    Rasterizer::Init();
    HwRasterizer::Init();
    Renderer::Init();
    DebugUtil::Init();
}

void VideoBackend::DoState(PointerWrap&)
{

}

void VideoBackend::EmuStateChange(PLUGIN_EMUSTATE newState)
{

}

//bool IsD3D()
//{
//	return false;
//}

void VideoBackend::Shutdown()
{
	Renderer::Shutdown();
	OpenGL_Shutdown();
}

// This is called after Video_Initialize() from the Core
void VideoBackend::Video_Prepare()
{    
    Renderer::Prepare();

    INFO_LOG(VIDEO, "Video plugin initialized.");
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackend::Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{	
}

// Run from the CPU thread (from VideoInterface.cpp)
void VideoBackend::Video_EndField()
{
}

u32 VideoBackend::Video_AccessEFB(EFBAccessType type, u32 x, u32 y, u32 InputData)
{
	u32 value = 0;

    switch (type)
    {
    case PEEK_Z:
        {
            value = EfbInterface::GetDepth(x, y);
            break;
        }
    case POKE_Z:
        break;
    case PEEK_COLOR:
        {
            u32 color = 0;
            EfbInterface::GetColor(x, y, (u8*)&color);

            // rgba to argb
            value = (color >> 8) | (color & 0xff) << 24;
            break;
        }
        
    case POKE_COLOR:
        break;
    }

    return value;
}

bool VideoBackend::Video_Screenshot(const char *_szFilename)
{
	return false;
}

// -------------------------------
// Enter and exit the video loop
// -------------------------------
void VideoBackend::Video_EnterLoop()
{
    Fifo_EnterLoop();
}

void VideoBackend::Video_ExitLoop()
{
	Fifo_ExitLoop();
}

void VideoBackend::Video_AddMessage(const char* pstr, u32 milliseconds)
{	
}

void VideoBackend::Video_SetRendering(bool bEnabled)
{
    Fifo_SetRendering(bEnabled);
}

void VideoBackend::Video_WaitForFrameFinish(void)
{

}

bool VideoBackend::Video_IsFifoBusy(void)
{
	return false;
}

void VideoBackend::Video_AbortFrame(void)
{

}

}