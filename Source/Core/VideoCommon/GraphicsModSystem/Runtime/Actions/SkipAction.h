// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class SkipAction final : public GraphicsModAction
{
public:
  static constexpr std::string_view factory_name = "skip";
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;
  void OnEFB(GraphicsModActionData::EFB*) override;
  std::string GetFactoryName() const override;
};
