// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "Common/CommonTypes.h"
#include "Common/Config.h"

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
void LoadAndApplyCodes(Config::Layer& global_ini, Config::Layer& local_ini);

std::vector<ARCode> LoadCodes(Config::Layer& global_ini, Config::Layer& local_ini);
void SaveCodes(Config::Section* ar, Config::Section* ar_enabled, const std::vector<ARCode>& codes);

void EnableSelfLogging(bool enable);
std::vector<std::string> GetSelfLog();
void ClearSelfLog();
bool IsSelfLogging();
}  // namespace
