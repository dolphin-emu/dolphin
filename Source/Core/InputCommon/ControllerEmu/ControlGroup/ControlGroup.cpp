// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Control.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
ControlGroup::ControlGroup(std::string name_, const GroupType type_)
    : name(name_), ui_name(std::move(name_)), type(type_)
{
}

ControlGroup::ControlGroup(std::string name_, std::string ui_name_, const GroupType type_)
    : name(std::move(name_)), ui_name(std::move(ui_name_)), type(type_)
{
}

void ControlGroup::AddDeadzoneSetting(SettingValue<double>* value, double maximum_deadzone)
{
  AddSetting(value,
             {_trans("Dead Zone"),
              // i18n: The percent symbol.
              _trans("%"),
              // i18n: Refers to the dead-zone setting of gamepad inputs.
              _trans("Input strength to ignore.")},
             0, 0, maximum_deadzone);
}

ControlGroup::~ControlGroup() = default;

void ControlGroup::LoadConfig(IniFile::Section* sec, const std::string& defdev,
                              const std::string& base)
{
  const std::string group(base + name + "/");

  for (auto& setting : numeric_settings)
    setting->LoadFromIni(*sec, group);

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
  if (type == GroupType::Attachments)
  {
    auto* const ext = static_cast<Attachments*>(this);

    ext->SetSelectedAttachment(0);
    u32 n = 0;
    std::string extname;
    sec->Get(base + name, &extname, "");

    for (auto& ai : ext->GetAttachmentList())
    {
      ai->SetDefaultDevice(defdev);
      ai->LoadConfig(sec, base + ai->GetName() + "/");

      if (ai->GetName() == extname)
        ext->SetSelectedAttachment(n);

      n++;
    }
  }
}

void ControlGroup::SaveConfig(IniFile::Section* sec, const std::string& defdev,
                              const std::string& base)
{
  const std::string group(base + name + "/");

  for (auto& setting : numeric_settings)
    setting->SaveToIni(*sec, group);

  for (auto& c : controls)
  {
    // control expression
    sec->Set(group + c->name, c->control_ref->GetExpression(), "");

    // range
    sec->Set(group + c->name + "/Range", c->control_ref->range * 100.0, 100.0);
  }

  // extensions
  if (type == GroupType::Attachments)
  {
    auto* const ext = static_cast<Attachments*>(this);
    sec->Set(base + name, ext->GetAttachmentList()[ext->GetSelectedAttachment()]->GetName(),
             "None");

    for (auto& ai : ext->GetAttachmentList())
      ai->SaveConfig(sec, base + ai->GetName() + "/");
  }
}

void ControlGroup::SetControlExpression(int index, const std::string& expression)
{
  controls.at(index)->control_ref->SetExpression(expression);
}
}  // namespace ControllerEmu
