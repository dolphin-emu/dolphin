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

#ifndef _PLUGINVIDEO_H_
#define _PLUGINVIDEO_H_

#include "pluginspecs_video.h"
#include "Plugin.h"

namespace Common {

typedef void (__cdecl* TVideo_Prepare)();
typedef void (__cdecl* TVideo_SendFifoData)(u8*,u32);
typedef void (__cdecl* TVideo_BeginField)(u32, FieldType, u32, u32);
typedef void (__cdecl* TVideo_EndField)();
typedef bool (__cdecl* TVideo_Screenshot)(const char* filename);
typedef void (__cdecl* TVideo_EnterLoop)();
typedef void (__cdecl* TVideo_ExitLoop)();
typedef void (__cdecl* TVideo_SetRendering)(bool bEnabled);
typedef void (__cdecl* TVideo_AddMessage)(const char* pstr, unsigned int milliseconds);
typedef u32 (__cdecl* TVideo_AccessEFB)(EFBAccessType, u32, u32);
typedef void (__cdecl* TVideo_Read16)(u16& _rReturnValue, const u32 _Address);
typedef void (__cdecl* TVideo_Write16)(const u16 _Data, const u32 _Address);
typedef void (__cdecl* TVideo_Read32)(u32& _rReturnValue, const u32 _Address);
typedef void (__cdecl* TVideo_Write32)(const u32 _Data, const u32 _Address);
typedef void (__cdecl* TVideo_GatherPipeBursted)();
typedef void (__cdecl* TVideo_WaitForFrameFinish)();

class PluginVideo : public CPlugin
{
public:
	PluginVideo(const char *_Filename);
	virtual ~PluginVideo();
	virtual bool IsValid() {return validVideo;};

	TVideo_Prepare              Video_Prepare;
	TVideo_EnterLoop            Video_EnterLoop;
	TVideo_ExitLoop             Video_ExitLoop;
	TVideo_BeginField           Video_BeginField;
	TVideo_EndField             Video_EndField;
	TVideo_AccessEFB	        Video_AccessEFB;

	TVideo_AddMessage           Video_AddMessage;
	TVideo_Screenshot           Video_Screenshot;

	TVideo_SetRendering         Video_SetRendering;    

    TVideo_Read16               Video_CommandProcessorRead16;
    TVideo_Write16              Video_CommandProcessorWrite16;
    TVideo_Read16               Video_PixelEngineRead16;
    TVideo_Write16              Video_PixelEngineWrite16;
    TVideo_Write32              Video_PixelEngineWrite32;
    TVideo_GatherPipeBursted    Video_GatherPipeBursted;
    TVideo_WaitForFrameFinish   Video_WaitForFrameFinish;

private:
	bool validVideo;
};

}  // namespace

#endif // _PLUGINVIDEO_H_
