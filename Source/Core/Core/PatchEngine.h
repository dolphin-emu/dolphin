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
enum PatchType
{
  PATCH_8BIT,
  PATCH_16BIT,
  PATCH_32BIT,
};

extern const char* PatchTypeStrings[];

struct PatchEntry
{
  PatchEntry() = default;
  PatchEntry(PatchType t, u32 addr, u32 value_) : type(t), address(addr), value(value_) {}
  PatchType type = PatchType::PATCH_8BIT;
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
  case PatchEngine::PATCH_8BIT:
    size = 2;
    break;

  case PatchEngine::PATCH_16BIT:
    size = 4;
    break;

  case PatchEngine::PATCH_32BIT:
    size = 8;
    break;
  }
  return size;
}

}  // namespace PatchEngine
