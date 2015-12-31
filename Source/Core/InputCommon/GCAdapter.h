// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <functional>

struct GCPadStatus;

namespace GCAdapter
{

void Init();
void Reset();
void ResetRumble();
void Setup();
void Shutdown();
void SetAdapterCallback(std::function<void(void)> func);
void StartScanThread();
void StopScanThread();
void Input(int chan, GCPadStatus* pad);
void Output(int chan, u8 rumble_command);
bool IsDetected();
bool IsDriverDetected();
bool DeviceConnected(int chan);
bool UseAdapter();

} // end of namespace GCAdapter
