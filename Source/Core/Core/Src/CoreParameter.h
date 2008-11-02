// Copyright (C) 2003-2008 Dolphin Project.

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

#ifdef _WIN32
#include <windows.h>
#endif

#include <string>


struct SCoreStartupParameter
{
#ifdef _WIN32
	HINSTANCE hInstance;
#endif

	// windows/GUI related
	void* hMainWindow;

	// flags
	bool bEnableDebugging;
	bool bUseJIT;
	bool bJITOff;
	bool bJITLoadStoreOff;
	bool bJITLoadStoreFloatingOff;
	bool bJITLoadStorePairedOff;
	bool bJITFloatingPointOff;
	bool bJITIntegerOff;
	bool bJITPairedOff;
	bool bJITSystemRegistersOff;
	bool bUseDualCore;
	bool bSkipIdle;
	bool bNTSC;
	bool bHLEBios;
	bool bUseFastMem;
	bool bLockThreads;
	bool bOptimizeQuantizers;
	bool bEnableCheats;
	bool bEnableIsoCache;

	bool bRunCompareServer;
	bool bRunCompareClient;
	int  SelectedLanguage;

	bool bWii;

	enum EBootBios
	{
		BOOT_DEFAULT,
		BOOT_BIOS_JAP,
		BOOT_BIOS_USA,
		BOOT_BIOS_EUR,
	};

	enum EBootType
	{
		BOOT_ISO,
		BOOT_ELF,
		BOOT_DOL,
		BOOT_BIOS
	};
	EBootType m_BootType;

	// files
	std::string m_strVideoPlugin;
	std::string m_strPadPlugin;
	std::string m_strDSPPlugin;
	std::string m_strWiimotePlugin;

	std::string m_strFilename;
	std::string m_strBios;	
	std::string m_strMemoryCardA;
	std::string m_strMemoryCardB;
	std::string m_strSRAM;
	std::string m_strDefaultGCM;
	std::string m_strDVDRoot;
	std::string m_strUniqueID;

	// 
	SCoreStartupParameter();

	void LoadDefaults();
	bool AutoSetup(EBootBios _BootBios);
	const std::string &GetUniqueID() const { return m_strUniqueID; }
};

#endif

