// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

class IniFile;

namespace ScriptEngine
{
struct Script
{
  std::string file_path;
  bool active = false;
};

void LoadScripts();
void Shutdown();
void Reload();

void ExecuteScript(u32 address, u32 script_id);

}  // namespace ScriptEngine
