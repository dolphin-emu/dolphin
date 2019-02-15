// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

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
constexpr bool operator==(const AREntry& left, const AREntry& right)
{
  return left.cmd_addr == right.cmd_addr && left.value == right.value;
}

struct ARCode
{
  std::string name;
  std::vector<AREntry> ops;
  bool active;
  bool user_defined;
};

void RunAllActive();

void ApplyCodes(const std::vector<ARCode>& codes);
void AddCode(ARCode new_code);
void LoadAndApplyCodes(const IniFile& global_ini, const IniFile& local_ini);

std::vector<ARCode> LoadCodes(const IniFile& global_ini, const IniFile& local_ini);
void SaveCodes(IniFile* local_ini, const std::vector<ARCode>& codes);

void EnableSelfLogging(bool enable);
std::vector<std::string> GetSelfLog();
void ClearSelfLog();
bool IsSelfLogging();
}  // namespace
