// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <Windows.h>

namespace UI
{
void MessageLoop();
void Stop();

void SetDescription(const std::string& text);
void SetMarquee(bool marquee);
void ResetProgress();
void SetProgress(int current, int total);
void IncrementProgress(int amount);
}  // namespace UI
