// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <Windows.h>
#include <unordered_set>

namespace ciface
{
namespace DInput
{
std::unordered_set<DWORD> GetXInputGUIDS();
}
}
