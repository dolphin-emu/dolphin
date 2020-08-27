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
#include "InputCommon/ControllerEmu/Setting/NumericSetting.h"

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

  NumericSetting<int>& GetSelectionSetting();

  const std::vector<std::unique_ptr<EmulatedController>>& GetAttachmentList() const;

private:
  SettingValue<int> m_selection_value;
  NumericSetting<int> m_selection_setting = {&m_selection_value, {""}, 0, 0, 0};

  std::vector<std::unique_ptr<EmulatedController>> m_attachments;
};
}  // namespace ControllerEmu
