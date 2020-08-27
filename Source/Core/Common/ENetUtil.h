// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//
#pragma once

#include <enet/enet.h>

namespace ENetUtil
{
void WakeupThread(ENetHost* host);
int ENET_CALLBACK InterceptCallback(ENetHost* host, ENetEvent* event);
}  // namespace ENetUtil
