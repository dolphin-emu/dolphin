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
  TransformAction(Common::Matrix44 transform);

  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;

  void DrawImGui() override;

  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

  Common::Matrix44* GetTransform() override;

private:
  Common::Matrix44 m_transform = Common::Matrix44::Identity();
};
