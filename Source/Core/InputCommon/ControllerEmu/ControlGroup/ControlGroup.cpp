// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Control/Output.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

namespace ControllerEmu
{
ControlGroup::ControlGroup(std::string name_, const GroupType type_, DefaultValue default_value_)
    : name(name_), ui_name(std::move(name_)), type(type_), default_value(default_value_)
{
}

ControlGroup::ControlGroup(std::string name_, std::string ui_name_, const GroupType type_,
                           DefaultValue default_value_)
    : name(std::move(name_)), ui_name(std::move(ui_name_)), type(type_),
      default_value(default_value_)
{
}

void ControlGroup::AddVirtualNotchSetting(SettingValue<double>* value, double max_virtual_notch_deg)
{
  AddSetting(value,
             {_trans("Virtual Notches"),
              // i18n: The degrees symbol.
              _trans("°"), _trans("Snap the thumbstick position to the nearest octagonal axis."),
              nullptr, SettingVisibility::Advanced},
             0, 0, max_virtual_notch_deg);
}

void ControlGroup::AddDeadzoneSetting(SettingValue<double>* value, double maximum_deadzone)
{
  AddSetting(value,
             {_trans("Dead Zone"),
              // i18n: The percent symbol.
              _trans("%"),
              // i18n: Refers to the dead-zone setting of gamepad inputs.
              _trans("Input strength to ignore and remap.")},
             0, 0, maximum_deadzone);
}

ControlGroup::~ControlGroup() = default;

void ControlGroup::LoadConfig(Common::IniFile::Section* sec, const std::string& defdev,
                              const std::string& base)
{
  const std::string group(base + name + "/");

  // enabled
  if (default_value != DefaultValue::AlwaysEnabled)
    sec->Get(group + "Enabled", &enabled, default_value != DefaultValue::Disabled);

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
    std::string attachment_text;
    sec->Get(base + name, &attachment_text, "");

    // First assume attachment string is a valid expression.
    // If it instead matches one of the names of our attachments it is overridden below.
    ext->GetSelectionSetting().GetInputReference().SetExpression(attachment_text);

    for (auto& ai : ext->GetAttachmentList())
    {
      ai->SetDefaultDevice(defdev);
      ai->LoadConfig(sec, base + ai->GetName() + "/");

      if (ai->GetName() == attachment_text)
        ext->SetSelectedAttachment(n);

      n++;
    }
  }
}

void ControlGroup::SaveConfig(Common::IniFile::Section* sec, const std::string& defdev,
                              const std::string& base)
{
  const std::string group(base + name + "/");

  // enabled
  sec->Set(group + "Enabled", enabled, default_value != DefaultValue::Disabled);

  for (auto& setting : numeric_settings)
    setting->SaveToIni(*sec, group);

  for (auto& c : controls)
  {
    // control expression
    std::string expression = c->control_ref->GetExpression();
    // We can't save line breaks in a single line config. Restoring them is too complicated.
    ReplaceBreaksWithSpaces(expression);
    sec->Set(group + c->name, expression, "");

    // range
    sec->Set(group + c->name + "/Range", c->control_ref->range * 100.0, 100.0);
  }

  // extensions
  if (type == GroupType::Attachments)
  {
    auto* const ext = static_cast<Attachments*>(this);

    if (ext->GetSelectionSetting().IsSimpleValue())
    {
      sec->Set(base + name, ext->GetAttachmentList()[ext->GetSelectedAttachment()]->GetName(),
               "None");
    }
    else
    {
      std::string expression = ext->GetSelectionSetting().GetInputReference().GetExpression();
      ReplaceBreaksWithSpaces(expression);
      sec->Set(base + name, expression, "None");
    }

    for (auto& ai : ext->GetAttachmentList())
      ai->SaveConfig(sec, base + ai->GetName() + "/");
  }
}

void ControlGroup::SetControlExpression(int index, const std::string& expression)
{
  controls.at(index)->control_ref->SetExpression(expression);
}

void ControlGroup::AddInput(Translatability translate, std::string name_)
{
  controls.emplace_back(std::make_unique<Input>(translate, std::move(name_)));
}

void ControlGroup::AddInput(Translatability translate, std::string name_, std::string ui_name_)
{
  controls.emplace_back(std::make_unique<Input>(translate, std::move(name_), std::move(ui_name_)));
}

void ControlGroup::AddOutput(Translatability translate, std::string name_)
{
  controls.emplace_back(std::make_unique<Output>(translate, std::move(name_)));
}

}  // namespace ControllerEmu
