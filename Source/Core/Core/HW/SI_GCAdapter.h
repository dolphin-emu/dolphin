// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "Common/Thread.h"
#include "Core/HW/SI.h"
#include "InputCommon/GCPadStatus.h"

namespace SI_GCAdapter
{

void Init();
void Shutdown();
void Input(int chan, GCPadStatus* pad);
void Output(int chan, u8 rumble_command);
SIDevices GetDeviceType(int channel);
void RefreshConnectedDevices();
bool IsDetected();
bool IsDriverDetected();

} // end of namespace SI_GCAdapter
