// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace BootManager
{
bool BootCore(const std::string& _rFilename);

void Stop();
}
