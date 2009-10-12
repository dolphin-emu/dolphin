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
#include "IniFile.h"
#include "VideoConfig.h"
#include "../../../Core/Core/Src/ConfigManager.h" // FIXME

Config g_Config;

Config::Config()
{
    bFullscreen = false;
    bHideCursor = false;
    renderToMainframe = false;	

    bShowStats = false;
    bDumpTextures = false;
    bDumpObjects = false;
    bDumpFrames = false;

    bHwRasterizer = false;

    nFrameSkip = 0;

    drawStart = 0;
    drawEnd = 100000;
}

void Config::Load()
{
    std::string temp;
    IniFile iniFile;
    iniFile.Load(FULL_CONFIG_DIR "gfx_software.ini");
    
    iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0); // Hardware
    iniFile.Get("Hardware", "RenderToMainframe", &renderToMainframe, false);
}



void Config::Save()
{
    IniFile iniFile;
    iniFile.Load(FULL_CONFIG_DIR "gfx_software.ini");

    iniFile.Set("Hardware", "Fullscreen", bFullscreen);
    iniFile.Set("Hardware", "RenderToMainframe", renderToMainframe);
    
    iniFile.Save(FULL_CONFIG_DIR "gfx_opengl.ini");
}

