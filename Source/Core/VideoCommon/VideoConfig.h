// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
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
	SCALE_3X,
	SCALE_4X,
};

enum StereoMode
{
	STEREO_OFF = 0,
	STEREO_SBS,
	STEREO_TAB,
	STEREO_ANAGLYPH,
	STEREO_3DVISION,
	STEREO_OCULUS,
	STEREO_VR920,
};

enum TGameCamera
{
	CAMERA_YAWPITCHROLL = 0,
	CAMERA_YAWPITCH,
	CAMERA_YAW,
	CAMERA_NONE
};

// NEVER inherit from this class.
struct VideoConfig final
{
	VideoConfig();
	void Load(const std::string& ini_file);
	void LoadVR(const std::string& ini_file);
	void GameIniLoad();
	void GameIniSave();
	void GameIniReset();
	void VerifyValidity();
	void Save(const std::string& ini_file);
	void SaveVR(const std::string& ini_file);
	void UpdateProjectionHack();
	bool IsVSync();
	bool VRSettingsModified();

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
	int iMultisampleMode;
	int iEFBScale;
	bool bForceFiltering;
	int iMaxAnisotropy;
	std::string sPostProcessingShader;
	int iStereoMode;
	int iStereoDepth;
	int iStereoConvergence;
	bool bStereoSwapEyes;

	// Information
	bool bShowFPS;
	bool bOverlayStats;
	bool bOverlayProjStats;
	bool bTexFmtOverlayEnable;
	bool bTexFmtOverlayCenter;
	bool bShowEFBCopyRegions;
	bool bLogRenderTimeToFile;

	// Render
	bool bWireFrame;
	bool bDstAlphaPass;
	bool bDisableFog;

	// Utility
	bool bDumpTextures;
	bool bHiresTextures;
	bool bConvertHiresTextures;
	bool bDumpEFBTarget;
	bool bUseFFV1;
	bool bFreeLook;
	bool bBorderlessFullscreen;

	// Hacks
	bool bEFBAccessEnable;
	bool bPerfQueriesEnable;

	bool bEFBCopyEnable;
	bool bEFBCopyClearDisable;
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
	bool bStereoEFBMonoDepth;
	int iStereoDepthPercentage;
	int iStereoConvergenceMinimum;

	// VR global
	float fScale;
	float fLeanBackAngle;
	bool bStabilizeRoll;
	bool bStabilizePitch;
	bool bStabilizeYaw;
	bool bStabilizeX;
	bool bStabilizeY;
	bool bStabilizeZ;
	bool bKeyhole;
	float fKeyholeWidth;
	bool bKeyholeSnap;
	float fKeyholeSnapSize;
	bool bPullUp20fps;
	bool bPullUp30fps;
	bool bPullUp60fps;
	bool bSynchronousTimewarp;
	bool bOpcodeWarningDisable;
	bool bPullUp20fpsTimewarp;
	bool bPullUp30fpsTimewarp;
	bool bPullUp60fpsTimewarp;
	bool bOpcodeReplay;
	bool bAsynchronousTimewarp;
	bool bEnableVR;
	bool bLowPersistence;
	bool bDynamicPrediction;
	bool bNoMirrorToWindow;
	bool bOrientationTracking;
	bool bMagYawCorrection;
	bool bPositionTracking;
	bool bChromatic;
	bool bTimewarp;
	bool bVignette;
	bool bNoRestore;
	bool bFlipVertical;
	bool bSRGB;
	bool bOverdrive;
	bool bHqDistortion;
	bool bDisableNearClipping;
	bool bShowHands;
	bool bShowFeet;
	bool bShowController;
	bool bShowSensorBar;
	bool bShowGameCamera;
	bool bShowGameFrustum;
	bool bShowTrackingCamera;
	bool bShowTrackingVolume;
	bool bShowBaseStation;

	bool bMotionSicknessAlways;
	bool bMotionSicknessFreelook;
	bool bMotionSickness2D;
	bool bMotionSicknessLeftStick;
	bool bMotionSicknessRightStick;
	bool bMotionSicknessDPad;
	bool bMotionSicknessIR;
	int iMotionSicknessMethod;
	int iMotionSicknessSkybox;
	float fMotionSicknessFOV;

	int iVRPlayer;
	float fTimeWarpTweak;
	u32 iExtraTimewarpedFrames;
	u32 iExtraVideoLoops;
	u32 iExtraVideoLoopsDivider;

	// VR per game
	float fUnitsPerMetre;
	float fFreeLookSensitivity;
	float fHudThickness;
	float fHudDistance;
	float fHud3DCloser;
	float fCameraForward;
	float fCameraPitch;
	float fAimDistance;
	float fMinFOV;
	float fScreenHeight;
	float fScreenThickness;
	float fScreenDistance;
	float fScreenRight;
	float fScreenUp;
	float fScreenPitch;
	float fTelescopeMaxFOV;
	float fReadPitch;
	bool bDisable3D;
	bool bHudFullscreen;
	bool bHudOnTop;
	bool bDontClearScreen;
	bool bCanReadCameraAngles;
	bool bDetectSkybox;
	int iTelescopeEye;
	int iMetroidPrime;
	// VR layer debugging
	int iSelectedLayer;
	int iFlashState;

	// D3D only config, mostly to be merged into the above
	int iAdapter;

	// Debugging
	bool bEnableShaderDebugging;

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
	} backend_info;

	// Utility
	bool RealXFBEnabled() const { return bUseXFB && bUseRealXFB; }
	bool VirtualXFBEnabled() const { return bUseXFB && !bUseRealXFB; }
	bool EFBCopiesToTextureEnabled() const { return bEFBCopyEnable && bSkipEFBCopyToRam; }
	bool EFBCopiesToRamEnabled() const { return bEFBCopyEnable && !bSkipEFBCopyToRam; }
	bool ExclusiveFullscreenEnabled() const { return backend_info.bSupportsExclusiveFullscreen && !bBorderlessFullscreen; }
};

extern VideoConfig g_Config;
extern VideoConfig g_ActiveConfig;

// Called every frame.
void UpdateActiveConfig();
