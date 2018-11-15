// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <Windows.h>

namespace UI
{
void MessageLoop();
void Error(const std::string& text);
void Stop();

void SetDescription(const std::string& text);

void SetTotalMarquee(bool marquee);
void ResetTotalProgress();
void SetTotalProgress(int current, int total);

void SetCurrentMarquee(bool marquee);
void ResetCurrentProgress();
void SetCurrentProgress(int current, int total);
}  // namespace UI
