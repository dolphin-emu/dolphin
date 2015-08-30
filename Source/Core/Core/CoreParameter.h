// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// When enabled you can isolate a single object using the object removal GUI.
// Great for debugging corruption or using the D3D debugger.
//#define DEBUG_OBJECTS

#include <string>
#include <vector>

#include "Common/IniFile.h"
#include "DiscIO/Volume.h"

typedef std::vector<u8> SkipEntry;

enum Hotkey
{
	HK_OPEN,
	HK_CHANGE_DISC,
	HK_REFRESH_LIST,

	HK_PLAY_PAUSE,
	HK_STOP,
	HK_RESET,
	HK_FRAME_ADVANCE,

	HK_START_RECORDING,
	HK_PLAY_RECORDING,
	HK_EXPORT_RECORDING,
	HK_READ_ONLY_MODE,

	HK_FULLSCREEN,
	HK_SCREENSHOT,
	HK_EXIT,

	HK_WIIMOTE1_CONNECT,
	HK_WIIMOTE2_CONNECT,
	HK_WIIMOTE3_CONNECT,
	HK_WIIMOTE4_CONNECT,
	HK_BALANCEBOARD_CONNECT,

	HK_VOLUME_DOWN,
	HK_VOLUME_UP,
	HK_VOLUME_TOGGLE_MUTE,

	HK_INCREASE_IR,
	HK_DECREASE_IR,

	HK_TOGGLE_IR,
	HK_TOGGLE_AR,
	HK_TOGGLE_EFBCOPIES,
	HK_TOGGLE_FOG,
	HK_TOGGLE_THROTTLE,

	HK_DECREASE_FRAME_LIMIT,
	HK_INCREASE_FRAME_LIMIT,

	HK_FREELOOK_DECREASE_SPEED,
	HK_FREELOOK_INCREASE_SPEED,
	HK_FREELOOK_RESET_SPEED,
	HK_FREELOOK_UP,
	HK_FREELOOK_DOWN,
	HK_FREELOOK_LEFT,
	HK_FREELOOK_RIGHT,
	HK_FREELOOK_ZOOM_IN,
	HK_FREELOOK_ZOOM_OUT,
	HK_FREELOOK_RESET,

	HK_DECREASE_DEPTH,
	HK_INCREASE_DEPTH,
	HK_DECREASE_CONVERGENCE,
	HK_INCREASE_CONVERGENCE,

	HK_LOAD_STATE_SLOT_1,
	HK_LOAD_STATE_SLOT_2,
	HK_LOAD_STATE_SLOT_3,
	HK_LOAD_STATE_SLOT_4,
	HK_LOAD_STATE_SLOT_5,
	HK_LOAD_STATE_SLOT_6,
	HK_LOAD_STATE_SLOT_7,
	HK_LOAD_STATE_SLOT_8,
	HK_LOAD_STATE_SLOT_9,
	HK_LOAD_STATE_SLOT_10,

	HK_SAVE_STATE_SLOT_1,
	HK_SAVE_STATE_SLOT_2,
	HK_SAVE_STATE_SLOT_3,
	HK_SAVE_STATE_SLOT_4,
	HK_SAVE_STATE_SLOT_5,
	HK_SAVE_STATE_SLOT_6,
	HK_SAVE_STATE_SLOT_7,
	HK_SAVE_STATE_SLOT_8,
	HK_SAVE_STATE_SLOT_9,
	HK_SAVE_STATE_SLOT_10,

	HK_SELECT_STATE_SLOT_1,
	HK_SELECT_STATE_SLOT_2,
	HK_SELECT_STATE_SLOT_3,
	HK_SELECT_STATE_SLOT_4,
	HK_SELECT_STATE_SLOT_5,
	HK_SELECT_STATE_SLOT_6,
	HK_SELECT_STATE_SLOT_7,
	HK_SELECT_STATE_SLOT_8,
	HK_SELECT_STATE_SLOT_9,
	HK_SELECT_STATE_SLOT_10,

	HK_SAVE_STATE_SLOT_SELECTED,
	HK_LOAD_STATE_SLOT_SELECTED,

