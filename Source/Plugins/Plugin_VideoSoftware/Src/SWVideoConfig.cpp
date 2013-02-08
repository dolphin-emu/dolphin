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

#include "FileUtil.h"
#include "IniFile.h"
#include "SWVideoConfig.h"

SWVideoConfig g_SWVideoConfig;

SWVideoConfig::SWVideoConfig()
{
    bFullscreen = false;
    bHideCursor = false;
    renderToMainframe = false;	

	bHwRasterizer = false;

    bShowStats = false;

    bDumpTextures = false;
    bDumpObjects = false;
    bDumpFrames = false;

    bDumpTevStages = false;
	bDumpTevTextureFetches = false;

    drawStart = 0;
    drawEnd = 100000;
}

void SWVideoConfig::Load(const char* ini_file)
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load(ini_file);
    
    iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0); // Hardware
    iniFile.Get("Hardware", "RenderToMainframe", &renderToMainframe, false);

	iniFile.Get("Rendering", "HwRasterizer", &bHwRasterizer, false);

	iniFile.Get("Info", "ShowStats", &bShowStats, false);

	iniFile.Get("Utility", "DumpTexture", &bDumpTextures, false);
	iniFile.Get("Utility", "DumpObjects", &bDumpObjects, false);
	iniFile.Get("Utility", "DumpFrames", &bDumpFrames, false);
	iniFile.Get("Utility", "DumpTevStages", &bDumpTevStages, false);
	iniFile.Get("Utility", "DumpTevTexFetches", &bDumpTevTextureFetches, false);

	iniFile.Get("Misc", "DrawStart", &drawStart, 0);
	iniFile.Get("Misc", "DrawEnd", &drawEnd, 100000);
}

void SWVideoConfig::Save(const char* ini_file)
{
    IniFile iniFile;
    iniFile.Load(ini_file);

    iniFile.Set("Hardware", "Fullscreen", bFullscreen);
    iniFile.Set("Hardware", "RenderToMainframe", renderToMainframe);

	iniFile.Set("Rendering", "HwRasterizer", bHwRasterizer);

	iniFile.Set("Info", "ShowStats", bShowStats);

	iniFile.Set("Utility", "DumpTexture", bDumpTextures);
	iniFile.Set("Utility", "DumpObjects", bDumpObjects);
	iniFile.Set("Utility", "DumpFrames", bDumpFrames);
	iniFile.Set("Utility", "DumpTevStages", bDumpTevStages);
	iniFile.Set("Utility", "DumpTevTexFetches", bDumpTevTextureFetches);

	iniFile.Set("Misc", "DrawStart", drawStart);
	iniFile.Set("Misc", "DrawEnd", drawEnd);
    
    iniFile.Save(ini_file);
}

