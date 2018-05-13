// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "Common/CommonTypes.h"

class IniFile;

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
};

struct Patch
{
  std::string name;
  std::vector<PatchEntry> entries;
  bool active = false;
  bool user_defined = false;  // False if this code is shipped with Dolphin.
};

const char* PatchTypeAsString(PatchType type);

int GetSpeedhackCycles(const u32 addr);
void LoadPatchSection(const std::string& section, std::vector<Patch>& patches, IniFile& globalIni,
                      IniFile& localIni);
void LoadPatches();
bool ApplyFramePatches();
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
