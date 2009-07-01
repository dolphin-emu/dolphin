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

#ifndef _PLUGINVIDEO_H_
#define _PLUGINVIDEO_H_

#include "pluginspecs_video.h"
#include "Plugin.h"

namespace Common {

typedef void (__cdecl* TVideo_Prepare)();
typedef void (__cdecl* TVideo_SendFifoData)(u8*,u32);
typedef void (__cdecl* TVideo_UpdateXFB)(u32, u32, u32, s32, bool);
typedef bool (__cdecl* TVideo_Screenshot)(const char* filename);
typedef void (__cdecl* TVideo_EnterLoop)();
typedef void (__cdecl* TVideo_ExitLoop)();
typedef void (__cdecl* TVideo_AddMessage)(const char* pstr, unsigned int milliseconds);
typedef u32 (__cdecl* TVideo_AccessEFB)(EFBAccessType, u32, u32);

class PluginVideo : public CPlugin
{
public:
	PluginVideo(const char *_Filename);
	virtual ~PluginVideo();
	virtual bool IsValid() {return validVideo;};

	TVideo_Prepare      Video_Prepare;
	TVideo_SendFifoData Video_SendFifoData;
	TVideo_EnterLoop    Video_EnterLoop;
	TVideo_ExitLoop     Video_ExitLoop;
	TVideo_UpdateXFB    Video_UpdateXFB;
	TVideo_AccessEFB	Video_AccessEFB;

	TVideo_AddMessage   Video_AddMessage;
	TVideo_Screenshot   Video_Screenshot;

private:
	bool validVideo;
};

}  // namespace

#endif // _PLUGINVIDEO_H_
