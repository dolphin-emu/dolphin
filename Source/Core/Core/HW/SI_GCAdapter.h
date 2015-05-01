// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <functional>

#include "Common/Thread.h"
#include "Core/HW/SI.h"
#include "InputCommon/GCPadStatus.h"

namespace SI_GCAdapter
{

void Init();
void Reset();
void Setup();
void Shutdown();
void SetAdapterCallback(std::function<void(void)> func);
void StartScanThread();
void StopScanThread();
void Input(int chan, GCPadStatus* pad);
void Output(int chan, u8 rumble_command);
bool IsDetected();
bool IsDriverDetected();

} // end of namespace SI_GCAdapter
