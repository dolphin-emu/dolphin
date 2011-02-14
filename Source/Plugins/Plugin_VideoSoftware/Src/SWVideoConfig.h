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

#ifndef _VIDEOSOFTWARE_CONFIG_H_
#define _VIDEOSOFTWARE_CONFIG_H_

#include "Common.h"

#define STATISTICS 1

// NEVER inherit from this class.
struct SWVideoConfig : NonCopyable
{
    SWVideoConfig();
    void Load(const char* ini_file);
    void Save(const char* ini_file);

    // General
    bool bFullscreen;
    bool bHideCursor;
    bool renderToMainframe;	

	bool bHwRasterizer;

    bool bShowStats;

    bool bDumpTextures;
    bool bDumpObjects;
    bool bDumpFrames;

	// Debug only
    bool bDumpTevStages;
	bool bDumpTevTextureFetches;

    u32 drawStart;
    u32 drawEnd;
};

extern SWVideoConfig g_SWVideoConfig;

#endif  // _VIDEOSOFTWARE_CONFIG_H_
