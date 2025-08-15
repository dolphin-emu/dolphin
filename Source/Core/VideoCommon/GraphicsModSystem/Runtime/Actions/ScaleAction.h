// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string_view>

#include <picojson.h>

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class ScaleAction final : public GraphicsModAction
{
public:
  static constexpr std::string_view factory_name = "scale";
  static std::unique_ptr<ScaleAction> Create(const picojson::value& json_data);
  static std::unique_ptr<ScaleAction> Create();

  explicit ScaleAction(Common::Vec3 scale);
  void OnEFB(GraphicsModActionData::EFB*) override;
  void OnProjection(GraphicsModActionData::Projection*) override;
  void OnProjectionAndTexture(GraphicsModActionData::Projection*) override;

  void DrawImGui() override;

  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

private:
  Common::Vec3 m_scale;
};
