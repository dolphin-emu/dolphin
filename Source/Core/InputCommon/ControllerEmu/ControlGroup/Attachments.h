// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "InputCommon/ControllerEmu/ControlGroup/ControlGroup.h"
#include "InputCommon/ControllerEmu/ControllerEmu.h"

namespace ControllerEmu
{
// A container of the selected and available attachments
// for configuration saving/loading purposes
class Attachments : public ControlGroup
{
public:
  explicit Attachments(const std::string& name);

  void AddAttachment(std::unique_ptr<EmulatedController> att);

  u32 GetSelectedAttachment() const;
  void SetSelectedAttachment(u32 val);

  const std::vector<std::unique_ptr<EmulatedController>>& GetAttachmentList() const;

private:
  std::vector<std::unique_ptr<EmulatedController>> m_attachments;

  std::atomic<u32> m_selected_attachment = {};
};
}  // namespace ControllerEmu
