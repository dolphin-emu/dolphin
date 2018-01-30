// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Extension.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/BooleanSetting.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
ControlGroup::ControlGroup(const std::string& name_, const GroupType type_)
    : name(name_), ui_name(name_), type(type_)
{
}

ControlGroup::ControlGroup(const std::string& name_, const std::string& ui_name_,
                           const GroupType type_)
    : name(name_), ui_name(ui_name_), type(type_)
{
}

ControlGroup::~ControlGroup() = default;

void ControlGroup::LoadConfig(IniFile::Section* sec, const std::string& defdev,
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
    {
      // control expression
      std::string expression;
      sec->Get(group + c->name, &expression, "");
      c->control_ref->SetExpression(std::move(expression));
    }

    // range
    sec->Get(group + c->name + "/Range", &c->control_ref->range, 100.0);
    c->control_ref->range /= 100;
  }

  // extensions
  if (type == GroupType::Extension)
  {
    Extension* const ext = (Extension*)this;

    ext->switch_extension = 0;
    u32 n = 0;
    std::string extname;
    sec->Get(base + name, &extname, "");

    for (auto& ai : ext->attachments)
    {
      ai->SetDefaultDevice(defdev);
      ai->LoadConfig(sec, base + ai->GetName() + "/");

      if (ai->GetName() == extname)
        ext->switch_extension = n;

      n++;
    }
  }
}

void ControlGroup::SaveConfig(IniFile::Section* sec, const std::string& defdev,
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
    sec->Set(group + c->name, c->control_ref->GetExpression(), "");

    // range
    sec->Set(group + c->name + "/Range", c->control_ref->range * 100.0, 100.0);
  }

  // extensions
  if (type == GroupType::Extension)
  {
    Extension* const ext = (Extension*)this;
    sec->Set(base + name, ext->attachments[ext->switch_extension]->GetName(), "None");

    for (auto& ai : ext->attachments)
      ai->SaveConfig(sec, base + ai->GetName() + "/");
  }
}

void ControlGroup::SetControlExpression(int index, const std::string& expression)
{
  controls.at(index)->control_ref->SetExpression(expression);
}
}  // namespace ControllerEmu
