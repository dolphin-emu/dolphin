// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>

#include <picojson.h>

#include "VideoCommon/GraphicsModSystem/Runtime/GraphicsModAction.h"

class SetSettingsAction final : public GraphicsModAction
{
public:
  enum class Setting
  {
    Setting_Invalid,
    Setting_Skip_EFB_To_Ram,
    Setting_Skip_XFB_To_Ram
  };

  static std::unique_ptr<SetSettingsAction> Create(const picojson::value& json_data);
  SetSettingsAction(Setting setting, bool value);

  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;
  void OnTextureLoad(GraphicsModActionData::TextureLoad*) override;
  void OnEFB(GraphicsModActionData::EFB*) override;
  void OnXFB(GraphicsModActionData::XFB*) override;

private:
  Setting m_setting;
  bool m_value;
};
