// Copyright (C) 2003 Dolphin Project.

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

#ifndef _COREPARAMETER_H
#define _COREPARAMETER_H

#include "IniFile.h"
#include <string>

enum Hotkey {
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

	HK_WIIMOTE1_CONNECT,
	HK_WIIMOTE2_CONNECT,
	HK_WIIMOTE3_CONNECT,
	HK_WIIMOTE4_CONNECT,

	HK_LOAD_STATE_SLOT_1,
	HK_LOAD_STATE_SLOT_2,
	HK_LOAD_STATE_SLOT_3,
	HK_LOAD_STATE_SLOT_4,
	HK_LOAD_STATE_SLOT_5,
	HK_LOAD_STATE_SLOT_6,
	HK_LOAD_STATE_SLOT_7,
	HK_LOAD_STATE_SLOT_8,

	HK_SAVE_STATE_SLOT_1,
	HK_SAVE_STATE_SLOT_2,
	HK_SAVE_STATE_SLOT_3,
	HK_SAVE_STATE_SLOT_4,
	HK_SAVE_STATE_SLOT_5,
	HK_SAVE_STATE_SLOT_6,
	HK_SAVE_STATE_SLOT_7,
	HK_SAVE_STATE_SLOT_8,

	NUM_HOTKEYS,
};

struct SCoreStartupParameter
{
	void* hInstance;  // HINSTANCE but we don't want to include <windows.h>

	// Settings
	bool bEnableDebugging;
	bool bAutomaticStart;
	bool bBootToPause;

	// 0 = Interpreter
	// 1 = Jit
	// 2 = JitIL
	int iCPUCore;

	// JIT (shared between JIT and JITIL)
	bool bJITNoBlockCache, bJITBlockLinking;
	bool bJITOff;
	bool bJITLoadStoreOff, bJITLoadStorelXzOff, bJITLoadStorelwzOff, bJITLoadStorelbzxOff;
	bool bJITLoadStoreFloatingOff;
	bool bJITLoadStorePairedOff;
	bool bJITFloatingPointOff;
	bool bJITIntegerOff;
	bool bJITPairedOff;
	bool bJITSystemRegistersOff;
	bool bJITBranchOff;
	bool bJITProfiledReJIT;
	bool bJITILTimeProfiling;
	bool bJITILOutputIR;

	bool bEnableFPRF;

	bool bCPUThread;
	bool bDSPThread;
	bool bDSPHLE;
	bool bSkipIdle;
	bool bNTSC;
	bool bForceNTSCJ;
	bool bHLE_BS2;
	bool bLockThreads;
	bool bEnableCheats;
	bool bMergeBlocks;

	bool bRunCompareServer;
	bool bRunCompareClient;

	bool bMMU;
	bool bMMUBAT;
	int iTLBHack;
	bool bVBeam;
	bool bFastDiscSpeed;

	int SelectedLanguage;

	bool bWii;
	bool bDisableWiimoteSpeaker;

	// Interface settings
	bool bConfirmStop, bHideCursor, bAutoHideCursor, bUsePanicHandlers, bOnScreenDisplayMessages;

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

	int iTheme;
	int iPosX, iPosY, iWidth, iHeight;
	
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

	// files
	std::string m_strFilename;
	std::string m_strBootROM;
	std::string m_strSRAM;
	std::string m_strDefaultGCM;
	std::string m_strDVDRoot;
	std::string m_strApploader;
	std::string m_strUniqueID;
	std::string m_strName;
	std::string m_strGameIni;

	// Constructor just calls LoadDefaults
	SCoreStartupParameter();

	void LoadDefaults();
	bool AutoSetup(EBootBS2 _BootBS2);
	const std::string &GetUniqueID() const { return m_strUniqueID; }
	void CheckMemcardPath(std::string& memcardPath, std::string Region, bool isSlotA);
};

#endif
