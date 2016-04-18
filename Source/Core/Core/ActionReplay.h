// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string>
#include <vector>
#include "Common/CommonTypes.h"

class IniFile;

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
	bool user_defined;
};

void RunAllActive();
bool RunCode(const ARCode &arcode);
void ApplyCodes(const std::vector<ARCode>& codes);
void AddCode(const ARCode& new_code);
void* RegisterCodeChangeCallback(std::function<void()> callback);
void  UnregisterCodeChangeCallback(void* token);
void LoadAndApplyCodes(const IniFile& globalini, const IniFile& localIni);
std::vector<ARCode> LoadCodes(const IniFile& globalini, const IniFile& localIni);
size_t GetCodeListSize();
ARCode GetARCode(size_t index);
void SetARCode_IsActive(bool active, size_t index);
void UpdateActiveList();
void EnableSelfLogging(bool enable);
const std::vector<std::string> &GetSelfLog();
bool IsSelfLogging();
}  // namespace
