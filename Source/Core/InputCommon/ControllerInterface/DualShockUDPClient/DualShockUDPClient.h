// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config/Config.h"

namespace ciface::DualShockUDPClient
{
namespace Settings
{
extern const Config::Info<bool> SERVER_ENABLED;
extern const Config::Info<std::string> SERVER_ADDRESS;
extern const Config::Info<int> SERVER_PORT;
}  // namespace Settings

void Init();
void PopulateDevices();
void DeInit();
}  // namespace ciface::DualShockUDPClient
