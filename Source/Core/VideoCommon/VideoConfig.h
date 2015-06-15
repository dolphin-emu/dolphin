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
#include "Common/IniFile.h"
#include "VideoCommon/VideoCommon.h"

// Log in two categories, and save three other options in the same byte
#define CONF_LOG          1
#define CONF_PRIMLOG      2
#define CONF_SAVETARGETS  8
#define CONF_SAVESHADERS  16

enum AspectMode
{
	ASPECT_AUTO       = 0,
	ASPECT_FORCE_16_9 = 1,
	ASPECT_FORCE_4_3  = 2,
	ASPECT_STRETCH    = 3,
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
	bool IsVSync();

	OptionGroup  m_hardware_group{"Hardware"};
	OptionGroup  m_settings_group{"Settings"};
	OptionGroup  m_enhancements_group{"Enhancements"};
	OptionGroup  m_hacks_group{"Hacks"};
	OptionGroup  m_stereoscopy_group{"Stereoscopy"};

	Option<bool> bVSync {m_hardware_group, "VSync", false};
	Option<int>  iAdapter {m_hardware_group, "Adapter", 0};

	Option<bool> bWidescreenHack {m_settings_group, "wideScreenHack", false};
	Option<int>  iAspectRatio {m_settings_group, "AspectRatio", (int)ASPECT_AUTO};
	Option<bool> bCrop {m_settings_group, "Crop", false};
	Option<bool> bUseXFB {m_settings_group, "UseXFB", 0};
	Option<bool> bUseRealXFB {m_settings_group, "UseRealXFB", 0};
	Option<int>  iSafeTextureCache_ColorSamples {m_settings_group, "SafeTextureCacheColorSamples", 128};
	Option<bool> bShowFPS {m_settings_group, "ShowFPS", false};
	Option<bool> bLogRenderTimeToFile {m_settings_group, "LogRenderTimeToFile", false};
	Option<bool> bOverlayStats {m_settings_group, "OverlayStats", false};
	Option<bool> bOverlayProjStats {m_settings_group, "OverlayProjStats", false};
	Option<bool> bShowEFBCopyRegions {m_settings_group, "ShowEFBCopyRegions", false};
	Option<bool> bDumpTextures {m_settings_group, "DumpTextures", false};
	Option<bool> bHiresTextures {m_settings_group, "HiresTextures", false};
	Option<bool> bConvertHiresTextures {m_settings_group, "ConvertHiresTextures", false};
	Option<bool> bCacheHiresTextures {m_settings_group, "CacheHiresTextures", false};
	Option<bool> bDumpEFBTarget {m_settings_group, "DumpEFBTarget", false};
	Option<bool> bFreeLook {m_settings_group, "FreeLook", false};
	Option<bool> bUseFFV1 {m_settings_group, "UseFFV1", false};
	Option<bool> bEnablePixelLighting {m_settings_group, "EnablePixelLighting", false};
	Option<bool> bFastDepthCalc {m_settings_group, "FastDepthCalc", true};
	Option<int>  iMultisampleMode {m_settings_group, "MSAA", 0};
	Option<int>  iEFBScale {m_settings_group, "EFBScale", (int)SCALE_1X};
	Option<bool> bDstAlphaPass {m_settings_group, "DstAlphaPass", false};
	Option<bool> bTexFmtOverlayEnable {m_settings_group, "TexFmtOverlayEnable", false};
	Option<bool> bTexFmtOverlayCenter {m_settings_group, "TexFmtOverlayCenter", false};
	Option<bool> bWireFrame {m_settings_group, "WireFrame", false};
	Option<bool> bDisableFog {m_settings_group, "DisableFog", false};
	Option<bool> bEnableShaderDebugging {m_settings_group, "EnableShaderDebugging", false};
	Option<bool> bBorderlessFullscreen {m_settings_group, "BorderlessFullscreen", false};

	Option<bool> bForceFiltering {m_enhancements_group, "ForceFiltering", false};
	Option<int>  iMaxAnisotropy {m_enhancements_group, "MaxAnisotropy", 0};
	Option<int>  iStereoMode {m_enhancements_group, "StereoMode", 0};
	Option<int>  iStereoDepth {m_enhancements_group, "StereoDepth", 20};
	Option<int>  iStereoConvergence {m_enhancements_group, "StereoConvergence", 20};
	Option<bool> bStereoSwapEyes {m_enhancements_group, "StereoSwapEyes", false};
	Option<std::string> sPostProcessingShader {m_enhancements_group, "PostProcessingShader", ""};

	Option<bool> bEFBAccessEnable {m_hacks_group, "EFBAccessEnable", true};
	Option<bool> bBBoxEnable {m_hacks_group, "BBoxEnable", false};
	Option<bool> bSkipEFBCopyToRam {m_hacks_group, "EFBToTextureEnable", true};
	Option<bool> bCopyEFBScaled {m_hacks_group, "EFBScaledCopy", true};
	Option<bool> bEFBEmulateFormatChanges {m_hacks_group, "EFBEmulateFormatChanges", false};
	Option<bool> bPerfQueriesEnable {m_hacks_group, "PerfQueriesEnable", false};
	Option<bool> bPhackvalue0 {m_hacks_group, "ProjectionHack", false};
	Option<bool> bPhackvalue1 {m_hacks_group, "PH_SZNear", false};
	Option<bool> bPhackvalue2 {m_hacks_group, "PH_SZFar", false};
	Option<std::string> sPhackvalue0 {m_hacks_group, "PH_ZNear", ""};
	Option<std::string> sPhackvalue1 {m_hacks_group, "PH_ZFar", ""};

	Option<bool> bStereoEFBMonoDepth {m_stereoscopy_group, "StereoEFBMonoDepth", false};
	Option<int>  iStereoDepthPercentage {m_stereoscopy_group, "StereoDepthPercentage", 100};
	Option<int>  iStereoConvergenceMinimum {m_stereoscopy_group, "StereoConvergenceMinimum", 0};

	// General
	bool bFullscreen;
	bool bExclusiveMode;

	float fAspectRatioHackW, fAspectRatioHackH;
	int iLog; // CONF_ bits
	int iSaveTargetId; // TODO: Should be dropped

	// Static config per API
	// TODO: Move this out of VideoConfig
	struct
	{
		API_TYPE APIType;

		std::vector<std::string> Adapters; // for D3D
		std::vector<std::string> AAModes;
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
	} backend_info;

	// Utility
	bool RealXFBEnabled() { return bUseXFB && bUseRealXFB; }
	bool VirtualXFBEnabled() { return bUseXFB && !bUseRealXFB; }
	bool ExclusiveFullscreenEnabled() { return backend_info.bSupportsExclusiveFullscreen && !bBorderlessFullscreen; }
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
