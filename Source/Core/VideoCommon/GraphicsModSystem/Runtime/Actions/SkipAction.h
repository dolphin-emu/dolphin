// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class SkipAction final : public GraphicsModAction
{
public:
  void OnDrawStarted(bool* skip) override;
  void OnEFB(bool* skip, u32 texture_width, u32 texture_height, u32* scaled_width,
             u32* scaled_height) override;
};
