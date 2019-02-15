// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace UI
{
void Error(const std::string& text);

void SetVisible(bool visible);

void SetDescription(const std::string& text);

void SetTotalMarquee(bool marquee);
void ResetTotalProgress();
void SetTotalProgress(int current, int total);

void SetCurrentMarquee(bool marquee);
void ResetCurrentProgress();
void SetCurrentProgress(int current, int total);
}  // namespace UI
