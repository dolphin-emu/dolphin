// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/OnionConfig.h"

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

void ApplyCodes(const std::vector<ARCode>& codes);
void AddCode(ARCode new_code);
void LoadAndApplyCodes(OnionConfig::Layer* global_config, OnionConfig::Layer* local_config);

std::vector<ARCode> LoadCodes(OnionConfig::Layer* global_config, OnionConfig::Layer* local_config);
void SaveCodes(OnionConfig::Section* ar, OnionConfig::Section* ar_enabled,
               const std::vector<ARCode>& codes);

void EnableSelfLogging(bool enable);
std::vector<std::string> GetSelfLog();
void ClearSelfLog();
bool IsSelfLogging();
}  // namespace
