// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

class InputConfig;
enum class GBAPadGroup;
struct GCPadStatus;

namespace ControllerEmu
{
class ControlGroup;
}  // namespace ControllerEmu

namespace Pad
{
void ShutdownGBA();
void InitializeGBA();
void LoadGBAConfig();
bool IsGBAInitialized();

InputConfig* GetGBAConfig();

GCPadStatus GetGBAStatus(int pad_num);
void SetGBAReset(int pad_num, bool reset);

ControllerEmu::ControlGroup* GetGBAGroup(int pad_num, GBAPadGroup group);
}  // namespace Pad
