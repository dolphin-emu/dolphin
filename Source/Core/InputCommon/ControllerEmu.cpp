// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu.h"
#include <memory>
#include "Common/Common.h"
#include "VideoCommon/OnScreenDisplay.h"

// This should be called before calling GetState() or State() on a control reference
// to prevent a race condition.
// This is a recursive mutex because UpdateReferences is recursive.
static std::recursive_mutex s_get_state_mutex;
std::unique_lock<std::recursive_mutex> ControllerEmu::GetStateLock()
{
  std::unique_lock<std::recursive_mutex> lock(s_get_state_mutex);
  return lock;
}

void ControllerEmu::UpdateReferences(ControllerInterface& devi)
{
  auto lock = ControllerEmu::GetStateLock();
  for (auto& ctrlGroup : groups)
  {
    for (auto& control : ctrlGroup->controls)
      devi.UpdateReference(control->control_ref.get(), default_device);

    // extension
    if (ctrlGroup->type == GROUP_TYPE_EXTENSION)
    {
      for (auto& attachment : ((Extension*)ctrlGroup.get())->attachments)
        attachment->UpdateReferences(devi);
    }
  }
}

void ControllerEmu::UpdateDefaultDevice()
{
  for (auto& ctrlGroup : groups)
  {
    // extension
    if (ctrlGroup->type == GROUP_TYPE_EXTENSION)
    {
      for (auto& ai : ((Extension*)ctrlGroup.get())->attachments)
      {
        ai->default_device = default_device;
        ai->UpdateDefaultDevice();
      }
    }
  }
}

void ControllerEmu::ControlGroup::LoadConfig(IniFile::Section* sec, const std::string& defdev,
                                             const std::string& base)
{
  std::string group(base + name + "/");

  // settings
  for (auto& s : numeric_settings)
  {
    if (s->m_type == SettingType::VIRTUAL)
      continue;
    sec->Get(group + s->m_name, &s->m_value, s->m_default_value * 100);
    s->m_value /= 100;
  }
  for (auto& s : boolean_settings)
  {
    if (s->m_type == SettingType::VIRTUAL)
      continue;
    sec->Get(group + s->m_name, &s->m_value, s->m_default_value);
  }

  for (auto& c : controls)
  {
    // control expression
    sec->Get(group + c->name, &c->control_ref->expression, "");

    // range
    sec->Get(group + c->name + "/Range", &c->control_ref->range, 100.0);
    c->control_ref->range /= 100;
  }

  // extensions
  if (type == GROUP_TYPE_EXTENSION)
  {
    Extension* const ext = (Extension*)this;

    ext->switch_extension = 0;
    unsigned int n = 0;
    std::string extname;
    sec->Get(base + name, &extname, "");

    for (auto& ai : ext->attachments)
    {
      ai->default_device.FromString(defdev);
      ai->LoadConfig(sec, base + ai->GetName() + "/");

      if (ai->GetName() == extname)
        ext->switch_extension = n;

      n++;
    }
  }
}

void ControllerEmu::LoadConfig(IniFile::Section* sec, const std::string& base)
{
  std::string defdev = default_device.ToString();
  if (base.empty())
  {
    sec->Get(base + "Device", &defdev, "");
    default_device.FromString(defdev);
  }

  for (auto& cg : groups)
    cg->LoadConfig(sec, defdev, base);
}

void ControllerEmu::ControlGroup::SaveConfig(IniFile::Section* sec, const std::string& defdev,
                                             const std::string& base)
{
  std::string group(base + name + "/");

  for (auto& s : numeric_settings)
  {
    if (s->m_type == SettingType::VIRTUAL)
      continue;
    sec->Set(group + s->m_name, s->m_value * 100.0, s->m_default_value * 100.0);
  }
  for (auto& s : boolean_settings)
  {
    if (s->m_type == SettingType::VIRTUAL)
      continue;
    sec->Set(group + s->m_name, s->m_value, s->m_default_value);
  }

  for (auto& c : controls)
  {
    // control expression
    sec->Set(group + c->name, c->control_ref->expression, "");

    // range
    sec->Set(group + c->name + "/Range", c->control_ref->range * 100.0, 100.0);
  }

  // extensions
  if (type == GROUP_TYPE_EXTENSION)
  {
    Extension* const ext = (Extension*)this;
    sec->Set(base + name, ext->attachments[ext->switch_extension]->GetName(), "None");

    for (auto& ai : ext->attachments)
      ai->SaveConfig(sec, base + ai->GetName() + "/");
  }
}

void ControllerEmu::SaveConfig(IniFile::Section* sec, const std::string& base)
{
  const std::string defdev = default_device.ToString();
  if (base.empty())
    sec->Set(/*std::string(" ") +*/ base + "Device", defdev, "");

  for (auto& ctrlGroup : groups)
    ctrlGroup->SaveConfig(sec, defdev, base);
}

void ControllerEmu::ControlGroup::SetControlExpression(int index, const std::string& expression)
{
  controls.at(index)->control_ref->expression = expression;
}

ControllerEmu::AnalogStick::AnalogStick(const char* const _name, ControlState default_radius)
    : AnalogStick(_name, _name, GROUP_TYPE_STICK)
{
}

