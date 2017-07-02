// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>

#include "Core/ConfigManager.h"

struct BootParameters;

namespace BootManager
{
bool BootCore(std::unique_ptr<BootParameters> parameters);

// Stop the emulation core and restore the configuration.
void Stop();
// Synchronise Dolphin's configuration with the SYSCONF (which may have changed during emulation),
// and restore settings that were overriden by per-game INIs or for some other reason.
void RestoreConfig();
}
