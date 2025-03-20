// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"

namespace ControllerEmu
{

void AttachedController::LoadDefaults()
{
}

Attachments::Attachments(const std::string& name_) : ControlGroup(name_, GroupType::Attachments)
{
}

void Attachments::AddAttachment(std::unique_ptr<AttachedController> att)
{
  m_attachments.emplace_back(std::move(att));
}

u32 Attachments::GetSelectedAttachment() const
{
  // This is originally an int, treat it as such
  const int value = m_selection_value.GetValue();

  if (value > 0 && static_cast<size_t>(value) < m_attachments.size())
    return u32(value);

  return 0;
}

void Attachments::SetSelectedAttachment(u32 val)
{
  m_selection_setting.SetValue(val);
}

NumericSetting<int>& Attachments::GetSelectionSetting()
{
  return m_selection_setting;
}

SubscribableSettingValue<int>& Attachments::GetAttachmentSetting()
{
  return m_selection_value;
}

const std::vector<std::unique_ptr<AttachedController>>& Attachments::GetAttachmentList() const
{
  return m_attachments;
}

void Attachments::LoadConfig(Common::IniFile::Section* sec, const std::string& base)
{
  ControlGroup::LoadConfig(sec, base);

  SetSelectedAttachment(0);

  std::string attachment_text;
  sec->Get(base + name, &attachment_text, "");

  // First assume attachment string is a valid expression.
  // If it instead matches one of the names of our attachments it is overridden below.
  GetSelectionSetting().GetInputReference().SetExpression(attachment_text);

  u32 n = 0;
  for (auto& ai : GetAttachmentList())
  {
    ai->LoadGroupsConfig(sec, base + ai->GetName() + "/");

    if (ai->GetName() == attachment_text)
      SetSelectedAttachment(n);

    ++n;
  }
}

void Attachments::SaveConfig(Common::IniFile::Section* sec, const std::string& base)
{
  if (GetSelectionSetting().IsSimpleValue())
  {
    sec->Set(base + name, GetAttachmentList()[GetSelectedAttachment()]->GetName(), "None");
  }
  else
  {
    std::string expression = GetSelectionSetting().GetInputReference().GetExpression();
    ReplaceBreaksWithSpaces(expression);
    sec->Set(base + name, expression, "None");
  }

  for (auto& ai : GetAttachmentList())
    ai->SaveGroupsConfig(sec, base + ai->GetName() + "/");
}

void Attachments::UpdateReferences(ciface::ExpressionParser::ControlEnvironment& env)
{
  ControlGroup::UpdateReferences(env);

  GetSelectionSetting().GetInputReference().UpdateReference(env);

  for (auto& attachment : GetAttachmentList())
    attachment->UpdateGroupsReferences(env);
}

}  // namespace ControllerEmu
