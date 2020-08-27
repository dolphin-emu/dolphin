// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "Common/CommonTypes.h"

namespace UI
{
void Error(const std::string& text);

void SetDescription(const std::string& text);

void SetTotalMarquee(bool marquee);
void ResetTotalProgress();
void SetTotalProgress(int current, int total);

void SetCurrentMarquee(bool marquee);
void ResetCurrentProgress();
void SetCurrentProgress(int current, int total);

void SetVisible(bool visible);

void Stop();

void Init();
void Sleep(int seconds);
void WaitForPID(u32 pid);
void LaunchApplication(std::string path);
}  // namespace UI
