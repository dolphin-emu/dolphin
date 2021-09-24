// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Core/HW/WiimoteEmu/ExtensionPort.h"
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
  // This is here and not added to the list of numeric_settings because it's serialized differently,
  // by string (to be independent from the enum), and visualized differently in the UI.
  // For the rest, it's treated similarly to other numeric_settings in the group.
  NumericSetting<int> m_selection_setting = {
      &m_selection_value, {""}, 0, 0, WiimoteEmu::ExtensionNumber::MAX - 1};

  std::vector<std::unique_ptr<EmulatedController>> m_attachments;
};
}  // namespace ControllerEmu
