// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include "Common/CommonTypes.h"
#include "Core/ScriptEngine.h"

// Script hooks
class ScriptHooks
{
public:
  [[nodiscard]] bool HasBreakPoint(u32 address) const;
  void ExecuteBreakPoint(u32 address);

public:
  std::multimap<u32, ScriptEngine::LuaFuncHandle> m_hooks;
};
