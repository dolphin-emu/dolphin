// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Config/Config.h"

namespace ciface::DualShockUDPClient
{
constexpr char DEFAULT_SERVER_ADDRESS[] = "127.0.0.1";
constexpr u16 DEFAULT_SERVER_PORT = 26760;

namespace Settings
{
// These two kept for backwards compatibility
extern const Config::Info<std::string> SERVER_ADDRESS;
extern const Config::Info<int> SERVER_PORT;

extern const Config::Info<std::string> SERVERS;
extern const Config::Info<bool> SERVERS_ENABLED;
}  // namespace Settings

void Init();
void PopulateDevices();
void DeInit();
}  // namespace ciface::DualShockUDPClient
