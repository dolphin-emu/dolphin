// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string_view>

#include "Common/CommonTypes.h"

#include "VideoCommon/RenderBase.h"

namespace SW
{
class SWRenderer final : public Renderer
{
public:
  u32 AccessEFB(EFBAccessType type, u32 x, u32 y, u32 poke_data) override;
  void PokeEFB(EFBAccessType type, const EfbPokeData* points, size_t num_points) override {}

  void ReinterpretPixelData(EFBReinterpretType convtype) override {}
};
}  // namespace SW
