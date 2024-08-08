// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/SetSettingsAction.h"

#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"

std::unique_ptr<SetSettingsAction> SetSettingsAction::Create(const picojson::value& json_data)
{
  SetSettingsAction::Setting setting;
  bool value = false;
  const auto& setting_name = json_data.get("setting_name");
  if (setting_name.is<std::string>())
  {
    std::string setting_name_str = setting_name.to_str();
    if (setting_name_str == "skip_efb_to_ram")
    {
      setting = SetSettingsAction::Setting::Setting_Skip_EFB_To_Ram;
      value = Config::Get<bool>(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
    }
    else if (setting_name_str == "skip_xfb_to_ram")
    {
      setting = SetSettingsAction::Setting::Setting_Skip_XFB_To_Ram;
      value = Config::Get<bool>(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM);
    }
    else
    {
      return nullptr;
    }
    const auto& setting_value = json_data.get("setting_value");
    if (setting_value.is<bool>())
    {
      value = setting_value.get<bool>();
    }
  }

  return std::make_unique<SetSettingsAction>(setting, value);
}

SetSettingsAction::SetSettingsAction(Setting setting, bool value)
    : m_setting(setting), m_value(value)
{
}

void SetSettingsAction::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (!draw_started) [[unlikely]]
    return;

  if (m_setting == Setting::Setting_Skip_EFB_To_Ram)
  {
    Config::SetBaseOrCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, m_value);
  }
  else if (m_setting == Setting::Setting_Skip_XFB_To_Ram)
  {
    Config::SetBaseOrCurrent(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, m_value);
  }
}

void SetSettingsAction::OnTextureLoad(GraphicsModActionData::TextureLoad* texture_load)
{
  if (!texture_load) [[unlikely]]
    return;

  if (m_setting == Setting::Setting_Skip_EFB_To_Ram)
  {
    Config::SetBaseOrCurrent(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM, m_value);
  }
  else if (m_setting == Setting::Setting_Skip_XFB_To_Ram)
  {
    Config::SetBaseOrCurrent(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM, m_value);
  }
}

void SetSettingsAction::OnEFB(GraphicsModActionData::EFB* efb)
{
  if (!efb) [[unlikely]]
    return;

  if (!efb->force_copy_to_ram) [[unlikely]]
    return;

  if (m_setting == Setting::Setting_Skip_EFB_To_Ram)
  {
    *efb->force_copy_to_ram = !m_value;
  }
}

void SetSettingsAction::OnXFB(GraphicsModActionData::XFB* xfb)
{
  if (!xfb) [[unlikely]]
    return;

  if (!xfb->force_copy_to_ram) [[unlikely]]
    return;

  if (m_setting == Setting::Setting_Skip_XFB_To_Ram)
  {
    *xfb->force_copy_to_ram = !m_value;
  }
}
