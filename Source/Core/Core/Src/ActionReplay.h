// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#ifndef _ACTIONREPLAY_H_
#define _ACTIONREPLAY_H_

#include "IniFile.h"

namespace ActionReplay
{

struct AREntry
{
	AREntry() {}
	AREntry(u32 _addr, u32 _value) : cmd_addr(_addr), value(_value) {}
	u32 cmd_addr;
	u32 value;
};

struct ARCode
{
	std::string name;
	std::vector<AREntry> ops;
	bool active;
};

void RunAllActive();
bool RunCode(const ARCode &arcode);
void LoadCodes(IniFile &ini, bool forceLoad);
void LoadCodes(std::vector<ARCode> &_arCodes, IniFile &ini);
size_t GetCodeListSize();
ARCode GetARCode(size_t index);
void SetARCode_IsActive(bool active, size_t index);
void UpdateActiveList();
void EnableSelfLogging(bool enable);
const std::vector<std::string> &GetSelfLog();
bool IsSelfLogging();
}  // namespace

#endif // _ACTIONREPLAY_H_
