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

#ifndef _PLUGIN_VIDEOOGL_CONFIG_H_
#define _PLUGIN_VIDEOOGL_CONFIG_H_

#include "Common.h"

// Log in two categories, and save three other options in the same byte
#define CONF_LOG			1
#define CONF_PRIMLOG		2
#define CONF_SAVETEXTURES	4
#define CONF_SAVETARGETS	8
#define CONF_SAVESHADERS	16

enum MultisampleMode {
	MULTISAMPLE_OFF,
	MULTISAMPLE_2X,
	MULTISAMPLE_4X,
	MULTISAMPLE_8X,
	MULTISAMPLE_CSAA_8X,
	MULTISAMPLE_CSAA_8XQ,
	MULTISAMPLE_CSAA_16X,
	MULTISAMPLE_CSAA_16XQ,
};

// NEVER inherit from this class.
struct Config
{
    Config();
    void Load();
	void GameIniLoad();
    void Save();
	void UpdateProjectionHack();

    // General
    bool bFullscreen;
    bool bHideCursor;
    bool renderToMainframe;
	bool bVSync;

	// Resolution control
	char iFSResolution[16];
    char iInternalRes[16];

    bool bNativeResolution, b2xResolution;  // Should possibly be augmented with 2x, 4x native.
    bool bKeepAR43, bKeepAR169, bCrop;   // Aspect ratio controls.
    bool bUseXFB;
    bool bAutoScale;  // Removes annoying borders without using XFB. Doesn't always work perfectly.
    
	// Enhancements
    int iMultisampleMode;
    bool bForceFiltering;
    int iMaxAnisotropy;
	std::string sPostProcessingShader;

	// Information
    bool bShowFPS;
    bool bOverlayStats;
	bool bOverlayBlendStats;
	bool bOverlayProjStats;
    bool bTexFmtOverlayEnable;
    bool bTexFmtOverlayCenter;
	bool bShowEFBCopyRegions;
    
    // Render
    bool bWireFrame;
    bool bDisableLighting;
    bool bDisableTexturing;
    bool bDstAlphaPass;
	bool bDisableFog;
    
    // Utility
    bool bDumpTextures;
	bool bHiresTextures;
	bool bDumpEFBTarget;
	bool bDumpFrames;
    bool bFreeLook;
    
    // Hacks
    bool bEFBCopyDisable;
    bool bEFBCopyDisableHotKey;
	bool bHack;
	bool bCopyEFBToRAM;
    bool bSafeTextureCache;
	int iPhackvalue;
	bool bPhackvalue1, bPhackvalue2;
	float fhackvalue1, fhackvalue2;
	bool bProjHack1;

    int iLog; // CONF_ bits
    int iSaveTargetId;
    
    //currently unused:
    int iCompileDLsLevel;
    bool bShowShaderErrors;

private:
	DISALLOW_COPY_AND_ASSIGN(Config);
};

extern Config g_Config;

#endif  // _PLUGIN_VIDEOOGL_CONFIG_H_