	HK_LOAD_LAST_STATE_1,
	HK_LOAD_LAST_STATE_2,
	HK_LOAD_LAST_STATE_3,
	HK_LOAD_LAST_STATE_4,
	HK_LOAD_LAST_STATE_5,
	HK_LOAD_LAST_STATE_6,
	HK_LOAD_LAST_STATE_7,
	HK_LOAD_LAST_STATE_8,

	HK_SAVE_FIRST_STATE,
	HK_UNDO_LOAD_STATE,
	HK_UNDO_SAVE_STATE,
	HK_SAVE_STATE_FILE,
	HK_LOAD_STATE_FILE,

	NUM_HOTKEYS,
};

enum VRFreelook
{
    VR_POSITION_RESET,
	VR_CAMERA_FORWARD,
	VR_CAMERA_BACKWARD,
	VR_CAMERA_LEFT,
	VR_CAMERA_RIGHT,
	VR_CAMERA_UP,
	VR_CAMERA_DOWN,

	VR_PERMANENT_CAMERA_FORWARD,
	VR_PERMANENT_CAMERA_BACKWARD,
	VR_LARGER_SCALE,
	VR_SMALLER_SCALE,
	VR_GLOBAL_LARGER_SCALE,
	VR_GLOBAL_SMALLER_SCALE,
	VR_CAMERA_TILT_UP,
	VR_CAMERA_TILT_DOWN,

	VR_HUD_FORWARD,
	VR_HUD_BACKWARD,
	VR_HUD_THICKER,
	VR_HUD_THINNER,
	VR_HUD_3D_CLOSER,
	VR_HUD_3D_FURTHER,

	VR_2D_SCREEN_LARGER,
	VR_2D_SCREEN_SMALLER,
	VR_2D_CAMERA_FORWARD,
	VR_2D_CAMERA_BACKWARD,
	//VR_2D_SCREEN_LEFT, //DOESN'T_EXIST_RIGHT_NOW?
	//VR_2D_SCREEN_RIGHT, //DOESN'T_EXIST_RIGHT_NOW?
	VR_2D_CAMERA_UP,
	VR_2D_CAMERA_DOWN,
	VR_2D_CAMERA_TILT_UP,
	VR_2D_CAMERA_TILT_DOWN,
	VR_2D_SCREEN_THICKER,
	VR_2D_SCREEN_THINNER,

	NUM_VR_HOTKEYS,
};

enum GPUDeterminismMode
{
	GPU_DETERMINISM_AUTO,
	GPU_DETERMINISM_NONE,
	// This is currently the only mode.  There will probably be at least
	// one more at some point.
	GPU_DETERMINISM_FAKE_COMPLETION,
};

struct SCoreStartupParameter
{
	// Settings
	bool bEnableDebugging;
	#ifdef USE_GDBSTUB
	int  iGDBPort;
	#ifndef _WIN32
	std::string gdb_socket;
	#endif
	#endif
	bool bAutomaticStart;
	bool bBootToPause;

	int iCPUCore;

	// JIT (shared between JIT and JITIL)
	bool bJITNoBlockCache, bJITNoBlockLinking;
	bool bJITOff;
	bool bJITLoadStoreOff, bJITLoadStorelXzOff, bJITLoadStorelwzOff, bJITLoadStorelbzxOff;
	bool bJITLoadStoreFloatingOff;
	bool bJITLoadStorePairedOff;
	bool bJITFloatingPointOff;
	bool bJITIntegerOff;
	bool bJITPairedOff;
	bool bJITSystemRegistersOff;
	bool bJITBranchOff;
	bool bJITILTimeProfiling;
	bool bJITILOutputIR;

	bool bFastmem;
	bool bFPRF;

	bool bCPUThread;
	bool bDSPThread;
	bool bDSPHLE;
	bool bSkipIdle;
	bool bSyncGPUOnSkipIdleHack;
	bool bNTSC;
	bool bForceNTSCJ;
	bool bHLE_BS2;
	bool bEnableCheats;
	bool bEnableMemcardSaving;
	float fAudioSlowDown;

	bool bDPL2Decoder;
	int iLatency;

