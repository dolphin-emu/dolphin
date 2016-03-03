// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.


// IMPORTANT: UI etc should modify g_Config. Graphics code should read g_ActiveConfig.
// The reason for this is to get rid of race conditions etc when the configuration
// changes in the middle of a frame. This is done by copying g_Config to g_ActiveConfig
// at the start of every frame. Noone should ever change members of g_ActiveConfig
// directly.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "VideoCommon/VideoCommon.h"

// Log in two categories, and save three other options in the same byte
#define CONF_LOG          1
#define CONF_PRIMLOG      2
#define CONF_SAVETARGETS  8
#define CONF_SAVESHADERS  16

enum AspectMode
{
	ASPECT_AUTO        = 0,
	ASPECT_ANALOG_WIDE = 1,
	ASPECT_ANALOG      = 2,
	ASPECT_STRETCH     = 3,
};

enum EFBScale
{
	SCALE_FORCE_INTEGRAL = -1,
	SCALE_AUTO,
	SCALE_AUTO_INTEGRAL,
	SCALE_1X,
	SCALE_1_5X,
	SCALE_2X,
	SCALE_2_5X,
};

enum StereoMode
{
	STEREO_OFF = 0,
	STEREO_SBS,
	STEREO_TAB,
	STEREO_ANAGLYPH,
	STEREO_3DVISION
};

// NEVER inherit from this class.
struct VideoConfig final
{
	VideoConfig();
	void Load(const std::string& ini_file);
	void GameIniLoad();
	void VerifyValidity();
	void Save(const std::string& ini_file);
	void UpdateProjectionHack();
	bool IsVSync();

	// General
	bool bVSync;
	bool bFullscreen;
	bool bExclusiveMode;
	bool bRunning;
	bool bWidescreenHack;
	int iAspectRatio;
	bool bCrop;   // Aspect ratio controls.
	bool bUseXFB;
	bool bUseRealXFB;

	// Enhancements
	int iMultisamples;
	bool bSSAA;
	int iEFBScale;
	bool bForceFiltering;
	int iMaxAnisotropy;
	std::string sPostProcessingShader;

	// Information
	bool bShowFPS;
	bool bOverlayStats;
	bool bOverlayProjStats;
	bool bTexFmtOverlayEnable;
	bool bTexFmtOverlayCenter;
	bool bLogRenderTimeToFile;

	// Render
	bool bWireFrame;
	bool bDisableFog;

	// Utility
	bool bDumpTextures;
	bool bHiresTextures;
	bool bConvertHiresTextures;
	bool bCacheHiresTextures;
	bool bDumpEFBTarget;
	bool bUseFFV1;
	bool bFreeLook;
	bool bBorderlessFullscreen;

	// Hacks
	bool bEFBAccessEnable;
	bool bPerfQueriesEnable;
	bool bBBoxEnable;
	bool bForceProgressive;

	bool bEFBEmulateFormatChanges;
	bool bSkipEFBCopyToRam;
	bool bCopyEFBScaled;
	int iSafeTextureCache_ColorSamples;
	int iPhackvalue[3];
	std::string sPhackvalue[2];
	float fAspectRatioHackW, fAspectRatioHackH;
	bool bEnablePixelLighting;
	bool bFastDepthCalc;
	int iLog; // CONF_ bits
	int iSaveTargetId; // TODO: Should be dropped

	// Stereoscopy
	int iStereoMode;
	int iStereoDepth;
	int iStereoConvergence;
	int iStereoConvergencePercentage;
	bool bStereoSwapEyes;
	bool bStereoEFBMonoDepth;
	int iStereoDepthPercentage;

	// D3D only config, mostly to be merged into the above
	int iAdapter;

	// Debugging
	bool bEnableShaderDebugging;

	// VideoSW Debugging
	int drawStart;
	int drawEnd;
	bool bZComploc;
	bool bZFreeze;
	bool bDumpObjects;
	bool bDumpTevStages;
	bool bDumpTevTextureFetches;

	// Static config per API
	// TODO: Move this out of VideoConfig
	struct
	{
		API_TYPE APIType;

		std::vector<std::string> Adapters; // for D3D
		std::vector<int> AAModes;
		std::vector<std::string> PPShaders; // post-processing shaders
		std::vector<std::string> AnaglyphShaders; // anaglyph shaders

		bool bSupportsExclusiveFullscreen;
		bool bSupportsDualSourceBlend;
		bool bSupportsPrimitiveRestart;
		bool bSupportsOversizedViewports;
		bool bSupportsGeometryShaders;
		bool bSupports3DVision;
		bool bSupportsEarlyZ; // needed by PixelShaderGen, so must stay in VideoCommon
		bool bSupportsBindingLayout; // Needed by ShaderGen, so must stay in VideoCommon
		bool bSupportsBBox;
		bool bSupportsGSInstancing; // Needed by GeometryShaderGen, so must stay in VideoCommon
		bool bSupportsPostProcessing;
		bool bSupportsPaletteConversion;
		bool bSupportsClipControl; // Needed by VertexShaderGen, so must stay in VideoCommon
		bool bSupportsSSAA;
	} backend_info;

	// Utility
	bool RealXFBEnabled() const { return bUseXFB && bUseRealXFB; }
	bool VirtualXFBEnabled() const { return bUseXFB && !bUseRealXFB; }
	bool ExclusiveFullscreenEnabled() const { return backend_info.bSupportsExclusiveFullscreen && !bBorderlessFullscreen; }
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
