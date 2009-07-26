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

#ifndef _GLOBALS_H
#define _GLOBALS_H

#include <string>

struct Config 
{
	Config();
	void Load();
	void Save();

	int iAdapter;
	int iFSResolution;
	int iMultisampleMode;

	int iPostprocessEffect;

	bool renderToMainframe;
	bool bFullscreen;
	bool bVsync;
	bool bWireFrame;
	bool bOverlayStats;
	bool bOverlayProjStats;
	bool bDumpTextures;
	bool bDumpFrames;
	bool bOldCard;
	bool bShowShaderErrors;
	//enhancements
	bool bForceFiltering;
	bool bForceMaxAniso;

	bool bPreUpscale;
	int iPreUpscaleFilter;

	bool bTruform;
	int iTruformLevel;

	int iWindowedRes;

	char psProfile[16];
	char vsProfile[16];

	bool bTexFmtOverlayEnable;
	bool bTexFmtOverlayCenter;

	std::string texDumpPath;
};

extern Config g_Config;

#endif