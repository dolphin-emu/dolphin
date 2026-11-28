// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#if defined(DYNAMICALLY_LOAD_FFMPEG)

#include <functional>
#include <string>

namespace Common
{

std::string GetSharedFFmpegPath();

bool LoadSharedFFmpeg();

// DolphinQt on Windows uses this to download FFmpeg DLLs from the internet after asking the user.
// Callback should return true to attempt loading again.
void SetSharedFFmpegLoadFailureCallback(std::function<bool()> callback);

}  // namespace Common

#endif
