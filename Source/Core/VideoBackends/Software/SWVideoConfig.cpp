// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "VideoBackends/Software/SWVideoConfig.h"

SWVideoConfig g_SWVideoConfig;

SWVideoConfig::SWVideoConfig()
{
	bFullscreen = false;
	bHideCursor = false;
	renderToMainframe = false;

	bHwRasterizer = false;
	bBypassXFB = false;

	bShowStats = false;

	bDumpTextures = false;
	bDumpObjects = false;
	bDumpFrames = false;

	bZComploc = true;
	bZFreeze = true;

	bDumpTevStages = false;
	bDumpTevTextureFetches = false;

	drawStart = 0;
	drawEnd = 100000;
}

void SWVideoConfig::Load(const char* ini_file)
{
	IniFile iniFile;
	iniFile.Load(ini_file);

	iniFile.Get("Hardware", "Fullscreen", &bFullscreen, 0); // Hardware
	iniFile.Get("Hardware", "RenderToMainframe", &renderToMainframe, false);

	iniFile.Get("Rendering", "HwRasterizer", &bHwRasterizer, false);
	iniFile.Get("Rendering", "BypassXFB", &bBypassXFB, false);
	iniFile.Get("Rendering", "ZComploc", &bZComploc, true);
	iniFile.Get("Rendering", "ZFreeze", &bZFreeze, true);

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
	iniFile.Set("Rendering", "BypassXFB", bBypassXFB);
	iniFile.Set("Rendering", "ZComploc", bZComploc);
	iniFile.Set("Rendering", "ZFreeze", bZFreeze);

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

