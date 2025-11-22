// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>

#include "VideoCommon/AbstractTexture.h"

namespace VideoCommon
{
std::unique_ptr<AbstractTexture> CreateInvalidTransparentTexture();
std::unique_ptr<AbstractTexture> CreateInvalidColorTexture();
std::unique_ptr<AbstractTexture> CreateInvalidCubemapTexture();
std::unique_ptr<AbstractTexture> CreateInvalidArrayTexture();
}  // namespace VideoCommon
