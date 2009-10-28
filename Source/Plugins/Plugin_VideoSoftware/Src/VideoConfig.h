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

#ifndef _PLUGIN_VIDEOSOFTWARE_CONFIG_H_
#define _PLUGIN_VIDEOSOFTWARE_CONFIG_H_

#include "Common.h"

#define STATISTICS 1

// NEVER inherit from this class.
struct Config
{
    Config();
    void Load();
    void Save();

    // General
    bool bFullscreen;
    bool bHideCursor;
    bool renderToMainframe;	

    bool bShowStats;
    bool bDumpTextures;
    bool bDumpObjects;
    bool bDumpFrames;
    bool bDumpTevStages;

    bool bHwRasterizer;

    u32 nFrameSkip;

    u32 drawStart;
    u32 drawEnd;

private:
	DISALLOW_COPY_AND_ASSIGN(Config);
};

extern Config g_Config;

#endif  // _PLUGIN_VIDEOSOFTWARE_CONFIG_H_
