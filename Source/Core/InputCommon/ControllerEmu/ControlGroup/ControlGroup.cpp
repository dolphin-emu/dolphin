// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"

#include "Common/CommonTypes.h"
#include "Common/IniFile.h"
#include "Common/FileSearch.h"
#include "Common/FileUtil.h"
#include "Common/IOFile.h"

#include "Core/HW/Wiimote.h"

#include "InputCommon/ControlReference/ControlReference.h"
#include "InputCommon/ControllerEmu/Control/Input.h"
#include "InputCommon/ControllerEmu/Control/Output.h"
#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackModes.h"
#include "InputCommon/ControllerEmu/ControlGroup/PrimeHackAltProfile.h"

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

void ControlGroup::LoadConfig(IniFile::Section* sec, const std::string& defdev,
                              const std::string& base)
{
  const std::string group(base + name + "/");

  // enabled
  if (default_value != DefaultValue::AlwaysEnabled)
    sec->Get(group + "Enabled", &enabled, default_value == DefaultValue::Enabled);

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

  if (type == GroupType::Buttons) {
    // Load D-Pad Down into Missiles control
    if (use_metroid_ui) {
      if (controls.size() == 7) { // Hacky way to tell if Wii or GCN
        std::string expression;
        sec->Get("D-Pad/Down", &expression, "");
        controls[6]->control_ref->SetExpression(std::move(expression));
      }
    }
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

  // extensions
  if (type == GroupType::PrimeHackMode)
  {
    auto* const ext = static_cast<PrimeHackModes*>(this);

    std::string i;
    sec->Get(base + name + "/Mode", &i, "0");
    ext->SetSelectedDevice(stoi(i));
  }

  if (type == GroupType::PrimeHackAltProfile)
  {
    auto* const ext = static_cast<PrimeHackAltProfile*>(this);

    std::string prof;
    sec->Get(base + name + "/AltProfile", &prof);
    ext->SetAltProfileName(prof);
  }
}

void ControlGroup::SaveConfig(IniFile::Section* sec, const std::string& defdev,
                              const std::string& base)
{
  const std::string group(base + name + "/");

  // enabled
  sec->Set(group + "Enabled", enabled, true);

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

  if (type == GroupType::Buttons) {
    // Load D-Pad Down into Missiles control
    if (use_metroid_ui) {
      if (controls.size() == 7) { // Hacky way to tell if Wii or GCN
        sec->Set("D-Pad/Down", controls[6]->control_ref->GetExpression(), "");
      }
    }
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

  if (type == GroupType::PrimeHackMode)
  {
    auto* const ext = static_cast<PrimeHackModes*>(this);

    sec->Set(base + name + "/Mode", std::to_string(ext->GetSelectedDevice()), "0");
  }

  if (type == GroupType::PrimeHackAltProfile)
  {
    auto* const ext = static_cast<PrimeHackAltProfile*>(this);

    std::string morph_prof = ext->GetAltProfileName();
    if (morph_prof.empty())
      morph_prof = "Disabled";
    sec->Set(base + name + "/AltProfile", morph_prof);


    // Go ahead and save out the backup of this profile for when in-game.
    const std::string og_wiimote_new = WIIMOTE_INI_NAME;
    const std::string backup_wiimote_new = std::string(WIIMOTE_INI_NAME) + "_Backup.ini";

    const std::string default_path = File::GetUserPath(D_CONFIG_IDX) + og_wiimote_new + ".ini";
    std::string file_text;
    if (File::ReadFileToString(default_path, file_text))
      File::WriteStringToFile(File::GetUserPath(D_CONFIG_IDX) + backup_wiimote_new, file_text);
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
