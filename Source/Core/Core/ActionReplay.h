// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "Common/CommonTypes.h"

class IniFile;

namespace ActionReplay
{
struct AREntry
{
  AREntry() = default;
  AREntry(u32 _addr, u32 _value) : cmd_addr(_addr), value(_value) {}
  u32 cmd_addr = 0;
  u32 value = 0;
};
constexpr bool operator==(const AREntry& left, const AREntry& right)
{
  return left.cmd_addr == right.cmd_addr && left.value == right.value;
}

struct ARCode
{
  std::string name;
  std::vector<AREntry> ops;
  bool enabled = false;
  bool default_enabled = false;
  bool user_defined = false;
};

void RunAllActive();

void ApplyCodes(const std::vector<ARCode>& codes);
void SetSyncedCodesAsActive();
void UpdateSyncedCodes(const std::vector<ARCode>& codes);
std::vector<ARCode> ApplyAndReturnCodes(const std::vector<ARCode>& codes);
void AddCode(ARCode new_code);
void LoadAndApplyCodes(const IniFile& global_ini, const IniFile& local_ini);

std::vector<ARCode> LoadCodes(const IniFile& global_ini, const IniFile& local_ini);
void SaveCodes(IniFile* local_ini, const std::vector<ARCode>& codes);

using EncryptedLine = std::string;
std::variant<std::monostate, AREntry, EncryptedLine> DeserializeLine(const std::string& line);
std::string SerializeLine(const AREntry& op);

void EnableSelfLogging(bool enable);
std::vector<std::string> GetSelfLog();
void ClearSelfLog();
bool IsSelfLogging();
}  // namespace ActionReplay
