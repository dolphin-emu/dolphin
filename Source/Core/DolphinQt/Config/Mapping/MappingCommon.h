// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>

class MappingWindow;

namespace MappingCommon
{
// A slight delay improves behavior when "clicking" the detect button via key-press.
static constexpr auto INPUT_DETECT_INITIAL_DELAY = std::chrono::milliseconds{100};

void CreateMappingProcessor(MappingWindow*);
}  // namespace MappingCommon
