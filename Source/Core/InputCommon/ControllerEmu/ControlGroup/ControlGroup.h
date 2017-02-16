// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Core/ConfigManager.h"
#include "InputCommon/ControllerInterface/Device.h"

namespace ControllerEmu
{
class Control;

enum
{
  GROUP_TYPE_OTHER,
  GROUP_TYPE_STICK,
  GROUP_TYPE_MIXED_TRIGGERS,
  GROUP_TYPE_BUTTONS,
  GROUP_TYPE_FORCE,
  GROUP_TYPE_EXTENSION,
  GROUP_TYPE_TILT,
  GROUP_TYPE_CURSOR,
  GROUP_TYPE_TRIGGERS,
  GROUP_TYPE_SLIDER
};

class ControlGroup
{
public:
  enum class SettingType
  {
    NORMAL,   // normal settings are saved to configuration files
    VIRTUAL,  // virtual settings are not saved at all
  };

  class NumericSetting
  {
  public:
    NumericSetting(const std::string& setting_name, const ControlState default_value,
                   const u32 low = 0, const u32 high = 100,
                   const SettingType setting_type = SettingType::NORMAL)
        : m_type(setting_type), m_name(setting_name), m_default_value(default_value), m_low(low),
          m_high(high)
    {
    }

    ControlState GetValue() const { return m_value; }
    void SetValue(ControlState value) { m_value = value; }
    const SettingType m_type;
    const std::string m_name;
    const ControlState m_default_value;
    const u32 m_low;
    const u32 m_high;
    ControlState m_value;
  };

  class BooleanSetting
  {
  public:
    BooleanSetting(const std::string& setting_name, const bool default_value,
                   const SettingType setting_type = SettingType::NORMAL)
        : m_type(setting_type), m_name(setting_name), m_default_value(default_value)
    {
    }
    virtual ~BooleanSetting();

    virtual bool GetValue() const { return m_value; }
    virtual void SetValue(bool value) { m_value = value; }
    const SettingType m_type;
    const std::string m_name;
    const bool m_default_value;
    bool m_value;
  };

  class BackgroundInputSetting : public BooleanSetting
  {
  public:
    BackgroundInputSetting(const std::string& setting_name)
        : BooleanSetting(setting_name, false, SettingType::VIRTUAL)
    {
    }

    bool GetValue() const override { return SConfig::GetInstance().m_BackgroundInput; }
    void SetValue(bool value) override
    {
      m_value = value;
      SConfig::GetInstance().m_BackgroundInput = value;
    }
  };

  explicit ControlGroup(const std::string& name, u32 type = GROUP_TYPE_OTHER);
  ControlGroup(const std::string& name, const std::string& ui_name, u32 type = GROUP_TYPE_OTHER);
  virtual ~ControlGroup();

  virtual void LoadConfig(IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");
  virtual void SaveConfig(IniFile::Section* sec, const std::string& defdev = "",
                          const std::string& base = "");

  void SetControlExpression(int index, const std::string& expression);

  const std::string name;
  const std::string ui_name;
  const u32 type;

  std::vector<std::unique_ptr<Control>> controls;
  std::vector<std::unique_ptr<NumericSetting>> numeric_settings;
  std::vector<std::unique_ptr<BooleanSetting>> boolean_settings;
};
}  // namespace ControllerEmu
