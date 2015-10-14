// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <cstdio>
#include <fstream>
#include <string>
#include <utility>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/NandPaths.h"
#include "Common/StringUtil.h"
#include "Common/Logging/Log.h"

namespace Common
{

static std::string s_temp_wii_root;

void InitializeWiiRoot(bool use_dummy)
{
	ShutdownWiiRoot();
	if (use_dummy)
	{
		s_temp_wii_root = File::CreateTempDir();
		if (s_temp_wii_root.empty())
		{
			ERROR_LOG(WII_IPC_FILEIO, "Could not create temporary directory");
			return;
		}
		File::CopyDir(File::GetSysDirectory() + WII_USER_DIR, s_temp_wii_root);
		WARN_LOG(WII_IPC_FILEIO, "Using temporary directory %s for minimal Wii FS", s_temp_wii_root.c_str());
		static bool s_registered;
		if (!s_registered)
		{
			s_registered = true;
			atexit(ShutdownWiiRoot);
		}
		File::SetUserPath(D_SESSION_WIIROOT_IDX, s_temp_wii_root);
	}
	else
	{
		File::SetUserPath(D_SESSION_WIIROOT_IDX, File::GetUserPath(D_WIIROOT_IDX));
	}
}

void ShutdownWiiRoot()
{
	if (!s_temp_wii_root.empty())
	{
		File::DeleteDirRecursively(s_temp_wii_root);
		s_temp_wii_root.clear();
	}
}

std::string GetTicketFileName(u64 _titleID)
{
	return StringFromFormat("%s/ticket/%08x/%08x.tik",
			File::GetUserPath(D_SESSION_WIIROOT_IDX).c_str(),
			(u32)(_titleID >> 32), (u32)_titleID);
}

std::string GetTitleDataPath(u64 _titleID)
{
	return StringFromFormat("%s/title/%08x/%08x/data/",
			File::GetUserPath(D_SESSION_WIIROOT_IDX).c_str(),
			(u32)(_titleID >> 32), (u32)_titleID);
}

std::string GetTMDFileName(u64 _titleID)
{
	return GetTitleContentPath(_titleID) + "title.tmd";
}
std::string GetTitleContentPath(u64 _titleID, int wiiRootIndex)
{
	return StringFromFormat("%s/title/%08x/%08x/content/",
			File::GetUserPath(wiiRootIndex).c_str(),
			(u32)(_titleID >> 32), (u32)_titleID);
}

bool CheckTitleTMD(u64 _titleID)
{
	const std::string TitlePath = GetTMDFileName(_titleID);
	if (File::Exists(TitlePath))
	{
		File::IOFile pTMDFile(TitlePath, "rb");
		u64 TitleID = 0;
		pTMDFile.Seek(0x18C, SEEK_SET);
		if (pTMDFile.ReadArray(&TitleID, 1) && _titleID == Common::swap64(TitleID))
			return true;
	}
	INFO_LOG(DISCIO, "Invalid or no tmd for title %08x %08x", (u32)(_titleID >> 32), (u32)(_titleID & 0xFFFFFFFF));
	return false;
}

bool CheckTitleTIK(u64 _titleID)
{
	const std::string ticketFileName = Common::GetTicketFileName(_titleID);
	if (File::Exists(ticketFileName))
	{
		File::IOFile pTIKFile(ticketFileName, "rb");
		u64 TitleID = 0;
		pTIKFile.Seek(0x1dC, SEEK_SET);
		if (pTIKFile.ReadArray(&TitleID, 1) && _titleID == Common::swap64(TitleID))
			return true;
	}
	INFO_LOG(DISCIO, "Invalid or no tik for title %08x %08x", (u32)(_titleID >> 32), (u32)(_titleID & 0xFFFFFFFF));
	return false;
}

static void CreateReplacementFile(std::string &filename)
{
	std::ofstream replace;
	OpenFStream(replace, filename, std::ios_base::out);
	replace <<"\" __22__\n";
	replace << "* __2a__\n";
	//replace << "/ __2f__\n";
	replace << ": __3a__\n";
	replace << "< __3c__\n";
	replace << "> __3e__\n";
	replace << "? __3f__\n";
	//replace <<"\\ __5c__\n";
	replace << "| __7c__\n";
}

void ReadReplacements(replace_v& replacements)
{
	replacements.clear();
	const std::string replace_fname = "/sys/replace";
	std::string filename = File::GetUserPath(D_SESSION_WIIROOT_IDX) + replace_fname;

	if (!File::Exists(filename))
		CreateReplacementFile(filename);

	std::ifstream f;
	OpenFStream(f, filename, std::ios_base::in);
	char letter;
	std::string replacement;

	while (f >> letter >> replacement && replacement.size())
		replacements.emplace_back(letter, replacement);
}

}
