// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/IniFile.h"

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

	HK_TOGGLE_IR,
	HK_TOGGLE_AR,
	HK_TOGGLE_EFBCOPIES,
	HK_TOGGLE_FOG,
	HK_TOGGLE_THROTTLE,

	HK_INCREASE_FRAME_LIMIT,
	HK_DECREASE_FRAME_LIMIT,

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
	#endif
	bool bAutomaticStart;
	bool bBootToPause;

	enum
	{
		CORE_INTERPRETER,
		CORE_JIT64,
		CORE_JITIL64,
		CORE_JITARM,
		CORE_JITARM64
	};
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
	bool bEnableFPRF;

	bool bCPUThread;
	bool bDSPThread;
	bool bDSPHLE;
	bool bSkipIdle;
	bool bNTSC;
	bool bForceNTSCJ;
	bool bHLE_BS2;
	bool bEnableCheats;
	bool bMergeBlocks;
	bool bEnableMemcardSaving;

	bool bDPL2Decoder;
	int iLatency;

	bool bRunCompareServer;
	bool bRunCompareClient;

	bool bMMU;
	bool bDCBZOFF;
	int iBBDumpPort;
	bool bVBeamSpeedHack;
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

	// Display settings
	std::string strFullscreenResolution;
	int iRenderWindowXPos, iRenderWindowYPos;
	int iRenderWindowWidth, iRenderWindowHeight;
	bool bRenderWindowAutoSize, bKeepWindowOnTop;
	bool bFullscreen, bRenderToMain;
	bool bProgressive, bDisableScreenSaver;

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
	std::string m_strDefaultGCM;
	std::string m_strDVDRoot;
	std::string m_strApploader;
	std::string m_strUniqueID;
	std::string m_strRevisionSpecificUniqueID;
	std::string m_strName;
	std::string m_strGameIniDefault;
	std::string m_strGameIniDefaultRevisionSpecific;
	std::string m_strGameIniLocal;

	// Constructor just calls LoadDefaults
	SCoreStartupParameter();

	void LoadDefaults();
	bool AutoSetup(EBootBS2 _BootBS2);
	const std::string &GetUniqueID() const { return m_strUniqueID; }
	void CheckMemcardPath(std::string& memcardPath, std::string Region, bool isSlotA);
	IniFile LoadDefaultGameIni() const;
	IniFile LoadLocalGameIni() const;
	IniFile LoadGameIni() const;
};
