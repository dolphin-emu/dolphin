// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Core/ConfigManager.h"

namespace BootManager
{
bool BootCore(const std::string& filename, SConfig::EBootBS2 type);

// Stop the emulation core and restore the configuration.
void Stop();
// Synchronise Dolphin's configuration with the SYSCONF (which may have changed during emulation),
// and restore settings that were overriden by per-game INIs or for some other reason.
void RestoreConfig();
}
