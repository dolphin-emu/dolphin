// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <functional>

struct GCPadStatus;

namespace SI_GCAdapter
{

void Init();
void Reset();
void Setup();
void Shutdown();
void SetAdapterCallback(std::function<void(void)> func);
void Input(int chan, GCPadStatus* pad);
void Output(int chan, u8 rumble_command);
bool IsDetected();
bool IsDriverDetected();

} // end of namespace SI_GCAdapter
