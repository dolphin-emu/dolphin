// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "InputCommon/ControllerEmu/ControlGroup/Attachments.h"

namespace ControllerEmu
{
Attachments::Attachments(const std::string& name_) : ControlGroup(name_, GroupType::Attachments)
{
}

void Attachments::AddAttachment(std::unique_ptr<EmulatedController> att)
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

const std::vector<std::unique_ptr<EmulatedController>>& Attachments::GetAttachmentList() const
{
  return m_attachments;
}

}  // namespace ControllerEmu
