// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"

namespace Common
{
class IniFile;
}
namespace Core
{
class System;
}

namespace PatchEngine
{
enum class PatchType
{
  Patch8Bit,
  Patch16Bit,
  Patch32Bit,
};

struct PatchEntry
{
  PatchEntry() = default;
  PatchEntry(PatchType t, u32 addr, u32 value_) : type(t), address(addr), value(value_) {}
  PatchType type = PatchType::Patch8Bit;
  u32 address = 0;
  u32 value = 0;
  u32 comparand = 0;
  bool conditional = false;
};

struct Patch
{
  std::string name;
  std::vector<PatchEntry> entries;
  bool enabled = false;
  bool default_enabled = false;
  bool user_defined = false;  // False if this code is shipped with Dolphin.
};

const char* PatchTypeAsString(PatchType type);

int GetSpeedhackCycles(const u32 addr);

std::optional<PatchEntry> DeserializeLine(std::string line);
std::string SerializeLine(const PatchEntry& entry);
void LoadPatchSection(const std::string& section, std::vector<Patch>* patches,
                      const Common::IniFile& globalIni, const Common::IniFile& localIni);
void SavePatchSection(Common::IniFile* local_ini, const std::vector<Patch>& patches);
void LoadPatches();

void AddMemoryPatch(std::size_t index);
void RemoveMemoryPatch(std::size_t index);

bool ApplyFramePatches(Core::System& system);
void Shutdown();
void Reload();

inline int GetPatchTypeCharLength(PatchType type)
{
  int size = 8;
  switch (type)
  {
  case PatchType::Patch8Bit:
    size = 2;
    break;

  case PatchType::Patch16Bit:
    size = 4;
    break;

  case PatchType::Patch32Bit:
    size = 8;
    break;
  }
  return size;
}

}  // namespace PatchEngine
