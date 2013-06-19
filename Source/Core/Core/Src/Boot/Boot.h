// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _BOOT_H
#define _BOOT_H

#include <string>

#include "Common.h"
#include "../CoreParameter.h"

class CBoot
{
public:

	static bool BootUp();
	static bool IsElfWii(const char *filename);
	static std::string GenerateMapFilename();

private:
	static void RunFunction(u32 _iAddr);

	static void UpdateDebugger_MapLoaded(const char* _gameID = NULL);

	static bool LoadMapFromFilename(const std::string& _rFilename, const char* _gameID = NULL);
	static bool Boot_ELF(const char *filename);
	static bool Boot_WiiWAD(const char *filename);

	static bool EmulatedBS2_GC();
	static bool EmulatedBS2_Wii();
	static bool EmulatedBS2(bool _bIsWii);
	static bool Load_BS2(const std::string& _rBootROMFilename);
	static void Load_FST(bool _bIsWii);

	static bool SetupWiiMemory(unsigned int _CountryCode);
};

#endif
