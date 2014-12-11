// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Thread.h"
#include "Core/HW/SI.h"

namespace SI_GCAdapter
{

void Init();
void Shutdown();
void Input(SerialInterface::SSIChannel* g_Channel);
void Output(SerialInterface::SSIChannel* g_Channel);
SIDevices GetDeviceType(int channel);
void RefreshConnectedDevices();
bool IsDetected();
bool IsDriverDetected();

} // end of namespace SI_GCAdapter
