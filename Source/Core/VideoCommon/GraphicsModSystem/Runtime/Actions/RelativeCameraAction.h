// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <picojson.h>

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class RelativeCameraAction final : public GraphicsModAction
{
public:
  static constexpr std::string_view factory_name = "relative_camera";
  static std::unique_ptr<RelativeCameraAction>
  Create(const picojson::value& json_data,
         std::shared_ptr<VideoCommon::CustomAssetLibrary> library);
  explicit RelativeCameraAction(GraphicsModSystem::Camera camera);
  RelativeCameraAction() = default;
  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;

  void DrawImGui() override;
  void SerializeToConfig(picojson::object* obj) override;
  std::string GetFactoryName() const override;

private:
  GraphicsModSystem::Camera m_camera;
};
