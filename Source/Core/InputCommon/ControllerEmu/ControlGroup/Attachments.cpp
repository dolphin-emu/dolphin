// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  return m_selected_attachment;
}

void Attachments::SetSelectedAttachment(u32 val)
{
  m_selected_attachment = val;
}

const std::vector<std::unique_ptr<EmulatedController>>& Attachments::GetAttachmentList() const
{
  return m_attachments;
}

}  // namespace ControllerEmu
