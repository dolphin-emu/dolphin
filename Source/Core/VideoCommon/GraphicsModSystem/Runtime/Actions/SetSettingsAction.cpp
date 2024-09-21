// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/Actions/SetSettingsAction.h"

#include <picojson.h>

#include "Common/Config/Config.h"
#include "Core/Config/GraphicsSettings.h"

namespace
{
template <typename T>
class SetSettingsActionImpl final : public SetSettingsAction
{
public:
  SetSettingsActionImpl(const Config::Info<T>& setting, const picojson::value& json_data);

  void OnDrawStarted(GraphicsModActionData::DrawStarted*) override;
  void OnTextureLoad(GraphicsModActionData::TextureLoad*) override;
  void OnEFB(GraphicsModActionData::EFB*) override;
  void OnXFB(GraphicsModActionData::XFB*) override;

private:
  const Config::Info<T>& m_setting;
  T m_value;
};

template <typename T>
SetSettingsActionImpl<T>::SetSettingsActionImpl(const Config::Info<T>& setting,
                                                const picojson::value& json_data)
    : m_setting(setting)
{
  const auto& setting_value = json_data.get("setting_value");
  if (setting_value.is<T>())
    m_value = setting_value.get<T>();
  else
    m_value = Config::Get<T>(setting);
}

template <typename T>
void SetSettingsActionImpl<T>::OnDrawStarted(GraphicsModActionData::DrawStarted* draw_started)
{
  if (draw_started == nullptr) [[unlikely]]
    return;

  Config::SetBaseOrCurrent(m_setting, m_value);
}

template <typename T>
void SetSettingsActionImpl<T>::OnTextureLoad(GraphicsModActionData::TextureLoad* texture_load)
{
  if (texture_load == nullptr) [[unlikely]]
    return;

  Config::SetBaseOrCurrent(m_setting, m_value);
}

template <typename T>
void SetSettingsActionImpl<T>::OnEFB(GraphicsModActionData::EFB* efb)
{
  if (efb == nullptr) [[unlikely]]
    return;

  if (&m_setting == &Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM)
    *efb->force_copy_to_ram = !m_value;
}

template <typename T>
void SetSettingsActionImpl<T>::OnXFB(GraphicsModActionData::XFB* xfb)
{
  if (xfb == nullptr) [[unlikely]]
    return;

  if (&m_setting == &Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM)
    *xfb->force_copy_to_ram = !m_value;
}
}  // namespace

std::unique_ptr<SetSettingsAction> SetSettingsAction::Create(const picojson::value& json_data)
{
  const auto& setting_name = json_data.get("setting_name");
  if (!setting_name.is<std::string>())
    return nullptr;

  const auto make_unique_ptr = [&json_data](const auto& setting) {
    using setting_type = decltype(setting.GetCachedValue().value);
    return std::make_unique<SetSettingsActionImpl<setting_type>>(setting, json_data);
  };

  const std::string setting_name_str = setting_name.to_str();
  if (setting_name_str == "skip_efb_to_ram")
    return make_unique_ptr(Config::GFX_HACK_SKIP_EFB_COPY_TO_RAM);
  else if (setting_name_str == "skip_xfb_to_ram")
    return make_unique_ptr(Config::GFX_HACK_SKIP_XFB_COPY_TO_RAM);

  return nullptr;
}
