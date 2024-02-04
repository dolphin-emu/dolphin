// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string_view>

#include <picojson.h>

#include "Common/Matrix.h"
#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class TransformAction final : public GraphicsModAction
{
public:
  static constexpr std::string_view factory_name = "transform";
  static std::unique_ptr<TransformAction> Create(const picojson::value& json_data);
  static std::unique_ptr<TransformAction> Create();

  TransformAction() = default;
  TransformAction(Common::Vec3 rotation, Common::Vec3 scale, Common::Vec3 translation);

  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;

  void DrawImGui() override;

  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

private:
  Common::Vec3 m_rotation = Common::Vec3{0, 0, 0};
  Common::Vec3 m_scale = Common::Vec3{1, 1, 1};
  Common::Vec3 m_translation = Common::Vec3{0, 0, 0};
  Common::Matrix44 m_calculated_transform = Common::Matrix44::Identity();
  bool m_transform_changed = false;
};
