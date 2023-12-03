// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>

#include <picojson.h>

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class ModifyLightAction final : public GraphicsModAction
{
public:
  static std::unique_ptr<ModifyLightAction> Create(const picojson::value& json_data);
  static std::unique_ptr<ModifyLightAction> Create();

  ModifyLightAction() = default;
  explicit ModifyLightAction(std::optional<float4> color, std::optional<float4> cosatt,
                             std::optional<float4> distatt, std::optional<float4> pos,
                             std::optional<float4> dir);

  void DrawImGui() override;

  void OnLight(GraphicsModActionData::Light* light_data) override;

  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

private:
  std::optional<float4> m_color = {};
  std::optional<float4> m_cosatt = {};
  std::optional<float4> m_distatt = {};
  std::optional<float4> m_pos = {};
  std::optional<float4> m_dir = {};
};
