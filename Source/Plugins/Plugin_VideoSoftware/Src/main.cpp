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
#include "pluginspecs_video.h"

#include "CommandProcessor.h"
#include "OpcodeDecoder.h"
#include "VideoConfig.h"
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


PLUGIN_GLOBALS* globals = NULL;
static volatile bool fifoStateRun = false;
SVideoInitialize g_VideoInitialize;
bool g_SkipFrame;


void GetDllInfo (PLUGIN_INFO* _PluginInfo)
{
    _PluginInfo->Version = 0x0100;
    _PluginInfo->Type = PLUGIN_TYPE_VIDEO;
#ifdef DEBUGFAST
    sprintf(_PluginInfo->Name, "Dolphin Software Renderer (DebugFast)");
#else
#ifndef _DEBUG
    sprintf(_PluginInfo->Name, "Dolphin Software Renderer");
#else
    sprintf(_PluginInfo->Name, "Dolphin Software Renderer (Debug)");
#endif
#endif
}

void SetDllGlobals(PLUGIN_GLOBALS* _pPluginGlobals)
{
	globals = _pPluginGlobals;
	LogManager::SetInstance((LogManager *)globals->logManager);
}

void DllDebugger(HWND _hParent, bool Show)
{
}

void DllConfig(HWND _hParent)
{
}

void Initialize(void *init)
{
    SVideoInitialize *_pVideoInitialize = (SVideoInitialize*)init;
    g_VideoInitialize = *_pVideoInitialize;

    g_SkipFrame = false;

    g_Config.Load();

    InitBPMemory();
    InitXFMemory();
    CommandProcessor::Init();
    PixelEngine::Init();
    OpcodeDecoder::Init();
    Clipper::Init();
    Rasterizer::Init();
    HwRasterizer::Init();
    Renderer::Init(_pVideoInitialize);
}

void DoState(unsigned char **ptr, int mode)
{
}

void Shutdown(void)
{
}

// This is called after Video_Initialize() from the Core
void Video_Prepare(void)
{    
    Renderer::Prepare();

    INFO_LOG(VIDEO, "Video plugin initialized.");
}

// Run from the CPU thread (from VideoInterface.cpp)
void Video_BeginField(u32 xfbAddr, FieldType field, u32 fbWidth, u32 fbHeight)
{	
}

// Run from the CPU thread (from VideoInterface.cpp)
void Video_EndField()
{
}

u32 Video_AccessEFB(EFBAccessType type, u32 x, u32 y)
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

void Video_Screenshot(const char *_szFilename)
{
}

// -------------------------------
// Enter and exit the video loop
// -------------------------------
void Video_EnterLoop()
{
    fifoStateRun = true;

    while (fifoStateRun)
    {
        if (!CommandProcessor::RunBuffer())
            Common::SleepCurrentThread(1);
    }
}

void Video_ExitLoop()
{
}

void Video_AddMessage(const char* pstr, u32 milliseconds)
{	
}

void Video_SetRendering(bool bEnabled)
{	
}

void Video_CommandProcessorRead16(u16& _rReturnValue, const u32 _Address)
{
    CommandProcessor::Read16(_rReturnValue, _Address);
}

void Video_CommandProcessorWrite16(const u16 _Data, const u32 _Address)
{
    CommandProcessor::Write16(_Data, _Address);
}

void Video_PixelEngineRead16(u16& _rReturnValue, const u32 _Address)
{
    PixelEngine::Read16(_rReturnValue, _Address);
}

void Video_PixelEngineWrite16(const u16 _Data, const u32 _Address)
{
    PixelEngine::Write16(_Data, _Address);
}

void Video_PixelEngineWrite32(const u32 _Data, const u32 _Address)
{
    PixelEngine::Write32(_Data, _Address);
}

inline void Video_GatherPipeBursted(void)
{
    CommandProcessor::GatherPipeBursted();
}

void Video_WaitForFrameFinish(void)
{
}
