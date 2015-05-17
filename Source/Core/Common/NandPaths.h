// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"

static const u64 TITLEID_SYSMENU = 0x0000000100000002;
static const std::string TITLEID_SYSMENU_STRING = "0000000100000002";

namespace Common
{
	typedef std::pair<char, std::string> replace_t;
	typedef std::vector<replace_t> replace_v;

	std::string GetTicketFileName(u64 _titleID);
	std::string GetTMDFileName(u64 _titleID);
	std::string GetTitleDataPath(u64 _titleID);
	std::string GetTitleContentPath(u64 _titleID);
	bool CheckTitleTMD(u64 _titleID);
	bool CheckTitleTIK(u64 _titleID);
	void ReadReplacements(replace_v& replacements);
}
