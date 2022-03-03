// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Originally written by Sven Peter <sven@fail0verflow.com> for anergistic.

#pragma once

#include "Common/CommonTypes.h"
#include "Core/CoreTiming.h"

namespace GDBStub
{
enum class Signal
{
  Sigtrap = 5,
  Sigterm = 15,
};

void Init(u32 port);
void InitLocal(const char* socket);
void Deinit();
bool IsActive();
bool HasControl();
void TakeControl();
bool JustConnected();

void ProcessCommands(bool loop_until_continue);
void SendSignal(Signal signal);
}  // namespace GDBStub
