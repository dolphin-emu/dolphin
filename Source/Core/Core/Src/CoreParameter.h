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
	HK_FULLSCREEN,
	HK_PLAY_PAUSE,
	HK_STOP,
	HK_SCREENSHOT,
	HK_WIIMOTE1_CONNECT,
	HK_WIIMOTE2_CONNECT,
	HK_WIIMOTE3_CONNECT,
	HK_WIIMOTE4_CONNECT,
	NUM_HOTKEYS,
};

struct SCoreStartupParameter
{
	void* hInstance;  // HINSTANCE but we don't want to include <windows.h>

	// Windows/GUI related
	void* hMainWindow;

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

	bool bEnableFPRF;

	bool bCPUThread;
	bool bDSPThread;
	bool bSkipIdle;
	bool bNTSC;
	bool bHLE_BS2;
	bool bEnableOpenCL;
	bool bUseFastMem;
	bool bLockThreads;
	bool bEnableCheats;

	bool bRunCompareServer;
	bool bRunCompareClient;

	bool bMMU;
	int iTLBHack;
	bool bAlternateRFI;

	int SelectedLanguage;

	bool bWii;

	// Interface settings
	bool bConfirmStop, bHideCursor, bAutoHideCursor, bUsePanicHandlers;

	// Hotkeys
	int iHotkey[NUM_HOTKEYS];
	int iHotkeyModifier[NUM_HOTKEYS];

	// Display settings
	std::string strFullscreenResolution;
	int iRenderWindowXPos, iRenderWindowYPos;
	int iRenderWindowWidth, iRenderWindowHeight;
	bool bFullscreen, bRenderToMain;

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
		BOOT_BS2
	};
	EBootType m_BootType;

	// files
	std::string m_strVideoPlugin;
	std::string m_strDSPPlugin;
	std::string m_strWiimotePlugin;

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
