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

#include "PluginVideo.h"

namespace Common
{

PluginVideo::PluginVideo(const char *_Filename) : CPlugin(_Filename), validVideo(false)
{
	Video_Prepare = 0;
	Video_SendFifoData = 0;
	Video_UpdateXFB = 0;
	Video_EnterLoop = 0;
	Video_ExitLoop = 0;
	Video_Screenshot = 0;
	Video_AddMessage = 0;

	Video_Prepare  = reinterpret_cast<TVideo_Prepare>
		(LoadSymbol("Video_Prepare"));
	Video_SendFifoData = reinterpret_cast<TVideo_SendFifoData>
		(LoadSymbol("Video_SendFifoData"));
	Video_UpdateXFB = reinterpret_cast<TVideo_UpdateXFB>
		(LoadSymbol("Video_UpdateXFB"));
	Video_Screenshot = reinterpret_cast<TVideo_Screenshot>
		(LoadSymbol("Video_Screenshot"));
	Video_EnterLoop = reinterpret_cast<TVideo_EnterLoop>
		(LoadSymbol("Video_EnterLoop"));
	Video_ExitLoop = reinterpret_cast<TVideo_ExitLoop>
		(LoadSymbol("Video_ExitLoop"));
	Video_AddMessage = reinterpret_cast<TVideo_AddMessage>
		(LoadSymbol("Video_AddMessage"));

	if ((Video_Prepare      != 0) &&
		(Video_SendFifoData != 0) &&
		(Video_UpdateXFB    != 0) &&
		(Video_EnterLoop    != 0) &&
		(Video_ExitLoop     != 0) &&
		(Video_Screenshot   != 0) &&
		(Video_AddMessage   != 0))
		validVideo = true;
}

PluginVideo::~PluginVideo() {}

}  // namespace