	bool bRunCompareServer;
	bool bRunCompareClient;

	bool bMMU;
	bool bDCBZOFF;
	int iBBDumpPort;
	bool bSyncGPU;
	bool bFastDiscSpeed;

	int SelectedLanguage;

	bool bWii;

	// Interface settings
	bool bConfirmStop, bHideCursor, bAutoHideCursor, bUsePanicHandlers, bOnScreenDisplayMessages;
	std::string theme_name;

	// Hotkeys
	int iHotkey[NUM_HOTKEYS];
	int iHotkeyModifier[NUM_HOTKEYS];
	bool bHotkeyKBM[NUM_HOTKEYS];
	bool bHotkeyDInput[NUM_HOTKEYS];
	u32 iHotkeyDandXInputMapping[NUM_HOTKEYS];
	u32 iHotkeyDInputMappingExtra[NUM_HOTKEYS];
	bool bHotkeysXInput;
	float fFreeLookSensitivity;

	// VR Settings
	int iVRSettings[NUM_VR_HOTKEYS];
	int iVRSettingsModifier[NUM_VR_HOTKEYS];
	bool bVRSettingsKBM[NUM_VR_HOTKEYS];
	bool bVRSettingsDInput[NUM_VR_HOTKEYS];
	u32 iVRSettingsDandXInputMapping[NUM_VR_HOTKEYS];
	u32 iVRSettingsDInputMappingExtra[NUM_VR_HOTKEYS];

	//Remove Layer
	std::vector<SkipEntry> object_removal_codes;
	u32 skip_objects_end = 0;
	u32 skip_objects_start = 0;
#ifdef DEBUG_OBJECTS
	u32 skip_objects_end_two = 0;
	u32 skip_objects_start_two = 0;
#endif
	volatile bool hide_objects_updating = true;
	volatile bool hide_objects_done = true;

	// Display settings
	std::string strFullscreenResolution;
	int iRenderWindowXPos, iRenderWindowYPos;
	int iRenderWindowWidth, iRenderWindowHeight;
	bool bRenderWindowAutoSize, bKeepWindowOnTop;
	bool bFullscreen, bRenderToMain;
	bool bProgressive, bDisableScreenSaver;
	bool bAsynchronousTimewarp;

	int iPosX, iPosY, iWidth, iHeight;

	// Fifo Player related settings
	bool bLoopFifoReplay;

	enum EBootBS2
	{
		BOOT_DEFAULT,
		BOOT_BS2_JAP,
		BOOT_BS2_USA,
		BOOT_BS2_EUR,
	};

	enum EBootType
	{
		BOOT_ISO,
		BOOT_ELF,
		BOOT_DOL,
		BOOT_WII_NAND,
		BOOT_BS2,
		BOOT_DFF
	};
	EBootType m_BootType;

	std::string m_strVideoBackend;
	std::string m_strGPUDeterminismMode;

	// set based on the string version
	GPUDeterminismMode m_GPUDeterminismMode;

	// files
	std::string m_strFilename;
	std::string m_strBootROM;
	std::string m_strSRAM;
	std::string m_strDefaultISO;
	std::string m_strDVDRoot;
	std::string m_strApploader;
	std::string m_strUniqueID;
	std::string m_strName;
	int m_revision;

	std::string m_perfDir;

	// Constructor just calls LoadDefaults
	SCoreStartupParameter();

	void LoadDefaults();
	bool AutoSetup(EBootBS2 _BootBS2);
	const std::string &GetUniqueID() const { return m_strUniqueID; }
	void CheckMemcardPath(std::string& memcardPath, std::string gameRegion, bool isSlotA);
	DiscIO::IVolume::ELanguage GetCurrentLanguage(bool wii) const;

	IniFile LoadDefaultGameIni() const;
	IniFile LoadLocalGameIni() const;
	IniFile LoadGameIni() const;

	static IniFile LoadDefaultGameIni(const std::string& id, int revision);
	static IniFile LoadLocalGameIni(const std::string& id, int revision);
	static IniFile LoadGameIni(const std::string& id, int revision);

	static std::vector<std::string> GetGameIniFilenames(const std::string& id, int revision);
};