ControllerEmu::AnalogStick::AnalogStick(const char* const _name, const char* const _ui_name,
                                        ControlState default_radius)
    : ControlGroup(_name, _ui_name, GROUP_TYPE_STICK)
{
  for (auto& named_direction : named_directions)
    controls.emplace_back(std::make_unique<Input>(named_direction));

  controls.emplace_back(std::make_unique<Input>(_trans("Modifier")));
  numeric_settings.emplace_back(
      std::make_unique<NumericSetting>(_trans("Radius"), default_radius, 0, 100));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

ControllerEmu::Buttons::Buttons(const std::string& _name) : ControlGroup(_name, GROUP_TYPE_BUTTONS)
{
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Threshold"), 0.5));
}

ControllerEmu::ModifySettingsButton::ModifySettingsButton(std::string button_name)
    : Buttons(std::move(button_name))
{
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Threshold"), 0.5));
}

void ControllerEmu::ModifySettingsButton::AddInput(std::string button_name, bool toggle)
{
  controls.emplace_back(new ControlGroup::Input(std::move(button_name)));
  threshold_exceeded.emplace_back(false);
  associated_settings.emplace_back(false);
  associated_settings_toggle.emplace_back(toggle);
}

void ControllerEmu::ModifySettingsButton::GetState()
{
  for (size_t i = 0; i < controls.size(); ++i)
  {
    ControlState state = controls[i]->control_ref->State();

    if (!associated_settings_toggle[i])
    {
      // not toggled
      associated_settings[i] = state > numeric_settings[0]->GetValue();
    }
    else
    {
      // toggle (loading savestates does not en-/disable toggle)
      // after we passed the threshold, we en-/disable. but after that, we don't change it
      // anymore
      if (!threshold_exceeded[i] && state > numeric_settings[0]->GetValue())
      {
        associated_settings[i] = !associated_settings[i];
        if (associated_settings[i])
          OSD::AddMessage(controls[i]->name + ": " + _trans("on"));
        else
          OSD::AddMessage(controls[i]->name + ": " + _trans("off"));
        threshold_exceeded[i] = true;
      }
      if (state < numeric_settings[0]->GetValue())
        threshold_exceeded[i] = false;
    }
  }
}

ControllerEmu::MixedTriggers::MixedTriggers(const std::string& _name)
    : ControlGroup(_name, GROUP_TYPE_MIXED_TRIGGERS)
{
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Threshold"), 0.9));
}

ControllerEmu::Triggers::Triggers(const std::string& _name)
    : ControlGroup(_name, GROUP_TYPE_TRIGGERS)
{
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

ControllerEmu::Slider::Slider(const std::string& _name) : ControlGroup(_name, GROUP_TYPE_SLIDER)
{
  controls.emplace_back(std::make_unique<Input>("Left"));
  controls.emplace_back(std::make_unique<Input>("Right"));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

ControllerEmu::Force::Force(const std::string& _name) : ControlGroup(_name, GROUP_TYPE_FORCE)
{
  memset(m_swing, 0, sizeof(m_swing));

  controls.emplace_back(std::make_unique<Input>(_trans("Up")));
  controls.emplace_back(std::make_unique<Input>(_trans("Down")));
  controls.emplace_back(std::make_unique<Input>(_trans("Left")));
  controls.emplace_back(std::make_unique<Input>(_trans("Right")));
  controls.emplace_back(std::make_unique<Input>(_trans("Forward")));
  controls.emplace_back(std::make_unique<Input>(_trans("Backward")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
}

ControllerEmu::Tilt::Tilt(const std::string& _name) : ControlGroup(_name, GROUP_TYPE_TILT)
{
  memset(m_tilt, 0, sizeof(m_tilt));

  controls.emplace_back(std::make_unique<Input>("Forward"));
  controls.emplace_back(std::make_unique<Input>("Backward"));
  controls.emplace_back(std::make_unique<Input>("Left"));
  controls.emplace_back(std::make_unique<Input>("Right"));

  controls.emplace_back(std::make_unique<Input>(_trans("Modifier")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Dead Zone"), 0, 0, 50));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Circle Stick"), 0));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Angle"), 0.9, 0, 180));
}

ControllerEmu::Cursor::Cursor(const std::string& _name)
    : ControlGroup(_name, GROUP_TYPE_CURSOR), m_z(0)
{
  for (auto& named_direction : named_directions)
    controls.emplace_back(std::make_unique<Input>(named_direction));
  controls.emplace_back(std::make_unique<Input>("Forward"));
  controls.emplace_back(std::make_unique<Input>("Backward"));
  controls.emplace_back(std::make_unique<Input>(_trans("Hide")));

  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Center"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Width"), 0.5));
  numeric_settings.emplace_back(std::make_unique<NumericSetting>(_trans("Height"), 0.5));
}

void ControllerEmu::LoadDefaults(const ControllerInterface& ciface)
{
  // load an empty inifile section, clears everything
  IniFile::Section sec;
  LoadConfig(&sec);

  const std::string& default_device_string = g_controller_interface.GetDefaultDeviceString();
  if (!default_device_string.empty())
  {
    default_device.FromString(default_device_string);
    UpdateDefaultDevice();
  }
}
