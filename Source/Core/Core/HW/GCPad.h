// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerInterface/CoreDevice.h"

class InputConfig;
enum class PadGroup;
struct GCPadStatus;

namespace ControllerEmu
{
class ControlGroup;
}

namespace Pad
{
void Shutdown();
void Initialize();
void LoadConfig();
bool IsInitialized();

InputConfig* GetConfig();

GCPadStatus GetStatus(int pad_num);
ControllerEmu::ControlGroup* GetGroup(int pad_num, PadGroup group);
void Rumble(int pad_num, ControlState strength);
void ResetRumble(int pad_num);

bool GetMicButton(int pad_num);
}  // namespace Pad
