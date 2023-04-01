// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdlib>
#include <string>

#include "DiscIO/Volume.h"

struct CountrySetting
{
	const std::string area;
	const std::string video;
	const std::string game;
	const std::string code;
};

class CBoot
{
public:

	static bool BootUp();
	static bool IsElfWii(const std::string& filename);

	// Tries to find a map file for the current game by looking first in the
	// local user directory, then in the shared user directory.
	//
	// If existing_map_file is not nullptr and a map file exists, it is set to the
	// path to the existing map file.
	//
	// If writable_map_file is not nullptr, it is set to the path to where a map
	// file should be saved.
	//
	// If title_id is not nullptr, it is set to the title id
	//
	// Returns true if a map file exists, false if none could be found.
	static bool FindMapFile(std::string* existing_map_file,
	                        std::string* writable_map_file,
	                        std::string* title_id = nullptr);

private:
	static bool DVDRead(u64 dvd_offset, u32 output_address, u32 length, bool decrypt);
	static void RunFunction(u32 _iAddr);

	static void UpdateDebugger_MapLoaded();

	static bool LoadMapFromFilename();
	static bool Boot_ELF(const std::string& filename);
	static bool Boot_WiiWAD(const std::string& filename);

	static bool EmulatedBS2_GC(bool skipAppLoader = false);
	static bool EmulatedBS2_Wii();
	static bool EmulatedBS2(bool _bIsWii);
	static bool Load_BS2(const std::string& _rBootROMFilename);
	static void Load_FST(bool _bIsWii);

	static bool SetupWiiMemory(DiscIO::IVolume::ECountry country);
};
