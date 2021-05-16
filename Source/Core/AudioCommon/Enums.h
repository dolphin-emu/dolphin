// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

namespace AudioCommon
{
enum class DPL2Quality
{
  Low = 0,
  Normal = 1,
  High = 2,
  Extreme = 3
};

enum class SurroundState
{
  Disabled,  // Surround is off or unsupported by backend
  EnabledNotRunning,  // Backend not running, starting or paused
  Failed,  // Backend tried to activate it but failed
  Enabled  // Backend is running and has enabled surround successfully
};
}  // namespace AudioCommon
