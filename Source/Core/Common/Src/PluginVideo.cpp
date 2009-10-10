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

#include "PluginVideo.h"

namespace Common
{

PluginVideo::PluginVideo(const char *_Filename) : CPlugin(_Filename), validVideo(false)
{
	Video_Prepare = 0;
	Video_BeginField = 0;
	Video_EndField = 0;
	Video_EnterLoop = 0;
	Video_ExitLoop = 0;
	Video_Screenshot = 0;
	Video_AddMessage = 0;
	Video_AccessEFB = 0;
    Video_SetRendering = 0;    
    Video_CommandProcessorRead16 = 0;
    Video_CommandProcessorWrite16 = 0;
    Video_PixelEngineRead16 = 0;
    Video_PixelEngineWrite16 = 0;
    Video_PixelEngineWrite32 = 0;
    Video_GatherPipeBursted = 0;
    Video_WaitForFrameFinish = 0;

	Video_Prepare  = reinterpret_cast<TVideo_Prepare>
		(LoadSymbol("Video_Prepare"));
	Video_BeginField = reinterpret_cast<TVideo_BeginField>
		(LoadSymbol("Video_BeginField"));
	Video_EndField = reinterpret_cast<TVideo_EndField>
		(LoadSymbol("Video_EndField"));
	Video_Screenshot = reinterpret_cast<TVideo_Screenshot>
		(LoadSymbol("Video_Screenshot"));
	Video_EnterLoop = reinterpret_cast<TVideo_EnterLoop>
		(LoadSymbol("Video_EnterLoop"));
	Video_ExitLoop = reinterpret_cast<TVideo_ExitLoop>
		(LoadSymbol("Video_ExitLoop"));
	Video_AddMessage = reinterpret_cast<TVideo_AddMessage>
		(LoadSymbol("Video_AddMessage"));
	Video_AccessEFB = reinterpret_cast<TVideo_AccessEFB>
		(LoadSymbol("Video_AccessEFB"));
	Video_SetRendering = reinterpret_cast<TVideo_SetRendering>
		(LoadSymbol("Video_SetRendering"));
    Video_CommandProcessorRead16 = reinterpret_cast<TVideo_Read16>
		(LoadSymbol("Video_CommandProcessorRead16"));
    Video_CommandProcessorWrite16 = reinterpret_cast<TVideo_Write16>
		(LoadSymbol("Video_CommandProcessorWrite16"));
    Video_PixelEngineRead16 = reinterpret_cast<TVideo_Read16>
		(LoadSymbol("Video_PixelEngineRead16"));
    Video_PixelEngineWrite16 = reinterpret_cast<TVideo_Write16>
		(LoadSymbol("Video_PixelEngineWrite16"));
    Video_PixelEngineWrite32 = reinterpret_cast<TVideo_Write32>
		(LoadSymbol("Video_PixelEngineWrite32"));
    Video_GatherPipeBursted = reinterpret_cast<TVideo_GatherPipeBursted>
		(LoadSymbol("Video_GatherPipeBursted"));
    Video_WaitForFrameFinish = reinterpret_cast<TVideo_WaitForFrameFinish>
		(LoadSymbol("Video_WaitForFrameFinish"));

	if ((Video_Prepare                  != 0) &&
		(Video_BeginField               != 0) &&
		(Video_EndField                 != 0) &&
		(Video_EnterLoop                != 0) &&
		(Video_ExitLoop                 != 0) &&
		(Video_Screenshot               != 0) &&
		(Video_AddMessage               != 0) &&
		(Video_SetRendering             != 0) &&
		(Video_AccessEFB	            != 0) &&
        (Video_SetRendering             != 0) &&        
        (Video_CommandProcessorRead16   != 0) &&
        (Video_CommandProcessorWrite16  != 0) &&
        (Video_PixelEngineRead16        != 0) &&
        (Video_PixelEngineWrite16       != 0) &&
        (Video_PixelEngineWrite32       != 0) &&
        (Video_GatherPipeBursted        != 0) &&
        (Video_WaitForFrameFinish       != 0))
		validVideo = true;
}

PluginVideo::~PluginVideo() {}

}  // namespace
