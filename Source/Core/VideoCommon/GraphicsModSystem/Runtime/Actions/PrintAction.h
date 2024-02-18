// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class PrintAction final : public GraphicsModAction
{
public:
  static constexpr std::string_view factory_name = "print";
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;
  void OnEFB(GraphicsModActionData::EFB*) override;
  void OnProjection(GraphicsModActionData::Projection*) override;
  void OnProjectionAndTexture(GraphicsModActionData::Projection*) override;
  void OnTextureLoad(GraphicsModActionData::TextureLoad*) override;
};
